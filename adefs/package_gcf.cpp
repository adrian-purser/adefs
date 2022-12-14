//=============================================================================
//	FILE:					package_gcf.cpp
//	SYSTEM:				Ade's Virtual File System
//	DESCRIPTION:
//-----------------------------------------------------------------------------
//  COPYRIGHT:		(C) Copyright 2020 Adrian Purser. All Rights Reserved.
//	LICENCE:			MIT - See LICENSE file for details
//	MAINTAINER:		Adrian Purser <ade@adrianpurser.co.uk>
//	CREATED:			28-AUG-2013 Adrian Purser <ade@adrianpurser.co.uk>
//=============================================================================
#include "package_gcf.h"

namespace adefs { namespace package_gcf
{

//=============================================================================
//
//
//	PACKAGE GCF - DIRECTORY CLASS
//
//
//=============================================================================

DirectoryGCF::DirectoryGCF(PackageGCF * p_package)
	: m_p_package(p_package)
{
	assert(m_p_package);
}

DirectoryGCF::~DirectoryGCF()
{
}

//-----------------------------------------------------------------------------
//
//	
//	NON-INTERFACE FUCTIONS
//
//
//-----------------------------------------------------------------------------

int	
DirectoryGCF::get_fileinfo(const std::string & filename,FileInfo & out_fileinfo)
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
		out_fileinfo = ifind->second;
		result = 0;
	}

	return result;
}

//-------------------------------------------------------------------------
//	INTERFACE FUNCTIONS
//-------------------------------------------------------------------------

// Get the size of the specified file in bytes.
size_t							
DirectoryGCF::file_size(const std::string & filename)
{
	FileInfo fileinfo;
	size_t size = 0;
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if(!get_fileinfo(filename,fileinfo))
			size = fileinfo.size;
	}

	return size;
}

// Get the attributes of the specified file.
Attributes						
DirectoryGCF::file_attr(const std::string & filename)
{
	return file_exists(filename) ? ATTR_READ : 0;
}

// Test whether the specified file exists.
bool							
DirectoryGCF::file_exists(const std::string & filename)
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
DirectoryGCF::file_list()
{
	std::vector<std::string> files;

	for(auto & info_pair : m_files)
		files.push_back(info_pair.first);

	return files;
}

std::unique_ptr<IFile>					
DirectoryGCF::openfile(	const std::string & filename,
						std::uint32_t		mode )
{
	//-------------------------------------------------------------------------
	//	Writing is not possible so abort if a writable mode is requested.
	//	Abort if the read mode is not requested.
	//-------------------------------------------------------------------------
	if((mode & (MODE_WRITE | MODE_APPEND)) || !(mode & MODE_READ))
		return nullptr;

	//-------------------------------------------------------------------------
	//	Get the files info.
	//-------------------------------------------------------------------------
	FileInfo info;

	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if(get_fileinfo(filename,info))
			return nullptr;
	}

	//-------------------------------------------------------------------------
	//	Create the file object.
	//-------------------------------------------------------------------------
	std::unique_ptr<IFile> p_file;

	std::unique_ptr<FileGCF> p_new_file(new FileGCF(info.file_id,mode,m_p_package));
	if(!p_new_file->is_fail())
		p_file = std::move(p_new_file);

	return p_file;
}

void							
DirectoryGCF::add_file(	const std::string & filename,
						std::uint32_t		index,
						std::uint32_t		size,
						std::uint32_t		id)
{
	FileInfo	info;

	info.filename		= filename;
	info.index			= index;
	info.size			= size;
	info.file_id		= id;

	std::string namelower(filename);
	std::transform(namelower.begin(),namelower.end(),namelower.begin(),::tolower);

	m_files[namelower] = info;
}

//=============================================================================
//
//
//	PACKAGE GCF - PACKAGE CLASS
//
//
//=============================================================================

PackageGCF::PackageGCF(	const std::string & filename )
	: m_filename(filename)
{
	//-------------------------------------------------------------------------
	// Fix the path.
	//-------------------------------------------------------------------------
	std::replace(m_filename.begin(),m_filename.end(),'\\','/');
}

