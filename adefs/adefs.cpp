//=============================================================================
//	FILE:					adefs.cpp
//	SYSTEM:				Ade's Virtual File System
//	DESCRIPTION:	
//-----------------------------------------------------------------------------
//  COPYRIGHT:		(C) Copyright 2013-2017 Adrian Purser. All Rights Reserved.
//	LICENCE:			MIT - See LICENSE file for details
//	MAINTAINER:		Adrian Purser <ade@adrianpurser.co.uk>
//	CREATED:			22-JUL-2013 Adrian Purser <ade@adrianpurser.co.uk>
//=============================================================================

#include <iostream>
#include <cstring>
#include <functional>
#include "adefs.h"
#include "package_fs.h"

namespace adefs
{

//=============================================================================
//
//
//	MOUNTPOINT
//
//
//=============================================================================

MountPoint::MountPoint(const std::string & name,Attributes attr,MountPoint * p_parent)
	: m_p_parent(p_parent)
	, m_name(name)
	, m_attributes(attr)
{
}

MountPoint::~MountPoint(void)
{
}

void
MountPoint::reset()
{
	std::unique_lock<std::mutex> lock(m_mutex);

	m_children.clear();
	m_directories.clear();
	m_name.clear();
	m_attributes = 0;
	m_p_parent = nullptr;
}


MountPoint *					
MountPoint::get_mountpoint(const std::string & path,bool b_create)
{
	//-------------------------------------------------------------------------
	//	If the path is empty then this is the mountpoint that we are looking 
	//	for.
	//-------------------------------------------------------------------------
	if(path.empty())
		return this;
	
	//-------------------------------------------------------------------------
	//	Strip any leading '/'.
	//-------------------------------------------------------------------------
	if(path[0] == '/')
		return get_mountpoint(path.substr(1));
	
	//-------------------------------------------------------------------------
	//	Peel off the first directory name from the path.
	//-------------------------------------------------------------------------
	std::string sub_dir;
	std::string sub_path;
	std::string::size_type pos = path.find_first_of("/");

	if(pos == std::string::npos)
		sub_dir = path;
	else
	{
		sub_dir		= path.substr(0,pos);
		sub_path	= path.substr(pos+1);
	}

	std::transform(sub_dir.begin(),sub_dir.end(),sub_dir.begin(),::tolower);

	//-------------------------------------------------------------------------
	//	Find the child mountpoint.
	//-------------------------------------------------------------------------
	auto ifind = m_children.find(sub_dir);
	if(ifind != m_children.end())
		return ifind->second->get_mountpoint(sub_path);

	//-------------------------------------------------------------------------
	//	The mountpoint was not found so create it if b_create is true.
	//-------------------------------------------------------------------------
	MountPoint * p_mp = nullptr; 

	if(b_create)
	{
		try
		{
			mountpoint_shared_ptr p_new_mp = std::make_shared<MountPoint>(sub_dir,m_attributes,this);
			m_children[sub_dir] = p_new_mp;
			p_mp = p_new_mp->get_mountpoint(sub_path,b_create);
		}
		catch(...){}
	}

	return p_mp;
}


int								
MountPoint::mount(const std::string & path,directory_shared_ptr p_dir)
{
	bool err = false;

	//-------------------------------------------------------------------------
	//	If the path is empty then mount the directory in this mountpoint.
	//-------------------------------------------------------------------------
	if(path.empty())
	{
		//std::cout << "Adding directory to path '" << fullpath() << "'" << std::endl;
		m_directories.push_back(p_dir);
	}
	//-------------------------------------------------------------------------
	//	If the path starts with a '/' then strip it.
	//-------------------------------------------------------------------------
	else if(path[0] == '/')
		err = !!mount(path.substr(1),p_dir);
	else
	{
		//---------------------------------------------------------------------
		//	Peel off the first directory name from the path.
		//---------------------------------------------------------------------
		std::string sub_dir;
		std::string sub_path;
		std::string::size_type pos = path.find_first_of("/");

		if(pos == std::string::npos)
			sub_dir = path;
		else
		{
			sub_dir		= path.substr(0,pos);
			sub_path	= path.substr(pos+1);
		}

		std::transform(sub_dir.begin(),sub_dir.end(),sub_dir.begin(),::tolower);

		//---------------------------------------------------------------------
		//	Find/Create the child mountpoint.
		//---------------------------------------------------------------------
		mountpoint_shared_ptr p_mp;

		auto ifind = m_children.find(sub_dir);
		if(ifind != m_children.end())
			p_mp = ifind->second;
		else
		{
			//std::cout << "    Creating: " << fullpath() << "/" << sub_dir << std::endl;
			try
			{
				mountpoint_shared_ptr p_new_mp = std::make_shared<MountPoint>(sub_dir,m_attributes,this);
				m_children[sub_dir] = p_new_mp;
				p_mp = p_new_mp;
			}
			catch(...){err = true;}
		}

		//---------------------------------------------------------------------
		//	If the mountpoint was found/created then mount the directory into 
		//	it.
		//---------------------------------------------------------------------
		if(!err && p_mp)
			err = !!p_mp->mount(sub_path,p_dir);			
	}	

	return (err ? -1 : 0);
}


IDirectory *
MountPoint::find_file_owner(const std::string & path,Attributes required_attributes)
{

	if(path[0] == '/')
		return find_file_owner(path.substr(1));

	IDirectory * p_owner = nullptr;

	//-------------------------------------------------------------------------
	//	If the path does not contain a mountpoint name (ie. just the filename)
	//	then search the directories for the file.
	//-------------------------------------------------------------------------
	std::string::size_type pos = path.find_first_of("/");
	if(pos == std::string::npos)
	{
		if((m_attributes & required_attributes) == required_attributes)
		{
			std::string name(path.size(),0);
			std::transform(path.begin(),path.end(),name.begin(),::tolower);

			auto idb = m_directories.rbegin();
			auto ide = m_directories.rend();


			while((idb != ide) && !p_owner)
			{
				auto p_dir = (*idb++).lock();
		
				if(!p_dir || ((p_dir->dir_attr() & required_attributes)!=required_attributes))
					continue;

				if(p_dir->file_exists(name))
					p_owner = p_dir.get();
			}
		}
	}
	//-------------------------------------------------------------------------
	//	If the path does contain a mountpoint name then search for that mount
	//	point and search inside it for the file owner.
	//-------------------------------------------------------------------------
	else
	{
		std::string mpname(path.substr(0,pos));
		std::transform(mpname.begin(),mpname.end(),mpname.begin(),::tolower);

		auto ifind = m_children.find(mpname);
		if(ifind != m_children.end())
		{
			std::string mppath(path.substr(pos+1));
			std::transform(mppath.begin(),mppath.end(),mppath.begin(),::tolower);

			p_owner = ifind->second->find_file_owner(mppath);
		}
	}

	return p_owner;
}

size_t
MountPoint::load(	const std::string & filename,
					char *				p_buffer,
					size_t				buffer_size )
{
	size_t size = 0;

	auto p_file = openfile(filename);

	if(p_file)
		size = p_file->read(p_buffer,buffer_size);

	return size;
}

size_t
MountPoint::load(	const std::string &											filename,
									const std::function<void(	fileoffset		offset,
																						const char *	p_buffer,
																						size_t				buffer_size)>	&func,
																						char *				p_buffer,
																						size_t				buffer_size )
{
	size_t size = 0;

	auto p_file = openfile(filename);

	if(p_file)
	{
		while(!p_file->is_eof() && !p_file->is_fail())
		{
			ssize_t sizeread = p_file->read(p_buffer,buffer_size);
			if((sizeread != EOF) && (sizeread != 0))
			{
				func(static_cast<fileoffset>(size),p_buffer,sizeread);
				size		+= sizeread;
			}
		}
	}

	return size;
}

std::unique_ptr<IFile>
MountPoint::openfile(	const std::string & filename,
						std::uint32_t		mode)
{
	std::unique_ptr<IFile> p_file;

	//-------------------------------------------------------------------------
	//	Lock the mountpoint and search for the directory that contains the 
	//	specified file (search into the mountpoint tree).
	//-------------------------------------------------------------------------
	IDirectory * p_dir = nullptr;
	adefs::Attributes attr =	(mode & MODE_READ ? ATTR_READ : 0) |
								(mode & MODE_WRITE ? ATTR_WRITE : 0); 
							
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		p_dir = find_file_owner(filename,attr);

		//-------------------------------------------------------------------------
		//	If the directory was found then open the file.
		//-------------------------------------------------------------------------
		if(p_dir && ((p_dir->dir_attr() & attr) == attr))
		{
			std::string::size_type pos = filename.find_last_of("/");
			if(pos == std::string::npos)
				p_file = p_dir->openfile(filename,mode);
			else
				p_file = p_dir->openfile(filename.substr(pos+1),mode);
		}
	}

