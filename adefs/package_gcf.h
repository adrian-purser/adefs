//=============================================================================
//	FILE:					package_gcf.h
//	SYSTEM:				Ade's Virtual File System
//	DESCRIPTION:
//-----------------------------------------------------------------------------
//  COPYRIGHT:		(C) Copyright 2020 Adrian Purser. All Rights Reserved.
//	LICENCE:			MIT - See LICENSE file for details
//	MAINTAINER:		Adrian Purser <ade@adrianpurser.co.uk>
//	CREATED:			28-AUG-2013 Adrian Purser <ade@adrianpurser.co.uk>
//=============================================================================
#ifndef GUARD_ADEFS_PACKAGE_GCF_H
#define GUARD_ADEFS_PACKAGE_GCF_H

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

namespace adefs { namespace package_gcf
{

//=============================================================================
//
//
//	PACKAGE GCF - STRUCTURES
//
//
//=============================================================================

#pragma pack(push,1)

//-----------------------------------------------------------------------------
//	GCF Header
//-----------------------------------------------------------------------------
struct GCFHeader
{
	std::uint32_t Dummy0;			// Always 0x00000001
	std::uint32_t Dummy1;			// Always 0x00000001
	std::uint32_t FormatVersion;	
	std::uint32_t CacheID;
	std::uint32_t GCFVersion;
	std::uint32_t Dummy3;
	std::uint32_t Dummy4;
	std::uint32_t FileSize;			// Total size of GCF file in bytes.
	std::uint32_t BlockSize;		// Size of each data block in bytes.
	std::uint32_t BlockCount;		// Number of data blocks.
	std::uint32_t Dummy5;
};

//-----------------------------------------------------------------------------
//	GCF Block Header
//-----------------------------------------------------------------------------
struct GCFBlockEntryHeader
{
	std::uint32_t BlockCount;		// Number of data blocks.
	std::uint32_t BlocksUsed;		// Number of data blocks that point to data.
	std::uint32_t Dummy0;
	std::uint32_t Dummy1;
	std::uint32_t Dummy2;
	std::uint32_t Dummy3;
	std::uint32_t Dummy4;
	std::uint32_t Checksum;			// Header checksum.
};
		
//-----------------------------------------------------------------------------
//	GCF Block Entry
//-----------------------------------------------------------------------------
struct GCFBlockEntry
{
	std::uint32_t EntryType;				// Flags for the block entry.  0x200F0000 == Not used.
	std::uint32_t FileDataOffset;			// The offset for the data contained in this block entry in the file.
	std::uint32_t FileDataSize;				// The length of the data in this block entry.
	std::uint32_t FirstDataBlockIndex;		// The index to the first data block of this block entry's data.
	std::uint32_t NextBlockEntryIndex;		// The next block entry in the series.  (N/A if == BlockCount.)
	std::uint32_t PreviousBlockEntryIndex;	// The previous block entry in the series.  (N/A if == BlockCount.)
	std::uint32_t DirectoryIndex;			// The index of the block entry in the directory.
};
	
//-----------------------------------------------------------------------------
//	GCF Fragmentation Map Header
//-----------------------------------------------------------------------------
struct GCFFragMapHeader
{
	std::uint32_t BlockCount;		// Number of data blocks.
	std::uint32_t Dummy0;			// index of 1st unused GCFFRAGMAP entry?
	std::uint32_t Dummy1;
	std::uint32_t Checksum;			// Header checksum.
};
	
//-----------------------------------------------------------------------------
//	GCF Block Map Header
//-----------------------------------------------------------------------------
struct GCFBlockEntryMapHeader
{
	std::uint32_t BlockCount;			// Number of data blocks.	
	std::uint32_t FirstBlockEntryIndex;	// Index of the first block entry.
	std::uint32_t LastBlockEntryIndex;	// Index of the last block entry.
	std::uint32_t Dummy0;
	std::uint32_t Checksum;				// Header checksum.
};		

//-----------------------------------------------------------------------------
//	GCF Block Map Entry
//-----------------------------------------------------------------------------
struct GCFBlockEntryMap
{
	std::uint32_t PreviousBlockEntryIndex;	// The previous block entry.  (N/A if == BlockCount.)
	std::uint32_t NextBlockEntryIndex;		// The next block entry.  (N/A if == BlockCount.)
};
	
//-----------------------------------------------------------------------------
//GCF Directory Header
//-----------------------------------------------------------------------------
struct GCFDirectoryHeader
{
	std::uint32_t Dummy0;			// Always 0x00000004
	std::uint32_t CacheID;			// Cache ID.
	std::uint32_t GCFVersion;		// GCF file version.
	std::uint32_t ItemCount;		// Number of items in the directory.	
	std::uint32_t FileCount;		// Number of files in the directory.
	std::uint32_t Dummy1;			// Always 0x00008000
	std::uint32_t DirectorySize;	// Size of lpGCFDirectoryEntries & lpGCFDirectoryNames & lpGCFDirectoryInfo1Entries & lpGCFDirectoryInfo2Entries & lpGCFDirectoryCopyEntries & lpGCFDirectoryLocalEntries in bytes.
	std::uint32_t NameSize;			// Size of the directory names in bytes.
	std::uint32_t Info1Count;		// Number of Info1 entires.
	std::uint32_t CopyCount;		// Number of files to copy.
	std::uint32_t LocalCount;		// Number of files to keep local.
	std::uint32_t Dummy2;
	std::uint32_t Dummy3;
	std::uint32_t Checksum;			// Header checksum.
};

//-----------------------------------------------------------------------------
//GCF Directory Entry
//-----------------------------------------------------------------------------
struct GCFDirectoryEntry
{
	std::uint32_t NameOffset;		// Offset to the directory item name from the end of the directory items.
	std::uint32_t ItemSize;			// Size of the item.  (If file, file size.  If folder, num items.)
	std::uint32_t ChecksumIndex;	// Checksum index. (0xFFFFFFFF == None).
	std::uint32_t DirectoryType;	// Flags for the directory item.  (0x00000000 == Folder).
	std::uint32_t ParentIndex;		// Index of the parent directory item.  (0xFFFFFFFF == None).
	std::uint32_t NextIndex;		// Index of the next directory item.  (0x00000000 == None).
	std::uint32_t FirstIndex;		// Index of the first directory item.  (0x00000000 == None).
};

//-----------------------------------------------------------------------------
//	GCF Directory Info 1 Entry
//-----------------------------------------------------------------------------
struct GCFDirectoryInfo1Entry
{
	std::uint32_t Dummy0;
};

//-----------------------------------------------------------------------------
//	GCF Directory Info 2 Entry
//-----------------------------------------------------------------------------
struct GCFDirectoryInfo2Entry
{
	std::uint32_t Dummy0;
};

//-----------------------------------------------------------------------------
//	GCF Directory Copy Entry
//-----------------------------------------------------------------------------
struct GCFDirectoryCopyEntry
{
	std::uint32_t DirectoryIndex;	// Index of the directory item.
};

//-----------------------------------------------------------------------------
//	GCF Directory Local Entry
//-----------------------------------------------------------------------------
struct GCFDirectoryLocalEntry
{
	std::uint32_t DirectoryIndex;	// Index of the directory item.
};

//-----------------------------------------------------------------------------
//	GCF Directory Map Header
//-----------------------------------------------------------------------------
struct GCFDirectoryMapHeader
{
	std::uint32_t Dummy0;			// Always 0x00000001
	std::uint32_t Dummy1;			// Always 0x00000000
};

//-----------------------------------------------------------------------------
//	GCF Directory Map Entry
//-----------------------------------------------------------------------------
struct GCFDirectoryMapEntry
{
	std::uint32_t FirstBlockIndex;	// Index of the first data block. (N/A if == BlockCount.)
};
	
//-----------------------------------------------------------------------------
//	GCF Checksum Header
//-----------------------------------------------------------------------------
struct GCFChecksumHeader
{
	std::uint32_t Dummy0;			// Always 0x00000001
	std::uint32_t ChecksumSize;		// Size of LPGCFCHECKSUMHEADER & LPGCFCHECKSUMMAPHEADER & in bytes.
};
 
//-----------------------------------------------------------------------------
//	GCF Checksum Map Header
//-----------------------------------------------------------------------------
struct GCFChecksumMapHeader
{
	std::uint32_t Dummy0;			// Always 0x14893721
	std::uint32_t Dummy1;			// Always 0x00000001
	std::uint32_t ItemCount;		// Number of items.
	std::uint32_t ChecksumCount;	// Number of checksums.
};
 
//-----------------------------------------------------------------------------
//	GCF Checksum Map Entry
//-----------------------------------------------------------------------------
struct GCFChecksumMapEntry
{
	std::uint32_t ChecksumCount;		// Number of checksums.
	std::uint32_t FirstChecksumIndex;	// Index of first checksum.
};
 
//-----------------------------------------------------------------------------
//	GCF Checksum Entry
//-----------------------------------------------------------------------------
struct GCFchecksumEntry
{
	std::uint32_t Checksum;			// Checksum.
};

//-----------------------------------------------------------------------------
//	GCF Data Header
//-----------------------------------------------------------------------------
struct GCFDataBlockHeader
{
	std::uint32_t GCFVersion;		// GCF file version.
	std::uint32_t BlockCount;		// Number of data blocks.
	std::uint32_t BlockSize;		// Size of each data block in bytes.
	std::uint32_t FirstBlockOffset;	// Offset to first data block.
	std::uint32_t BlocksUsed;		// Number of data blocks that contain data.
	std::uint32_t Checksum;			// Header checksum.
};

#pragma pack(pop)

//=============================================================================
//
//
//	PACKAGE GCF - FILE CLASS
//
//
//=============================================================================


class FileGCF : public IFile
{
private:
	std::uint32_t					m_mode;
	class PackageGCF *				m_p_package;		
	std::ifstream					m_stream;
	std::uint32_t					m_block_index;
	std::uint32_t					m_size;
	std::uint32_t					m_id;
	std::uint32_t					m_first_data_block_offset;

