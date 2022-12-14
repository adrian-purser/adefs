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
#ifndef GUARD_ADEFS_PACKAGE_ZIP_H
#define GUARD_ADEFS_PACKAGE_ZIP_H

#include <iostream>
#include <fstream>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <vector>
#include <map>
#include <string>
#include <thread>
#include <mutex>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "adefs.h"

namespace adefs { namespace package_zip
{
//=============================================================================
//
//
//	PACKAGE ZIP - STRUCTURES
//
//
//=============================================================================
#pragma pack(push,1)

	//-----------------------------------------------------------------------------
	//	ZIP file central directory header.
	//-----------------------------------------------------------------------------
	struct zip_central_dir	
	{
		std::uint16_t	disk_number;					//	Number of this disk.
		std::uint16_t	central_dir_disk_num;			//	Number of the disk that contains the start of the central directory.
		std::uint16_t	dir_entry_count_this_disk;		//	Number of central directory entries on this disk.
		std::uint16_t	dir_entry_count;				//	Total number of directory entries in the central directory.
		std::uint32_t	dir_size;						//	Size of the central directory.
		std::uint32_t	dir_offset;						//	Offset of the start of the central directory.
		std::uint16_t	comment_length;					//	The length of the comment (0-65535 bytes).
		char			comment[1];						//	Start of the comment.
	};

	//-----------------------------------------------------------------------------
	//	ZIP file central directory entry.
	//-----------------------------------------------------------------------------
	struct zip_dir_entry
	{
		std::uint16_t	version;						//	The version of this entry.
		std::uint16_t	version_needed;					//	The version number needed to decompress this entry.
		std::uint16_t	flag;							//	??
		std::uint16_t	compression_method;				//	0 = uncompressed, 8 = deflated.
		std::uint32_t	dos_date;						//
		std::uint32_t	crc;							//	File CRC.
		std::uint32_t	size_compressed;				//	The size of the file (compressed).
		std::uint32_t	size_uncompressed;				//	The size of the file (uncompressed).
		std::uint16_t	filename_size;					//	The size of the filename.
		std::uint16_t	extra_size;						//	??
		std::uint16_t	comment_size;					//	The size of the comment.
		std::uint16_t	disk_num_start;					//
		std::uint16_t	internal_fa;					//
		std::uint32_t	external_fa;					//
		std::uint32_t	file_offset;					//	Offset of the file data in the zip file.
		std::uint8_t	filename[1];					//
	};

	//-----------------------------------------------------------------------------
	//	ZIP file header.
	//-----------------------------------------------------------------------------
	struct zip_file_header
	{
		std::uint16_t	version;						//	The version of this entry.
		std::uint16_t	flag;							//	??
		std::uint16_t	compression_method;				//	0 = uncompressed, 8 = deflated.
		std::uint32_t	dos_date;						//
		std::uint32_t	crc;							//	File CRC.
		std::uint32_t	size_compressed;				//	The size of the file (compressed).
		std::uint32_t	size_uncompressed;				//	The size of the file (uncompressed).
		std::uint16_t	filename_size;					//	The size of the filename.
		std::uint16_t	extra_size;						//	??
	};

	//-----------------------------------------------------------------------------
	//	GZIP header.
	//-----------------------------------------------------------------------------
	struct gzip_header
	{
		unsigned char	id1;
		unsigned char	id2;
		unsigned char	cm;
		unsigned char	flg;
		std::uint32_t	mtime;
		unsigned char	xfl;
		unsigned char	os;
	};


	static const size_t	sizeof_central_dir		= (sizeof(zip_central_dir)-1);
	static const size_t	sizeof_dir_entry		= (sizeof(zip_dir_entry)-1);
	static const size_t	sizeof_zipfile_header	= (sizeof(zip_file_header));

	struct FileInfo
	{
		std::int32_t				size_compressed;
		std::int32_t				size_uncompressed;
		std::int32_t				file_offset;
		std::int32_t				dir_entry_file_offset;
		std::uint32_t				crc;
		std::uint16_t				compression_method;
	};

	enum
	{
		ZIP_UNCOMPRESSED	= 0,
		ZIP_DEFLATED		= 8
	};
#pragma pack(pop)

//=============================================================================
//
//
//	PACKAGE ZIP - FILE CLASS (STORE)
//
//
//=============================================================================
class FileZIPStore : public IFile
{
private:
	FileInfo						m_fileinfo;
	std::ifstream					m_zip_file;
	size_t							m_count;

public:
	FileZIPStore(const FileZIPStore &) = delete;
	FileZIPStore & operator=(const FileZIPStore & x) = delete;

	FileZIPStore(void);
	~FileZIPStore(void);

	int								open(	const std::string &	zip_filename,
											const FileInfo &	fileinfo,
											std::uint32_t		mode );