	return p_file;
}

void
MountPoint::write_tree(std::ostream & stream,std::string & prefix)
{
	stream << prefix << "+=[" << m_name << "]" << std::endl;

	for(auto & dir : m_directories)
	{
		if(!dir.expired())
		{
			auto pdir = dir.lock();
			auto files = pdir->file_list();

			for(auto & name : files)
			{
				stream << prefix << "| |  " << name << std::endl;
			}
		}
	}


	std::string subpfx(prefix + "| ");

	for(auto & mp_pair : m_children)
	{
		if(mp_pair.second)
			mp_pair.second->write_tree(stream,subpfx);
	}

	//auto ib = std::begin(m_children);
	//auto ie = std::end(m_children);

	//while(ib!=ie)
	//{
	//	auto p_mp = ib->second;
	//	++ib;
	//	
	//	std::string subpfx(prefix + (ib == ie ? "  " : "| "));
	//	p_mp->write_tree(stream,subpfx);
	//}

}


//=============================================================================
//
//
//	ADEFS - VIRTUAL FILE SYSTEM
//
//
//=============================================================================

AdeFS::AdeFS(void)
	: m_root("",ATTR_READ | ATTR_WRITE,nullptr)
{
}

AdeFS::~AdeFS(void)
{
}

void
AdeFS::reset()
{
	m_owned_packages.clear();
	m_package_factories_by_type.clear();
	m_package_factories.clear();
	m_root.reset();
}