	std::uint32_t					m_file_pointer;		// The current position in the file.
	std::uint32_t					m_block_num;		// The current block number (that relates to m_file_pointer).
	std::uint32_t					m_block_offset;		// The offset from the start of the current block to the current file pointer.
	size_t							m_block_data_avail;	// The amount of data available in the current block.

	size_t							m_gcount;			// The amount of data read by the last read operation.
	bool							m_b_failbit;
	std::uint32_t					m_block_size;
	std::uint32_t					m_first_data_block_index;	


public:
	FileGCF(void) = delete;
	FileGCF(const FileGCF &) = delete;
	FileGCF & operator=(const FileGCF &) = delete;

	FileGCF(std::uint32_t			id,
			std::uint32_t			mode,
			class PackageGCF *		p_package);
	~FileGCF(void);

	int								get();
	size_t							read(char * p_buffer,size_t size);
	void							write(const char * p_data,size_t size);
	void							ignore(size_t count,int delimeter = -1);
	void							seek(filepos pos)							{m_file_pointer = std::min<std::uint32_t>(pos,m_size);	update_block_info();}
	void							seek(fileoffset offset,adefs::Seek dir);
	size_t							tell()										{return m_file_pointer;}

	bool							is_fail()									{return m_b_failbit;}
	bool							is_eof()									{return m_file_pointer >= m_size;}
	size_t							count()										{return m_gcount;}
	size_t							size()										{return m_size;}

private:
	void							update_block_info();
};

//=============================================================================
//
//
//	PACKAGE GCF - DIRECTORY CLASS
//
//
//=============================================================================
class DirectoryGCF : public IDirectory
{
private:
	struct FileInfo
	{
		std::string					filename;
		std::uint32_t				index;				// The block index of the start of the file.
		std::uint32_t				size;				// Size of the file in bytes.
		std::uint32_t				file_id;			// The index of the first data block for this file.
	};