PackageGCF::~PackageGCF(void)
{
}

int
PackageGCF::mount(MountPoint * p_mountpoint)
{
	if(!p_mountpoint)
		return -1;

	return mount_directory(p_mountpoint,"",m_root_directory);
}

int
PackageGCF::mount_directory(MountPoint *			p_mountpoint,
							const std::string &		path,
							DirectoryNode &			dir_node )
{
	//-------------------------------------------------------------------------
	//	Mount the directory.
	//-------------------------------------------------------------------------
	if(!dir_node.p_directory)
		return 0;

	if(p_mountpoint->mount(path,dir_node.p_directory))
		return -1;

	//-------------------------------------------------------------------------
	//	Fix the path.
	//-------------------------------------------------------------------------
	std::string basepath(path);
	if(!basepath.empty())
		basepath.push_back('/');

	//-------------------------------------------------------------------------
	//	Mount the sub-directories.
	//-------------------------------------------------------------------------
	bool err = false;

	for(auto & dir_pair : dir_node.sub_directories)
	{
		if(mount_directory(p_mountpoint,basepath+dir_pair.first,dir_pair.second))
		{
			err = true;
			break;
		}
	}

	return (int)err;
}

int
PackageGCF::scan()
{
	std::ifstream	gcf_file;

	//-------------------------------------------------------------------------
	//	Open the package file.
	//-------------------------------------------------------------------------
	gcf_file.open(m_filename.c_str(),std::ios_base::in | std::ios_base::binary | std::ios_base::ate);
	if(gcf_file.fail())
	{
		//std::clog << "PACKAGEGCF: Package file '" << m_filename << "' not found!\n";
		return 0;
	}

	//-------------------------------------------------------------------------
	//	Get the length of the file. Make sure that it's at least as big as the 
	//	file header.
	//-------------------------------------------------------------------------
	gcf_file.seekg(0, std::ios::end);
	size_t filesize = (size_t)gcf_file.tellg();
	if(filesize < sizeof(GCFHeader))
	{
		//std::clog << "PACKAGEGCF: Package file '" << m_filename << "' is too small!\n";
		return 0;
	}
	gcf_file.seekg(0, std::ios::beg);

	//-------------------------------------------------------------------------
	//	Read and validate the file header.
	//-------------------------------------------------------------------------
	
	gcf_file.read((char *)&m_gcf_header,sizeof(GCFHeader));
	if(m_gcf_header.FileSize != filesize)
	{
		//std::clog << "PACKAGEGCF: Header Filesize for GCF file '" << m_filename << "' is incorrect!\n";
		return 0;
	}

	//-------------------------------------------------------------------------
	//	Read the block entry header.
	//-------------------------------------------------------------------------
	GCFBlockEntryHeader block_entry_header;
	gcf_file.read((char *)&block_entry_header,sizeof(GCFBlockEntryHeader));

	size_t pos = (size_t)gcf_file.tellg();
	gcf_file.seekg(pos+(sizeof(GCFBlockEntry)*block_entry_header.BlockCount), std::ios::beg);

	if(gcf_file.fail())
		return 0;

	//-------------------------------------------------------------------------
	//	Read the Frag Map 
	//-------------------------------------------------------------------------
	GCFFragMapHeader frag_map_header;
	gcf_file.read((char *)&frag_map_header,sizeof(GCFFragMapHeader));

	pos = (size_t)gcf_file.tellg();
	m_fragmap_file_offset	= (std::uint32_t)pos;
	m_frag_map.resize(frag_map_header.BlockCount);

	gcf_file.read((char *)&m_frag_map[0],sizeof(std::uint32_t)*frag_map_header.BlockCount);

	//-------------------------------------------------------------------------
	//	Read the Block Entry Map Header
	//-------------------------------------------------------------------------
	if(m_gcf_header.FormatVersion <= 5)
	{
		GCFBlockEntryMapHeader block_entry_map_header;
		gcf_file.read((char *)&block_entry_map_header,sizeof(GCFBlockEntryMapHeader));
	
		pos = (size_t)gcf_file.tellg();
		gcf_file.seekg(pos+(std::int32_t)(sizeof(GCFBlockEntryMap)*block_entry_map_header.BlockCount), std::ios::beg);

		if(gcf_file.fail())
			return 0;
	}

	//-------------------------------------------------------------------------
	//	Read the Directory Header
	//-------------------------------------------------------------------------
	GCFDirectoryHeader	dir_header;
	size_t				dirpos = (size_t)gcf_file.tellg();

	gcf_file.read((char *)&dir_header,sizeof(GCFDirectoryHeader));
	
	//-------------------------------------------------------------------------
	//	Read the directory info.
	//-------------------------------------------------------------------------
	gcf_file.seekg(dirpos+dir_header.DirectorySize,std::ios::beg);
	GCFDirectoryMapHeader dir_map_header;
	gcf_file.read((char *)&dir_map_header,sizeof(GCFDirectoryMapHeader));

	DirectoryInfo	dirinfo;

	dirinfo.dir_map.resize(dir_header.ItemCount);
	dirinfo.raw_dir_block.resize(dir_header.DirectorySize);

	gcf_file.read((char *)&dirinfo.dir_map[0],dir_header.ItemCount*sizeof(std::uint32_t));
	gcf_file.seekg(dirpos,std::ios::beg);
	gcf_file.read(&dirinfo.raw_dir_block[0],dir_header.DirectorySize);

	dirinfo.p_header			=	(GCFDirectoryHeader *)&dirinfo.raw_dir_block[0];
	dirinfo.p_entries			=	(GCFDirectoryEntry *)&dirinfo.p_header[1];
	dirinfo.p_names				=	((char *)&dirinfo.raw_dir_block[0]) +
									sizeof(GCFDirectoryHeader) +
									(sizeof(GCFDirectoryEntry)*dir_header.ItemCount);
	dirinfo.dir_entries_offset	=	((std::uint32_t)dirpos)+sizeof(GCFDirectoryHeader);

	//-------------------------------------------------------------------------
	//	Find the data blocks.
	//-------------------------------------------------------------------------
	GCFChecksumHeader		chksum_header;
	GCFChecksumMapHeader	chksum_map_header;
	size_t					chksum_pos = dirpos + (dir_header.DirectorySize+sizeof(GCFDirectoryMapHeader)+(dir_header.ItemCount*sizeof(std::uint32_t)));	// Offset of the checksum data.

	gcf_file.seekg(chksum_pos,std::ios::beg);
	gcf_file.read((char *)&chksum_header,sizeof(GCFChecksumHeader));
	gcf_file.read((char *)&chksum_map_header,sizeof(GCFChecksumMapHeader));

	size_t data_pos = chksum_pos+(chksum_header.ChecksumSize + sizeof(GCFChecksumHeader));

	//-------------------------------------------------------------------------
	//	Read the data block header.
	//-------------------------------------------------------------------------
	gcf_file.seekg(data_pos,std::ios::beg);
	gcf_file.read((char *)&m_gcf_data_block_header,sizeof(GCFDataBlockHeader));

	//-------------------------------------------------------------------------
	//	Scan the directory info and add the files to the package directory.
	//-------------------------------------------------------------------------
	std::uint32_t entry_index = 0;

	if(!dirinfo.p_entries->DirectoryType)
		entry_index = dirinfo.p_entries->FirstIndex;

	scan_directory(dirinfo,entry_index,m_root_directory);

	//-------------------------------------------------------------------------
	//	Diagnostic Messages.
	//-------------------------------------------------------------------------
	//std::cout << "PACKAGEGCF: First Block Offset : " << m_gcf_data_block_header.FirstBlockOffset << std::endl;
	//std::cout << "PACKAGEGCF: Block Size :         " << m_gcf_header.BlockSize << std::endl;
	//std::cout << "PACKAGEGCF: Block Count :        " << m_gcf_header.BlockCount << std::endl;

	return 0;
}

