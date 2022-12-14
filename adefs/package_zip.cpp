//=============================================================================
//	FILE:					package_zip.h
//	SYSTEM:				Ade's Virtual File System
//	DESCRIPTION:
//-----------------------------------------------------------------------------
//  COPYRIGHT:		(C) Copyright 2020 Adrian Purser. All Rights Reserved.
//	LICENCE:			MIT - See LICENSE file for details
//	MAINTAINER:		Adrian Purser <ade@adrianpurser.co.uk>
//	CREATED:			01-OCT-2013 Adrian Purser <ade@adrianpurser.co.uk>
//=============================================================================

#include <exception>
#include <cstring>
#include "package_zip.h"
//#include "debug.h"
//#include "inflate.h"

//#define PKGZIP_INFLATE_ZLIB
//#define HAVE_ZLIB


#ifdef HAVE_ZLIB
	extern "C"
	{
		#include <zlib.h>
	}
	#pragma comment(lib, "zlib.lib")
#endif


namespace adefs { namespace package_zip
{

//=============================================================================
//
//
//	PACKAGE ZIP - FILE CLASS
//
//
//=============================================================================

FileZIPStore::FileZIPStore(void)
	: m_count(0)
{
}

int
FileZIPStore::open(	const std::string &	zip_filename,
					const FileInfo &	fileinfo,
					std::uint32_t		mode  )
{
	if(		is_open()
		||	(fileinfo.compression_method != ZIP_UNCOMPRESSED) )
		return -1;

	m_zip_file.open(zip_filename.c_str(),std::ios_base::in | std::ios_base::binary);
	if(!m_zip_file.is_open())
		return -1;
	
	seek(0,(mode & MODE_AT_END) ? adefs::Seek::END : adefs::Seek::BEGINNING);
	m_fileinfo = fileinfo;

	return 0;
}

FileZIPStore::~FileZIPStore(void)
{
	m_zip_file.close();
}

int
FileZIPStore::get()
{
	return (is_eof() || is_fail() ? EOF : m_zip_file.get());
}

size_t
FileZIPStore::read(char * p_buffer,size_t size)
{
	if(is_fail() || is_eof())
		return 0;

	auto pos = (std::int32_t)m_zip_file.tellg();

	if(pos < m_fileinfo.file_offset)
		return 0;

	const size_t avail = m_fileinfo.size_uncompressed - (pos - m_fileinfo.file_offset);

	if(size > avail)
		size = avail;

	return static_cast<size_t>(m_zip_file.read(p_buffer,size).gcount());
}

void
FileZIPStore::write(const char * /*p_data*/,size_t /*size*/)
{
}

void
FileZIPStore::ignore(size_t count,int delimeter)
{
	m_zip_file.ignore(count,delimeter);
}

void
FileZIPStore::seek(filepos pos)
{
	m_zip_file.seekg(m_fileinfo.file_offset + pos,m_zip_file.beg);
}

void
FileZIPStore::seek(fileoffset offset,adefs::Seek dir)
{
	switch(dir)
	{
		//---------------------------------------------------------------------
		case adefs::Seek::BEGINNING :	
		//---------------------------------------------------------------------
			m_zip_file.seekg(m_fileinfo.file_offset + offset,m_zip_file.beg); 
			break;

		//---------------------------------------------------------------------
		case adefs::Seek::CURRENT :	
		//---------------------------------------------------------------------
			{
				fileoffset pos = static_cast<fileoffset>(tell()) + offset;
				if(pos <0) 
					pos = 0;
				else if(pos > m_fileinfo.size_uncompressed)
					pos = m_fileinfo.size_uncompressed;

				m_zip_file.seekg(m_fileinfo.file_offset + pos,m_zip_file.beg);
			}
			break;

		//---------------------------------------------------------------------
		case adefs::Seek::END :	
		//---------------------------------------------------------------------
			{
				fileoffset pos = m_fileinfo.size_uncompressed - offset;
				if(pos <0) 
					pos = 0;
				else if(pos > m_fileinfo.size_uncompressed)
					pos = m_fileinfo.size_uncompressed;

				m_zip_file.seekg(m_fileinfo.file_offset + pos,m_zip_file.beg);
			}
			break;

		//---------------------------------------------------------------------
		default : 
		//---------------------------------------------------------------------
			break;
	}
}


//=============================================================================
//
//
//	PACKAGE ZIP - DIRECTORY CLASS
//
//
//=============================================================================

DirectoryZIP::DirectoryZIP(class PackageZIP * p_package)
	: m_p_package(p_package)
{
}

DirectoryZIP::~DirectoryZIP()
{
}


//-------------------------------------------------------------------------
//	NON-INTERFACE FUNCTIONS
//-------------------------------------------------------------------------
void							
DirectoryZIP::add_file(	const std::string & filename,
						std::int32_t		id )
{
	if(!filename.empty() && (id>=0))
	{
		std::string name(filename.size(),0);
		std::transform(filename.begin(),filename.end(),name.begin(),::tolower);

		m_files[name] = id;
	}
}

int
DirectoryZIP::get_file_id(const std::string & filename)
{
	std::string name(filename.size(),0);
	std::transform(filename.begin(),filename.end(),name.begin(),::tolower);
	auto ifind = m_files.find(name);
	if(ifind != m_files.end())
		return ifind->second;
	return -1;
}

//-------------------------------------------------------------------------
//	INTERFACE FUNCTIONS
//-------------------------------------------------------------------------

// Get the size of the specified file in bytes.
size_t							
DirectoryZIP::file_size(const std::string & filename)
{
	std::unique_lock<std::mutex> lock(m_mutex);
	auto id = get_file_id(filename);
	if(id>=0)
		return m_p_package->get_filesize(id);

	return 0;
}

// Get the attributes of the specified file.
Attributes						
DirectoryZIP::file_attr(const std::string & filename)
{
	return file_exists(filename) ? ATTR_READ : 0;
}


// Test whether the specified file exists.
bool							
DirectoryZIP::file_exists(const std::string & filename)
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
DirectoryZIP::file_list()
{
	std::vector<std::string>	files;

	for(auto & info_pair : m_files)
		files.push_back(info_pair.first);

	return files;
}


std::unique_ptr<IFile>					
DirectoryZIP::openfile(	const std::string & filename,
						std::uint32_t		mode )
{
	//-------------------------------------------------------------------------
	//	Writing is not possible so abort if a writable mode is requested.
	//	Abort if the read mode is not requested.
	//-------------------------------------------------------------------------
	if((mode & (MODE_WRITE | MODE_APPEND)) || !(mode & MODE_READ))
		return nullptr;

	//-------------------------------------------------------------------------
	//	Get the files id.
	//-------------------------------------------------------------------------
	std::unique_lock<std::mutex> lock(m_mutex);
	std::int32_t file_id = get_file_id(filename);
	if(file_id < 0)
		return nullptr;

	//-------------------------------------------------------------------------
	//	Create the file object.
	//-------------------------------------------------------------------------
	return m_p_package->openfile(file_id,mode);
}


//=============================================================================
//
//
//	PACKAGE ZIP - PACKAGE CLASS
//
//
//=============================================================================

//std::string						m_filename;			// The name of the ZIP file.
//std::mutex						m_mutex;			// Mutex for exclusive access.
//std::vector<FileInfo>			m_file_info;

PackageZIP::PackageZIP(const std::string & filename)
	: m_filename(filename)
{
}

PackageZIP::~PackageZIP(void)
{
}