	int								get();
	size_t							read(char * p_buffer,size_t size);
	void							write(const char * p_data,size_t size);
	void							ignore(size_t count,int delimeter = -1);
	void							seek(filepos pos);
	void							seek(fileoffset offset,adefs::Seek dir);
	size_t							tell()			{return static_cast<size_t>(m_zip_file.tellg())-m_fileinfo.file_offset;}
	bool							is_fail() 		{return m_zip_file.fail();}
	bool							is_eof() 		{return !!(m_zip_file.tellg() >= (m_fileinfo.file_offset + m_fileinfo.size_uncompressed));}
	size_t							count() 		{return static_cast<size_t>(m_zip_file.gcount());}
	size_t							size() 			{return m_fileinfo.size_uncompressed;}

private:
	bool							is_open() const {return !!m_zip_file.is_open();}

};

//=============================================================================
//
//
//	PACKAGE ZIP - DIRECTORY CLASS
//
//
//=============================================================================
class DirectoryZIP : public IDirectory
{
private:
	typedef std::map<std::string,std::int32_t> FileInfoArray;

	//=========================================================================
	//	ATTRIBUTES
	//=========================================================================
	class PackageZIP *					m_p_package;	// A pointer to the package that owns this directory.
	std::mutex							m_mutex;		// Mutex for exclusive access to the directory.
	std::map<std::string,std::int32_t>	m_files;		// An array of structures that contain information about the files 
														// in this directory. The key for this map holds the filename 
														// converted to lower case so that files can be looked up in a 
														// non case sensitive way.

	//=========================================================================
	//	PRIVATE FUNCTIONS
	//=========================================================================
private:
	// Stop the object being copied.
	DirectoryZIP(void);
	DirectoryZIP(const DirectoryZIP & x);
	DirectoryZIP & operator=(const DirectoryZIP & x);

	int								get_file_id(const std::string & filename);


	//=========================================================================
	//	PUBLIC FUNCTIONS
	//=========================================================================
public:
	DirectoryZIP(class PackageZIP * p_package);
	~DirectoryZIP();

	//-------------------------------------------------------------------------
	//	NON-INTERFACE FUNCTIONS
	//-------------------------------------------------------------------------
	void							add_file(	const std::string & filename,
												std::int32_t		id );

	//-------------------------------------------------------------------------
	//	INTERFACE FUNCTIONS
	//-------------------------------------------------------------------------

									// Get the size of the specified file in bytes.
	size_t							file_size(const std::string & filename);

									// Get the attributes of the specified file.
	Attributes						file_attr(const std::string & filename);

									// Get the attributes of this directory.
	Attributes						dir_attr() {return ATTR_READ;}

									// Test whether the specified file exists.
	bool							file_exists(const std::string & filename);

	std::vector<std::string>		file_list();

	std::unique_ptr<IFile>			openfile(	const std::string & filename,
												std::uint32_t		mode = MODE_READ );

};



//=============================================================================
//
//
//	PACKAGE ZIP - PACKAGE CLASS
//
//
//=============================================================================

class PackageZIP : public IPackage
{
private:
	struct DirectoryNode
	{
		directory_shared_ptr					p_directory;
		std::map<std::string,DirectoryNode>		sub_directories;
	};

	std::string									m_filename;			// The name of the ZIP file.
	std::mutex									m_mutex;			// Mutex for exclusive access.
	std::vector<FileInfo>						m_file_info;		// An array of FileInfo objects. One entry for each file in the package.
	DirectoryNode								m_root_directory;	// The root node of the directory tree.

	//-------------------------------------------------------------------------
	// Prevent the object from being copied.
	//-------------------------------------------------------------------------
	PackageZIP(void);
	PackageZIP(const PackageZIP &);
	PackageZIP & operator=(const PackageZIP &);

public:
	PackageZIP(const std::string & filename);
	~PackageZIP(void);

	const std::string &				get_filename() const				{return m_filename;}
	size_t							get_filesize(std::int32_t id) const
									{
										if((id>=0) && (id<(std::int32_t)m_file_info.size()))
											return m_file_info[id].size_uncompressed;
										return 0;
									}

	std::unique_ptr<IFile>			openfile(	std::int32_t	id,
												std::uint32_t	mode = MODE_READ);

	//-------------------------------------------------------------------------
	//	PACKAGE INTERFACE FUNCTIONS
	//-------------------------------------------------------------------------
	int								mount(MountPoint * p_mountpoint);
	int								scan();
	Attributes						attributes() const	{return ATTR_READ;}

private:
	std::int32_t					add_file(	const std::string & path,
												FileInfo &			info );

	DirectoryNode *					get_directory(const std::string & path,bool b_create = false);
	const FileInfo *				get_file_info(std::int32_t id) const
									{
										if((id>=0) && (id<(std::int32_t)m_file_info.size()))
											return &m_file_info[id];
										return nullptr;
									}

	int								mount_directory(MountPoint *			p_mountpoint,
													const std::string &		path,
													DirectoryNode &			dir_node );

	void							inflate(const std::uint8_t *	p_source,
											size_t					source_size,
											std::uint8_t *			p_target,
											size_t					target_size );

};





}} // namespace package_zip, adefs

#endif // ! defined GUARD_ADEFS_PACKAGE_ZIP_H