std::uint32_t					
PackageGCF::add_file(std::uint32_t size,std::uint32_t block_index)
{
	std::uint32_t	id = static_cast<std::uint32_t>(m_file_info.size());

	FileInfo info;
	info.file_size			= size;
	info.data_block_index	= block_index;
	m_file_info.push_back(info);

	return id;
}

void
PackageGCF::scan_directory(	DirectoryInfo &			dirinfo,
							std::uint32_t			entry_index,
							DirectoryNode &			dir_node )
{
	auto p_dir = std::make_shared<DirectoryGCF>(this);
	bool err = false;

	dir_node.p_directory.reset();
	dir_node.sub_directories.clear();

	while(entry_index) 
	{
		GCFDirectoryEntry & entry = dirinfo.p_entries[entry_index];

		std::string filename(dirinfo.p_names+entry.NameOffset);
	
		if(entry.DirectoryType)
		{
			const std::uint32_t block_index = dirinfo.dir_map.at(entry_index);
			auto id = add_file(entry.ItemSize,block_index);
			p_dir->add_file(filename,block_index,entry.ItemSize,id);
		}
		else
		{
			DirectoryNode sub_dir;
			
			std::transform(filename.begin(),filename.end(),filename.begin(),::tolower);

			scan_directory(dirinfo,entry.FirstIndex,sub_dir);
			dir_node.sub_directories[filename] = sub_dir;
		}

		entry_index = entry.NextIndex;
	}

	if(!err)
		dir_node.p_directory = p_dir;
}