	//=========================================================================
	//	ATTRIBUTES
	//=========================================================================
	class PackageGCF *				m_p_package;		// A pointer to the package that owns this directory. This is used
														// to get access to the GCF file data.
	std::mutex						m_mutex;			// Mutex for exclusive access to the directory.
	std::map<std::string,FileInfo>	m_files;			// An array of structures that contain information about the files 
														// in this directory. The key for this map holds the filename 
														// converted to lower case so that files can be looked up in a 
														// non case sensitive way.

	//=========================================================================
	//	PRIVATE FUNCTIONS
	//=========================================================================
private:
	// Stop the object being copied.
	DirectoryGCF(void);
	DirectoryGCF(const DirectoryGCF & x);
	DirectoryGCF & operator=(const DirectoryGCF & x);

	int								get_fileinfo(	const std::string & filename,
													FileInfo & out_fileinfo );


	//=========================================================================
	//	PUBLIC FUNCTIONS
	//=========================================================================
public:
	DirectoryGCF(class PackageGCF * p_package);
	~DirectoryGCF();

	//-------------------------------------------------------------------------
	//	NON-INTERFACE FUNCTIONS
	//-------------------------------------------------------------------------
	void							add_file(	const std::string & filename,
												std::uint32_t		index,
												std::uint32_t		size,
												std::uint32_t		id);

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
//	PACKAGE GCF - PACKAGE CLASS
//
//
//=============================================================================

class PackageGCF : public IPackage
{
private:
	struct DirectoryInfo
	{
		std::vector<char>						raw_dir_block;
		std::vector<std::uint32_t>				dir_map;
		GCFDirectoryHeader *					p_header;
		GCFDirectoryEntry *						p_entries;
		char *									p_names;
		std::uint32_t							dir_entries_offset;
	};

