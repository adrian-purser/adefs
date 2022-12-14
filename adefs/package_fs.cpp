//=============================================================================
//	FILE:					package_fs.cpp
//	SYSTEM:				Ade's Virtual File System
//	DESCRIPTION:
//-----------------------------------------------------------------------------
//  COPYRIGHT:		(C) Copyright 2020 Adrian Purser. All Rights Reserved.
//	LICENCE:			MIT - See LICENSE file for details
//	MAINTAINER:		Adrian Purser <ade@adrianpurser.co.uk>
//	CREATED:			17-JUL-2013 Adrian Purser <ade@adrianpurser.co.uk>
//=============================================================================

#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include "package_fs.h"

#define _CRT_SECURE_NO_WARNINGS

namespace adefs
{

//*****************************************************************************
//
//
//
//	PACKAGE FILESYSTEM
//
//
//
//*****************************************************************************

namespace package_fs
{

//=============================================================================
//
//
//	PACKAGE FILESYSTEM - FILE CLASS
//
//
//=============================================================================
/*

FileFS::FileFS(	const std::string &		path,
				std::uint32_t			mode,
				DirectoryFS *			p_owner ) 
	: m_path(path)
	, m_mode(mode)
	, m_p_owner(p_owner)
	, m_stream(path,(mode & MODE_READ ?		std::ios_base::in	: 0) |
					(mode & MODE_WRITE ?	std::ios_base::out	: 0) |
					(mode & MODE_APPEND ?	std::ios_base::app	: 0) |
					(mode & MODE_AT_END ?	std::ios_base::ate	: 0) |
					(mode & MODE_TRUNCATE ?	std::ios_base::trunc: 0) |
											std::ios_base::binary )
{
	assert(m_p_owner);
}

FileFS::~FileFS(void)
{
	m_stream.close();
}

int								
FileFS::get()
{
	return m_stream.get();
}

size_t							
FileFS::read(char * p_buffer,size_t size)
{
	return static_cast<size_t>(m_stream.read(p_buffer,size).gcount());
}

void							
FileFS::write(const char * p_data,size_t size)
{
	if(m_mode & MODE_WRITE)
		m_stream.write(p_data,size);
}

void							
FileFS::ignore(size_t count,int delimeter)
{
	m_stream.ignore(count,delimeter);
}

void								
FileFS::seek(filepos pos)
{
	m_stream.seekg(pos);
}

void	
FileFS::seek(fileoffset offset,seek::Direction dir)	
{
	switch(dir)
	{
		case		adefs::seek::BEGINNING :	m_stream.seekg(offset,std::ios_base::beg);	break;
		case		adefs::seek::CURRENT :		m_stream.seekg(offset,std::ios_base::cur);	break;
		case		adefs::seek::END :			m_stream.seekg(offset,std::ios_base::end);	break;
		default :	break;
	}
}

size_t							
FileFS::tell()
{
	return static_cast<size_t>(m_stream.tellg());
}

bool							
FileFS::is_fail()	
{
	return m_stream.fail();
}

bool
FileFS::is_eof()
{
	return m_stream.eof();
}

size_t
FileFS::count()
{
	return static_cast<size_t>(m_stream.gcount());
}

*/
//=============================================================================
//
//
//	PACKAGE FILESYSTEM - DIRECTORY CLASS
//
//
//=============================================================================


DirectoryFS::DirectoryFS(const std::string & path,Attributes attr)
	: m_path(path)
	, m_attributes(attr)
{

	//-------------------------------------------------------------------------
	//	Fix the path.
	//-------------------------------------------------------------------------
	if(!m_path.empty())
	{
		std::replace(m_path.begin(),m_path.end(),'\\','/');
		if(m_path.back() != '/')
			m_path.push_back('/');
	}
}

DirectoryFS::~DirectoryFS()
{
}

// Get the size of the specified file in bytes.
size_t							
DirectoryFS::file_size(const std::string & filename)
{
	FileInfo fileinfo;
	size_t size = 0;
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if(!get_fileinfo(filename,fileinfo))
			size = fileinfo.status.st_size;
	}

	return size;
}
								
// Get the attributes of the specified file.
Attributes						
DirectoryFS::file_attr(const std::string & filename)
{
	FileInfo fileinfo;
	
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if(get_fileinfo(filename,fileinfo))
			fileinfo.attributes = 0;
	}

	return fileinfo.attributes;
}

// Test whether the specified file exists.
bool							
DirectoryFS::file_exists(const std::string & filename)
{
	//-------------------------------------------------------------------------
	//	Create a lower-case version of the filename.
	//-------------------------------------------------------------------------
	std::string name(filename.size(),0);
	std::transform(filename.begin(),filename.end(),name.begin(),::tolower);

	//-------------------------------------------------------------------------
	//	Test whether the file exists.
	//-------------------------------------------------------------------------
	std::unique_lock<std::mutex> lock(m_mutex);
	return !!m_files.count(name);
}

std::vector<std::string>		
DirectoryFS::file_list()
{
	std::vector<std::string> files;

	for(auto & info_pair : m_files)
		files.push_back(info_pair.first);

	return files;
}

std::unique_ptr<IFile>
DirectoryFS::openfile(	const std::string & filename,std::uint32_t mode)
{

	//-------------------------------------------------------------------------
	//	Get the files info.
	//-------------------------------------------------------------------------
	FileInfo info;

	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if(get_fileinfo(filename,info))
			return nullptr;
	}

	if(		((mode & (MODE_WRITE | MODE_APPEND)) && !(info.attributes & ATTR_WRITE)) 
		||	((mode & MODE_READ) && !(info.attributes & ATTR_READ)) )
		return nullptr;

	//-------------------------------------------------------------------------
	//	Create the file object.
	//-------------------------------------------------------------------------
	std::unique_ptr<IFile> p_file;
	try
	{
		std::unique_ptr<FileOnDisk> p_newfile(new FileOnDisk(m_path + filename,mode));
		if(!p_newfile->is_fail())
			p_file = std::move(p_newfile);
	}
	catch(...){}

	return p_file;
}