bool							
PackageGCF::get_file_info(	std::uint32_t		file_id,
							std::uint32_t &		out_block_index,
							std::uint32_t &		out_file_size )
{
	if(file_id >= m_file_info.size())
		return false;

	auto & info = m_file_info[file_id];

	out_block_index	= info.data_block_index;
	out_file_size	= info.file_size;

	return true;
}

std::uint32_t
PackageGCF::get_block_index(std::uint32_t first_block,fileoffset offset)
{
	std::uint32_t block_offset = offset/m_gcf_header.BlockSize;

	while(block_offset--)
		first_block = m_frag_map[first_block];

	return first_block;
}

//=============================================================================
//
//
//	PACKAGE GCF - FILE CLASS
//
//
//=============================================================================

FileGCF::FileGCF(	std::uint32_t	id,
					std::uint32_t	mode,
					PackageGCF *	p_package )
	: m_mode(mode)
	, m_p_package(p_package)
	, m_id(id)
	, m_first_data_block_offset(0)
	, m_file_pointer(0)
	, m_block_num(0)
	, m_block_offset(0)
	, m_block_data_avail(0)
	, m_gcount(0)
	, m_b_failbit(false)
	, m_block_size(0)
{
	assert(m_p_package);
	if(m_p_package->get_file_info(m_id,m_block_index,m_size))
	{	
		m_file_pointer	= (mode & MODE_AT_END ? m_size : 0);
		m_block_size	= m_p_package->get_blocksize();

		//---------------------------------------------------------------------
		//	Open the GCF package file. 
		//---------------------------------------------------------------------
		std::string package_name(m_p_package->get_filename());
		m_stream.open(package_name.c_str(),std::ios_base::in | std::ios_base::binary | std::ios_base::ate);
		if(m_stream.fail())
		{
			//std::clog << "PACKAGEGCF:FILEGCF: Package file '" << package_name << "' not found!\n";
			m_b_failbit = true;
		}
		else
		{
			//-----------------------------------------------------------------
			//	Read the Block Entry
			//-----------------------------------------------------------------
			GCFBlockEntry block_entry;
			m_stream.seekg(sizeof(GCFHeader)+sizeof(GCFBlockEntryHeader)+(sizeof(GCFBlockEntry)*m_block_index), std::ios::beg);
			m_stream.read((char *)&block_entry,sizeof(GCFBlockEntry));
			m_first_data_block_index	= block_entry.FirstDataBlockIndex;
			m_first_data_block_offset	= m_p_package->get_first_block_offset();
			update_block_info();
		}
	}
	else
		m_b_failbit = true;
}

FileGCF::~FileGCF(void)
{
}