int
AdeFS::mount(directory_shared_ptr p_dir,const std::string & mountpoint)
{
	return m_root.mount(mountpoint,p_dir);
}

int
AdeFS::mount(const std::string & package_name,const std::string & mountpoint)
{

	package_shared_ptr p_package = create_package(package_name);
	if(!p_package)
		return -1;

	auto p_mp = m_root.get_mountpoint(mountpoint);
	if(!p_mp)
		return -1;

	if(p_package->mount(p_mp))
		return -1;

	m_owned_packages.push_back(p_package);

	return 0;
}

size_t
AdeFS::load(const std::string & filename,
			char *				p_buffer,
			size_t				buffer_size )
{
	return m_root.load(filename,p_buffer,buffer_size);
}

size_t
AdeFS::load(const std::string &											filename,
						const std::function<void(	fileoffset		offset,
																			const char *	p_buffer,
																			size_t				buffer_size)>	& func,
						char *																	p_buffer,
						size_t																	buffer_size	)
{
	return m_root.load(filename,func,p_buffer,buffer_size);
}

std::vector<std::uint8_t>	
AdeFS::load( const std::string & filename )
{
	std::vector<std::uint8_t>	data;

	auto p_src = openfile(filename);
	if(p_src)
	{
		auto size = p_src->size();
		if(size > 0)
		{
			data.resize(size);
			auto readsize = p_src->read(reinterpret_cast<char *>(data.data()),size);
			if(readsize != size)
				data.clear();
		}
	}

	return data;
}


std::unique_ptr<IFile>
AdeFS::openfile(const std::string & filename,
				std::uint32_t		mode )
{
	return m_root.openfile(filename,mode);
}