//-----------------------------------------------------------------------------
//
//	
//	NON-INTERFACE FUCTIONS
//
//
//-----------------------------------------------------------------------------

int	
DirectoryFS::get_fileinfo(const std::string & filename,FileInfo & out_fileinfo)
{
	//-------------------------------------------------------------------------
	//	Create a lower-case version of the filename.
	//-------------------------------------------------------------------------
	std::string name(filename.size(),0);
	std::transform(filename.begin(),filename.end(),name.begin(),::tolower);
	
	//-------------------------------------------------------------------------
	//	Search for the file in the file list. If the file is found then 
	//	rescan the file to update the status.
	//-------------------------------------------------------------------------
	int result = FILE_NOT_FOUND;

	auto ifind = m_files.find(name);
	if(ifind != m_files.end())
	{
		if(!rescan_file(ifind->second))
		{
			out_fileinfo = ifind->second;
			result = 0;
		}
	}

	return result;
}

int								
DirectoryFS::rescan_file(FileInfo & fileinfo)
{
	struct stat s;

	int result = stat((m_path+fileinfo.filename).c_str(),&s);
	if(result != 0)
		return (errno == ENOENT ? FILE_NOT_FOUND : -1);

	fileinfo.status = s;

	Attributes attr = 0;

	if((m_attributes & ATTR_WRITE) && (s.st_mode & S_IWRITE))		attr |= ATTR_WRITE;
	if((m_attributes & ATTR_READ) && (s.st_mode & S_IREAD))			attr |= ATTR_READ;
	if(s.st_mode & S_IFDIR)											attr |= ATTR_DIR;
	attr |= ATTR_RANDOM;

	fileinfo.attributes = attr;

	return 0;
}

int								
DirectoryFS::scan(std::vector<std::string> * p_out_directories)
{
	int								count = 0;
	FindFile						find;
	std::unique_lock<std::mutex>	lock(m_mutex);

	m_files.clear();

	if(find.find(m_path))
	{
		bool	found;
		bool	err = false;

		do
		{
			found = find.findnext();
			if(found && !find.isdots())
			{
				std::string	filename(find.filename());

				if(!filename.empty())
				{
					if(find.isdirectory())
					{
						if(		p_out_directories
							&&	(filename != "CVS")
							&&	(filename != ".git") )
						p_out_directories->push_back(filename);
					}
					else
					{
						//-----------------------------------------------------
						//	Create a lower-case version of the filename.
						//-----------------------------------------------------
						std::string name(filename.size(),0);
						std::transform(filename.begin(),filename.end(),name.begin(),::tolower);

						//-----------------------------------------------------
						//	Scan the file.
						//-----------------------------------------------------
						FileInfo info;
						info.filename = filename;

						if(!rescan_file(info))
						{
							m_files[name] = info;
							if(m_b_logging)
							{
								std::cout << "SCAN: " << std::setw(32) << std::left << name;
								std::cout << std::setw(9) << info.status.st_size << " ";
								std::cout << (info.attributes & ATTR_READ ? 'R' : '-');
								std::cout << (info.attributes & ATTR_WRITE ? 'W' : '-');
								std::cout << " (" << filename << ")" << std::endl;
							}
							++count;
						}
					}
				}
			}

		} while(found && !err);

		find.close();
	}
	return count;
}



//=============================================================================
//
//
//	PACKAGE FILESYSTEM - PACKAGE CLASS
//
//
//=============================================================================



PackageFS::PackageFS(	const std::string &		path,
						Attributes				attributes )
	: m_path(path)
	, m_attributes(attributes)
{
	//-------------------------------------------------------------------------
	// Fix the path.
	//-------------------------------------------------------------------------
	std::replace(m_path.begin(),m_path.end(),'\\','/');

	if(m_path.empty() || (m_path.back()!='/'))
		m_path.push_back('/');
}

PackageFS::~PackageFS(void)
{
}

int								
PackageFS::mount(MountPoint * p_mountpoint)
{
	if(!p_mountpoint)
		return -1;

	size_t base_size = m_path.size();
	bool err = false;

	for(auto & p_dir : m_directories)
	{
		DirectoryFS & dir = *static_cast<DirectoryFS *>(p_dir.get());
		err |= !!p_mountpoint->mount(dir.get_path().substr(base_size),p_dir);
	}

	return (int)err;
}

int								
PackageFS::scan(const std::string & path)
{
	//-------------------------------------------------------------------------
	//	Create a directory object.
	//-------------------------------------------------------------------------
	directory_shared_ptr p_dir;

	try
	{
		auto p_newdir = std::make_shared<DirectoryFS>(path,attributes());
		p_dir = p_newdir;
	}
	catch(...){return -1;}

	DirectoryFS & dir = *static_cast<DirectoryFS *>(p_dir.get());

	//-------------------------------------------------------------------------
	//	Scan the directory contents.
	//-------------------------------------------------------------------------
	std::vector<std::string> sub_dirs;
	if(dir.scan(&sub_dirs))
		m_directories.push_back(p_dir);

	//-------------------------------------------------------------------------
	//	If sub-directories were found then scan them also.
	//-------------------------------------------------------------------------
	for(auto & subdir : sub_dirs)
		scan(path+subdir+"/");

	return 0;
}


} // namespace package_fs
} // namespace adefs