int
FileGCF::get()
{
	if(is_eof())
		return EOF;

	const fileoffset fileofs	= m_first_data_block_offset + (m_block_num * m_block_size) + m_block_offset;	// The offset of the data in the package file.

	m_stream.seekg(fileofs,std::ios::beg);

	++m_file_pointer;
	
	if(m_block_data_avail > 1)
	{
		--m_block_data_avail;
		++m_block_offset;
	}
	else
	{
		m_block_num			= m_p_package->get_next_block(m_block_num);
		m_block_data_avail	= (m_block_num >= m_p_package->get_blockcount() ? 0 : m_block_size);
		m_block_offset		= 0;
	}

	return m_stream.get();
}

size_t
FileGCF::read(char * p_buffer,size_t size)
{
	if(is_eof() || is_fail())
		return 0;

	size_t				totalread = 0;
	const std::uint32_t	available = m_size - m_file_pointer;

	if(size > available)
		size = available;

	while(size)
	{
		size_t				readsize	= std::min(m_block_data_avail,size);											// The amount of data to read from the block.
		const fileoffset	fileofs		= m_first_data_block_offset + (m_block_num * m_block_size) + m_block_offset;	// The offset of the data in the package file.

		if(!readsize)
			break;

		m_stream.seekg(fileofs,std::ios::beg);
		readsize = (size_t)m_stream.read(p_buffer,readsize).gcount();

		p_buffer			+= readsize;
		totalread			+= readsize;
		m_file_pointer		+= static_cast<std::uint32_t>(readsize);
		size				-= readsize;
	
		m_block_data_avail	-= readsize;
		m_block_offset		+= static_cast<std::uint32_t>(readsize);

		if(m_block_data_avail == 0)
		{
			m_block_num			= m_p_package->get_next_block(m_block_num);
			m_block_data_avail	= (m_block_num >= m_p_package->get_blockcount() ? 0 : m_block_size);
			m_block_offset		= 0;
		}
	}

	return totalread;
}

void
FileGCF::write(const char * /*p_data*/,size_t /*size*/)
{
	// This package is read only so don't do anything here.
}

void
FileGCF::ignore(size_t count,int delimeter)
{
	// TODO : Improve this function for performance.

	if(delimeter < 0)
		seek(static_cast<adefs::fileoffset>(count),adefs::Seek::CURRENT);
	else
	{
		while(count-- && !is_eof() && !is_fail())
			if(get() == delimeter)
				break;
	}
}

void
FileGCF::seek(fileoffset offset,adefs::Seek dir)
{
	switch(dir)
	{
		//---------------------------------------------------------------------
		case adefs::Seek::BEGINNING :		
		//---------------------------------------------------------------------
			m_file_pointer = std::min<std::uint32_t>(offset,m_size); 
			break;

		//---------------------------------------------------------------------
		case adefs::Seek::CURRENT :	
		//---------------------------------------------------------------------
			{
				std::int32_t ofs = ((std::int32_t)m_file_pointer)+offset;
				m_file_pointer = (ofs < 0 ? 0 : ((std::uint32_t)ofs > m_size ? m_size : ofs));
			}
			break;

		//---------------------------------------------------------------------
		case adefs::Seek::END :
		//---------------------------------------------------------------------
			m_file_pointer = m_size;
			break;

		default :
			break;
	}

	update_block_info();
}

void 
FileGCF::update_block_info()
{
	m_block_num			= m_p_package->get_block_index(m_first_data_block_index,m_file_pointer);
	m_block_offset		= m_file_pointer % m_block_size;
	m_block_data_avail	= m_block_size - m_block_offset;
}


//=============================================================================
//
//
//	PACKAGE GCF - FACTORY CLASS
//
//
//=============================================================================

bool							
PackageFactoryGCF::is_supported(const std::string & path)
{
	auto pos = path.find_last_of(".");

	if(pos == std::string::npos)
		return false;

	std::string ext(path.substr(pos));
	std::transform(ext.begin(),ext.end(),ext.begin(),::tolower);
	return !!(ext == "gcf");
}

package_shared_ptr				
PackageFactoryGCF::create_package(const std::string & path)
{
	return std::make_shared<PackageGCF>(path);
}




}} // namespace package_gcf, adefs