	struct FileInfo
	{
		std::uint32_t							file_size;					// Size of the item.  (If file, file size.  If folder, num items.)
		std::uint32_t							data_block_index;			// The index of the first data block for this file.
	};

	struct DirectoryNode
	{
		directory_shared_ptr					p_directory;
		std::map<std::string,DirectoryNode>		sub_directories;
	};

	std::string									m_filename;
	std::mutex									m_mutex;					// Mutex for exclusive access.
	DirectoryNode								m_root_directory;			// The root node of the directory tree.
	std::vector<FileInfo>						m_file_info;

	GCFHeader									m_gcf_header;
	GCFDataBlockHeader							m_gcf_data_block_header;
	std::uint32_t								m_fragmap_file_offset;			
	std::vector<std::uint32_t>					m_frag_map;


	//-------------------------------------------------------------------------
	// Prevent the object from being copied.
	//-------------------------------------------------------------------------
	PackageGCF(void);
	PackageGCF(const PackageGCF &);
	PackageGCF & operator=(const PackageGCF &);

public:
	PackageGCF(const std::string & filename);
	~PackageGCF(void);

	const std::string &				get_filename() const				{return m_filename;}
	std::uint32_t					get_blocksize() const				{return m_gcf_header.BlockSize;}
	std::uint32_t					get_blockcount() const				{return m_gcf_header.BlockCount;}
	std::uint32_t					get_first_block_offset() const		{return m_gcf_data_block_header.FirstBlockOffset;}
	std::uint32_t					get_next_block(std::uint32_t index)	{return m_frag_map[index];}
	std::uint32_t					get_block_index(std::uint32_t first_block,fileoffset offset);

	bool							get_file_info(	std::uint32_t		file_id,
													std::uint32_t &		out_block_index,
													std::uint32_t &		out_file_size );

	//-------------------------------------------------------------------------
	//	PACKAGE INTERFACE FUNCTIONS
	//-------------------------------------------------------------------------
	int								mount(MountPoint * p_mountpoint);
	int								scan();
	Attributes						attributes() const	{return ATTR_READ;}

private:
	void							scan_directory(	DirectoryInfo &			dirinfo,
													std::uint32_t			entry_index,
													DirectoryNode &			p_node );

	int								mount_directory(MountPoint *			p_mountpoint,
													const std::string &		path,
													DirectoryNode &			dir_node );

	std::uint32_t					add_file(std::uint32_t size,std::uint32_t block_offset);

};

//=============================================================================
//
//
//	PACKAGE FACTORY CLASS
//
//
//=============================================================================

class PackageFactoryGCF : public IPackageFactory
{
public:
	std::string						name() const override			{return "GCF";}
	std::string						description() const	override	{return "Valve GCF (Game Cache File)";}
	std::vector<std::string>		file_types() const override		{std::vector<std::string> v;v.push_back("gcf");return v;}

	bool							is_supported(const std::string & path) override;
	package_shared_ptr				create_package(const std::string & path) override;
};


}} // namespace package_gcf, adefs

#endif // ! defined GUARD_ADEFS_PACKAGE_GCF_H