package_factory_shared_ptr		
AdeFS::get_package_factory(const std::string & package_name)
{
	auto pos = package_name.find_last_of(".");
	if(pos != std::string::npos)
	{
		std::string ext = package_name.substr(pos+1);
		if(!ext.empty() && (ext.find_first_of("/\\*") == std::string::npos))
		{
			std::transform(ext.begin(),ext.end(),ext.begin(),::tolower);

			auto ifind = m_package_factories_by_type.find(ext);
			if(ifind != m_package_factories_by_type.end())
				return ifind->second;
		}
	}

	for(auto & p_factory : m_package_factories)
		if(p_factory->is_supported(package_name))
			return p_factory;

	return package_factory_shared_ptr();
}


package_shared_ptr 
AdeFS::create_package(const std::string & package_name)
{
	package_shared_ptr p_pkg;
	auto p_factory = get_package_factory(package_name);

	//-------------------------------------------------------------------------
	//	If a factory was found then use it to create a package.
	//-------------------------------------------------------------------------
	if(p_factory)
	{
		auto p_np = p_factory->create_package(package_name);
		if(p_np && !p_np->scan())
			p_pkg = p_np;
	}
	//-------------------------------------------------------------------------
	//	If the package type is unknown then attempt to create a FS package.
	//-------------------------------------------------------------------------
	else
	{
		try
		{
			auto p_np = std::make_shared<package_fs::PackageFS>(package_name);
			if(!p_np->scan())
				p_pkg = p_np;
		}
		catch(...){}
	}

	return p_pkg;
}

void
AdeFS::register_package_factory(package_factory_shared_ptr p_factory)
{
	if(p_factory)
	{
		m_package_factories.push_back(p_factory);

		auto types = p_factory->file_types();

		for(auto type : types)
		{
			std::transform(type.begin(),type.end(),type.begin(),::tolower);
			m_package_factories_by_type[type] = p_factory;
		}
	}
}

//=============================================================================
//
//
//	FILE - IN MEMORY
//
//
//=============================================================================


FileInMemory::FileInMemory(	std::uint32_t	mode,
							const char *	p_data,
							size_t			size )
	: m_mode(mode)
{
	if(p_data && size && !(mode & MODE_TRUNCATE))
	{
		m_data.assign(p_data,p_data+size);
		if(mode & (MODE_APPEND | MODE_AT_END))
			m_position = static_cast<filepos>(size);
	}
}

//-------------------------------------------------------------------------
//	INTERFACE FUNCTIONS
//-------------------------------------------------------------------------
int								
FileInMemory::get()
{
	int value {0};

	if(m_position >= m_data.size())
		m_count = 0;
	else
	{
		value = m_data.at(m_position);
		++m_position;
		m_count = 1;
	}

	return value;
}

size_t							
FileInMemory::read(char * p_buffer,size_t size)
{
	size_t sz {0};
	
	if(	p_buffer && 
		size &&
		(m_mode & MODE_READ) && 
		(m_position < m_data.size()) )
	{
		sz = std::min(size,m_data.size()-m_position);
		if(sz)
			std::memcpy(p_buffer,&m_data[m_position],sz);
	}

	m_count = sz;

	return sz;
}

void							
FileInMemory::write(const char * p_data,size_t size)
{
	if(	p_data && 
		size &&
		(m_mode & MODE_WRITE) )
	{
		if(m_position > m_data.size())
			m_position = static_cast<adefs::fileoffset>(m_data.size());
		if((m_position + size) > m_data.size())
			m_data.resize(m_position + size);
		std::memcpy(&m_data[m_position],p_data,size);
	}
}

void							
FileInMemory::ignore(size_t /*count*/,int /*delimeter*/)
{
}

void							
FileInMemory::seek(filepos pos)
{
	m_position = pos;
}

void							
FileInMemory::seek(fileoffset /*offset*/,adefs::Seek /*dir*/)
{

}

size_t							
FileInMemory::tell()
{
	return 0;
}

//-------------------------------------------------------------------------
//	BUFFER FUNCTIONS
//-------------------------------------------------------------------------
void							
FileInMemory::resize(size_t size)
{
	m_data.resize(size);
}



} // namespace adefs