	//-------------------------------------------------------------------------
	//	PACKAGE INTERFACE FUNCTIONS
	//-------------------------------------------------------------------------
int
PackageZIP::mount(MountPoint * p_mountpoint)
{
	if(!p_mountpoint)
		return -1;

	return mount_directory(p_mountpoint,"",m_root_directory);
}

int
PackageZIP::mount_directory(MountPoint *			p_mountpoint,
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
PackageZIP::scan()
{
	int count = 0;

	//-------------------------------------------------------------------------
	//	Reset the current contents.
	//-------------------------------------------------------------------------
	m_root_directory.sub_directories.clear();
	m_root_directory.p_directory = std::make_shared<DirectoryZIP>(this);

	//-------------------------------------------------------------------------
	//	Open the package file.
	//-------------------------------------------------------------------------
	std::ifstream zip_file(m_filename,std::ios_base::in | std::ios_base::binary | std::ios_base::ate);

	if(zip_file.fail())
		return count;

	//-------------------------------------------------------------------------
	//	Get the length of the file. If it is smaller than the central directory 
	//	then abort.
	//-------------------------------------------------------------------------
	zip_file.seekg(0, std::ios::end);
	auto filesize = static_cast<size_t>(zip_file.tellg());

	if(filesize < sizeof_central_dir)
	{
		std::clog << "PACKAGEZIP: Error! Package file '" << m_filename << "' is corrupt!\n";
		return 0;
	}

	//-------------------------------------------------------------------------
	//	Load the central directory header.
	//-------------------------------------------------------------------------
	int								buffsize = std::min<int>(filesize,0x0FFFF + sizeof_central_dir + 4);
	std::vector<char>	buffer(buffsize+4);
	zip_central_dir		central_dir;

	std::memset(&central_dir,0,sizeof(central_dir));

	zip_file.seekg(-(std::fstream::off_type)buffsize,std::ios::end);
	zip_file.read(&buffer[0],buffsize);

	char *	pdata = &buffer[0] + (buffsize-sizeof_central_dir);
	bool		found = false;

	for(int i=buffsize-static_cast<int>(sizeof_central_dir);(i>=0) && !found;--i,--pdata)
	{
		if(	(pdata[0]==0x50) && 
				(pdata[1]==0x4B) && 
				(pdata[2]==0x05) && 
				(pdata[3]==0x06) )
		{
			pdata += 4;
			std::memcpy(&central_dir,pdata,sizeof_central_dir);
			found = true;
		}
	}

	if(!found)
	{
		std::clog << "PACKAGEZIP: Invalid package file '" << m_filename << "'\n";
		
		pdata = &buffer[0] + (buffsize-sizeof_central_dir);
		
		std::clog << "PACKAGEZIP: Buffersize = " << buffsize << "\n";
		std::clog << "PACKAGEZIP: Filesize   = " << filesize << "\n";
		std::clog << "PACKAGEZIP: sizeof central directory = " << sizeof_central_dir << "\n";
		//std::clog << "\nCENTRAL DIRECTORY:\n";
		//std::clog << debug::hexdump(pdata,sizeof_central_dir) << "\n";
		std::clog << "\n";
		return 0;
	}

	if(	central_dir.disk_number || central_dir.central_dir_disk_num)
	{
		std::clog << "PACKAGEZIP: Multi-file ZIP packages are not supported! (" << m_filename << ")\n";
		return 0;
	}

	//-------------------------------------------------------------------------
	//	Add the entries in the zip directory to the package.
	//-------------------------------------------------------------------------
	if(central_dir.dir_entry_count)
	{
		zip_dir_entry	direntry;
		zip_file_header	fileheader;
		std::uint32_t	offset = central_dir.dir_offset;
		char			id[4];
//		char			filename[256];
		bool			finished = false;

		do
		{
			//-----------------------------------------------------------------
			//	Read the directory entry id (PK..)
			//-----------------------------------------------------------------
			zip_file.seekg(offset,std::ios::beg);
			zip_file.read(id,4);

			if(!zip_file.fail() && id[0]=='P' && id[1]=='K' && id[2]==1 && id[3]==2)
			{
				//-------------------------------------------------------------
				//	Read the directory entry.
				//-------------------------------------------------------------
				std::uint32_t direntry_file_offset = (std::uint32_t)zip_file.tellg();

				zip_file.read((char *)&direntry,sizeof_dir_entry);
				if(!zip_file.fail())
				{
					//---------------------------------------------------------
					//	Read the file name.
					//---------------------------------------------------------
					std::string filename;
					filename.resize(direntry.filename_size);
					zip_file.read(&filename[0],direntry.filename_size);
//					filename[direntry.filename_size] = 0;

					zip_file.seekg(direntry.file_offset,std::ios::beg);
					zip_file.read(id,4);

					if(!zip_file.fail() && id[0]=='P' && id[1]=='K' && id[2]==0x03 && id[3]==0x04)
					{
						//-----------------------------------------------------
						//	Read the file header.
						//	If the size of the file is non-zero then process
						//	the file.
						//-----------------------------------------------------
						zip_file.read((char *)&fileheader,sizeof_zipfile_header);

						if(fileheader.size_uncompressed)
						{
							//fileinfo finfo;

							////-------------------------------------------------
							////	Add a fileinfo object to the package.
							////-------------------------------------------------
							//finfo.compression_method	= fileheader.compression_method;
							//finfo.crc					= fileheader.crc;
							//finfo.file_offset			=	direntry.file_offset +
							//								sizeof_zipfile_header +
							//								4 +
							//								fileheader.filename_size +
							//								fileheader.extra_size;
							//finfo.dir_entry_file_offset	= direntry_file_offset;
							//finfo.size_compressed		= fileheader.size_compressed;
							//finfo.size_uncompressed		= fileheader.size_uncompressed;

							//uint32t index = (uint32t)m_fileinfo.size();
							//m_fileinfo.push_back(finfo);

							////-------------------------------------------------
							////	Add an entry for the file into the main directory
							////-------------------------------------------------
							//prootdir->create_file(std::string(filename),finfo.size_uncompressed,this,index);

							FileInfo info;

							info.compression_method		=	fileheader.compression_method;
							info.crc					=	fileheader.crc;
							info.file_offset			=	direntry.file_offset +
															sizeof_zipfile_header +
															4 +
															fileheader.filename_size +
															fileheader.extra_size;
							info.dir_entry_file_offset	=	direntry_file_offset;
							info.size_compressed		=	fileheader.size_compressed;
							info.size_uncompressed		=	fileheader.size_uncompressed;

							add_file(filename,info);

							//std::cout << std::setw(10) << fileheader.size_compressed << std::setw(10) << fileheader.size_uncompressed << " " << filename << "\n";
						}
					}
					else
						finished = true;

					offset += (sizeof_dir_entry+4+direntry.filename_size+direntry.extra_size+direntry.comment_size);
				}
				else
					finished = true;
			}
			else
				finished = true;

		} while(!finished);
	}

	return 0;
}


std::int32_t
PackageZIP::add_file(	const std::string & path,
						FileInfo &			info )
{
	std::int32_t file_id = -1;

	//-------------------------------------------------------------------------
	//	Get the directory that the file should be added to.
	//-------------------------------------------------------------------------
	directory_shared_ptr p_dir;
	std::string filename;

	auto pos = path.find_last_of("/");
	if(pos == std::string::npos)
	{
		p_dir = m_root_directory.p_directory;
		filename = path;
	}
	else
	{
		auto p_node = get_directory(path.substr(0,pos),true);
		if(p_node)
		{
			p_dir = p_node->p_directory;
			filename = path.substr(pos+1);
		}
	}

	if(!p_dir || filename.empty())
		return -1;

	//-------------------------------------------------------------------------
	//	Add the file to the package.
	//-------------------------------------------------------------------------
	file_id = static_cast<std::int32_t>(m_file_info.size());
	m_file_info.push_back(info);

	//-------------------------------------------------------------------------
	//	Add the file to the directory.
	//-------------------------------------------------------------------------
	static_cast<DirectoryZIP *>(p_dir.get())->add_file(filename,file_id);

	return file_id;
}

PackageZIP::DirectoryNode *					
PackageZIP::get_directory(const std::string & path,bool b_create)
{
	std::string dirs(path.size(),0);
	std::transform(path.begin(),path.end(),dirs.begin(),::tolower);

	DirectoryNode * p_dir = &m_root_directory;

	auto pos=dirs.find_first_not_of("/");
	bool err = false;

	while((pos!=std::string::npos) && !err)
	{
		auto pos2 = dirs.find_first_of("/",pos);
		std::string dir;
		
		if(pos2 == std::string::npos)
		{
			dir = dirs.substr(pos);
			pos = pos2;
		}
		else
		{
			dir = dirs.substr(pos,pos2-pos);
			pos = dirs.find_first_not_of("/",pos2);
		}

		if(dir.empty())
		{
			if(pos != std::string::npos)
				err = true;
		}
		else
		{
			auto ifind = p_dir->sub_directories.find(dir);

			if(ifind == p_dir->sub_directories.end())
			{
				if(b_create)
				{
					DirectoryNode node;
					node.p_directory = std::make_shared<DirectoryZIP>(this);
					p_dir->sub_directories[dir] = node;
					ifind = p_dir->sub_directories.find(dir);
				}
				else
					err = true;
			}

			if(!err && (ifind != p_dir->sub_directories.end()))
				p_dir = &ifind->second;
			else
				err = true;
		}
	}

	return (err ? nullptr : p_dir);
}

std::unique_ptr<IFile>
PackageZIP::openfile(	std::int32_t	id,
						std::uint32_t	mode )
{
	std::unique_ptr<IFile> p_file;

	auto p_info = get_file_info(id);

	if(p_info)
	{
		switch(p_info->compression_method)
		{
			//-----------------------------------------------------------------
			case ZIP_UNCOMPRESSED :
			//-----------------------------------------------------------------
				{
					auto p_new_file = std::make_unique<FileZIPStore>();
					if(!p_new_file->open(m_filename,*p_info,mode))
						p_file = std::move(p_new_file);
				}
				break;

			//-----------------------------------------------------------------
			case ZIP_DEFLATED :
			//-----------------------------------------------------------------
				{
					std::ifstream infile(m_filename,std::ios_base::in | std::ios_base::binary);

					if(infile.is_open())
					{
						std::vector<char> data(p_info->size_compressed);
						infile.seekg(p_info->file_offset,std::ios_base::beg);
						if(infile.read(&data[0],p_info->size_compressed).gcount() == p_info->size_compressed)
						{
							try
							{
								auto p_new_file = std::make_unique<FileInMemory>(MODE_READ);
								p_new_file->resize(p_info->size_uncompressed);
								inflate((std::uint8_t *)&data[0],data.size(),(std::uint8_t *)p_new_file->data(),p_new_file->size());
								p_file = std::move(p_new_file);
							}
							catch(...){}
						}
					}
				}
				break;

			default :
				break;
		}
	}

	return p_file;
}


void
PackageZIP::inflate(const std::uint8_t *	p_source,
					size_t					source_size,
					std::uint8_t *			p_target,
					size_t					target_size )
{


#ifdef PKGZIP_INFLATE_ZLIB

	#ifdef HAVE_ZLIB				
		int		err;
		z_stream s;

		s.total_in	= 0;
		s.total_out	= 0;
		s.zalloc	= Z_NULL;
		s.zfree		= Z_NULL;
		s.opaque	= Z_NULL;
		s.avail_in	= 0;
		s.next_in	= Z_NULL;

		err=inflateInit2(&s, -MAX_WBITS);
		if(err == Z_OK)
		{
			s.avail_in	= source_size;
			s.next_in	= (Bytef *)p_source;

			s.avail_out = target_size;
			s.next_out	= (Bytef *)p_target;

			switch(err = ::inflate(&s,Z_FINISH)) //Z_SYNC_FLUSH))
			{
				//---------------------------------------------------------
				case	Z_OK :
				case	Z_STREAM_END :
				//---------------------------------------------------------
					break;

				//---------------------------------------------------------
				case	Z_MEM_ERROR :
				case	Z_BUF_ERROR :
				case	Z_DATA_ERROR :
				default :
				//---------------------------------------------------------
					throw std::exception(__FILE__,__LINE__);
					break;
			}

			inflateEnd(&s);
		}

	#else
		std::clog << "ZIP Deflate is not supported in this version.\n";	
		throw std::exception(__FILE__,__LINE__);
	#endif

//#else
//
//
//	std::clog << "INFLATE: source_size = " << source_size << " bytes. target_size = " << target_size << "bytes\r\n";
//	std::clog << debug::hexdump(p_source,source_size) << std::endl;
//
//
//	ade::Inflater inflater;
//
//	inflater.inflate((std::uint8_t *)p_source,source_size,p_target,target_size);

#else
	(void) p_source;
	(void)source_size;
	(void)p_target;
	(void)target_size;
#endif

}


}} // namespace package_zip, adefs

