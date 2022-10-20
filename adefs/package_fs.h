//=============================================================================
//	FILE:					package_fs.h
//	SYSTEM:				Ade's Virtual File System
//	DESCRIPTION:
//-----------------------------------------------------------------------------
//  COPYRIGHT:		(C) Copyright 2020 Adrian Purser. All Rights Reserved.
//	LICENCE:			MIT - See LICENSE file for details
//	MAINTAINER:		Adrian Purser <ade@adrianpurser.co.uk>
//	CREATED:			17-JUL-2013 Adrian Purser <ade@adrianpurser.co.uk>
//=============================================================================
#ifndef GUARD_ADEFS_PACKAGE_FS_H
#define GUARD_ADEFS_PACKAGE_FS_H

#include <iostream>
#include <fstream>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <stack>
#include <vector>
#include <map>
#include <string>
#include <cassert>
#include <thread>
#include <mutex>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif


#include "adefs.h"

namespace adefs
{


namespace package_fs
{

//=============================================================================
//
//
//	FINDFILE CLASS
//
//
//=============================================================================

#ifdef _WIN32

class FindFile
{
private:
	bool							m_b_dir;
	bool							m_b_dots;
	bool							m_b_first;
	size_t							m_size;
	std::string						m_path;
	intptr_t						m_handle;
	_finddata_t 					m_fileinfo;

public:
	FindFile() : m_b_dir(false), m_b_dots(false),m_b_first(false), m_size(0), m_handle(-1){}
	~FindFile() {close();}

	//-------------------------------------------------------------------------
	//	close
	//-------------------------------------------------------------------------
	void close()
	{
		if(m_handle>=0)
		{
			_findclose(m_handle);
			m_handle=-1;
		}
	}

	//-------------------------------------------------------------------------
	//	find
	//-------------------------------------------------------------------------
	bool find(const std::string & name)
	{
		bool found = false;

		if(!name.empty())
		{
			m_path = name;
			std::string path	= name+std::string("*.*");
			m_handle			= _findfirst(path.c_str(),&m_fileinfo);
			found				= (m_b_first = (m_handle>=0));	
		}
		return found;
	}

	//-------------------------------------------------------------------------
	//	findnext
	//-------------------------------------------------------------------------
	bool findnext()
	{
		if(m_handle < 0)
			return false;
		if(m_b_first)
			m_b_first=false;
		else if(_findnext(m_handle,&m_fileinfo)!=0)
			return false;

		m_b_dir		=	!!(m_fileinfo.attrib & _A_SUBDIR);
		m_b_dots	=	m_b_dir && 
						(m_fileinfo.name[0]=='.') && 
						((m_fileinfo.name[1]==0)||((m_fileinfo.name[1]=='.')&&(m_fileinfo.name[2]==0)));
		m_size		=   m_fileinfo.size;

		return true;
	}

	//-------------------------------------------------------------------------
	//	getfilename
	//-------------------------------------------------------------------------
	std::string filename()
	{
		std::string name;
		if(m_handle>=0)
			name = m_fileinfo.name;
		return name;
	}

	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------
	bool	isdirectory() const		{return m_b_dir;}
	bool	isdots() const			{return m_b_dots;}
	size_t	size() const			{return m_size;}
};

#else

class FindFile
{
private:
	bool							m_b_dir;
	bool							m_b_dots;
	size_t							m_size;
	std::string						m_path;
	DIR *							m_dir;
	struct dirent *					m_p_dirent;

public:
	FindFile() : m_b_dir(false), m_b_dots(false), m_dir(NULL),m_p_dirent(NULL){}
	~FindFile() {close();}

	//-------------------------------------------------------------------------
	//	close
	//-------------------------------------------------------------------------
	void close()
	{
		if(m_dir)
		{
			closedir(m_dir);
			m_dir = NULL;
		}
		m_p_dirent = NULL;
	}

	//-------------------------------------------------------------------------
	//	find
	//-------------------------------------------------------------------------
	bool find(const std::string & name)
	{
		bool found = false;

		if(!name.empty())
		{
			m_path				= name;
			std::string path	= name+std::string(".");
			m_dir				= opendir(path.c_str());
			found				= !!m_dir;
		}
		return found;
	}

	//-------------------------------------------------------------------------
	//	findnext
	//-------------------------------------------------------------------------
	bool findnext()
	{
		if(!m_dir)
			return false;
		m_p_dirent = readdir(m_dir);
		if(m_p_dirent)
		{
			m_b_dir		=	(m_p_dirent->d_type & DT_DIR);
			m_b_dots	=	m_b_dir && 
							(m_p_dirent->d_name[0]=='.') && 
							((m_p_dirent->d_name[1]==0)||((m_p_dirent->d_name[1]=='.')&&(m_p_dirent->d_name[2]==0)));
			if(m_p_dirent->d_type == DT_REG)
			{
				std::string filename(m_path);
				filename.append(m_p_dirent->d_name);
				struct stat st;
				stat(filename.c_str(), &st);
				m_size = st.st_size;
			}
			else
				m_size = 0;
		}
		return !!m_p_dirent;
	}


	//-------------------------------------------------------------------------
	//	getfilename
	//-------------------------------------------------------------------------
	std::string filename()
	{
		std::string name;
		if(m_dir && m_p_dirent)
			name = m_p_dirent->d_name;
		return name;
	}

	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------
	bool	isdirectory() const		{return m_b_dir;}
	bool	isdots() const			{return m_b_dots;}
	size_t	size() const			{return m_size;}
};

#endif


//=============================================================================
//
//
//	PACKAGE FILESYSTEM - FILE CLASS
//
//
//=============================================================================
/*

class FileFS : public IFile
{
private:
	std::string						m_path;
	std::uint32_t					m_mode;
	class DirectoryFS *				m_p_owner;		// The Directory object that this file object belongs to.
	std::fstream					m_stream;

	FileFS(void);
	FileFS(const FileFS & x);
	FileFS & operator=(const FileFS & x);

public:
	FileFS(	const std::string &		path,
			std::uint32_t			mode,
			DirectoryFS *			p_owner );
	~FileFS(void);

	int								get();
	size_t							read(char * p_buffer,size_t size);
	void							write(const char * p_data,size_t size);
	void							ignore(size_t count,int delimeter = -1);
	void							seek(filepos pos);
	void							seek(fileoffset offset,seek::Direction dir);
	size_t							tell();

	bool							is_fail();
	bool							is_eof();
	size_t							count();
};

*/

//=============================================================================
//
//
//	PACKAGE FILESYSTEM - DIRECTORY CLASS
//
//
//=============================================================================

class DirectoryFS : public IDirectory
{
private:
	struct FileInfo
	{
		std::string					filename;
		Attributes					attributes;
		struct stat					status;
	};

	//=========================================================================
	//	ATTRIBUTES
	//=========================================================================
	std::string											m_path;							// The full path to this directory on the host file system.
	Attributes											m_attributes;				// The attributes for this directory (eg. ATTR_READ, ATTR_WRITE)
	std::mutex											m_mutex;						// Mutex for exclusive access to the directory.
	std::map<std::string,FileInfo>	m_files;						// An array of structures that contain information about the files in this directory.

	bool														m_b_logging = true;

	//=========================================================================
	//	PRIVATE FUNCTIONS
	//=========================================================================
private:
	// Stop the object being copied.
	DirectoryFS(void);
	DirectoryFS(const DirectoryFS & x);
	DirectoryFS & operator=(const DirectoryFS & x);

	int								rescan_file(FileInfo & fileinfo);
	int								get_fileinfo(	const std::string & filename, FileInfo & out_fileinfo );

	//=========================================================================
	//	PUBLIC FUNCTIONS
	//=========================================================================
public:
	DirectoryFS(const std::string & path,Attributes attr);
	~DirectoryFS();

	//-------------------------------------------------------------------------
	//	NON-INTERFACE FUNCTIONS
	//-------------------------------------------------------------------------
	const std::string &				get_path() {return m_path;}
	int												scan(std::vector<std::string> * p_out_directories = nullptr);

	//-------------------------------------------------------------------------
	//	INTERFACE FUNCTIONS
	//-------------------------------------------------------------------------

	size_t										file_size(const std::string & filename);
	Attributes								file_attr(const std::string & filename);
	Attributes								dir_attr() {return m_attributes;}
	bool											file_exists(const std::string & filename);
	std::vector<std::string>	file_list();
	std::unique_ptr<IFile>		openfile(	const std::string & filename, std::uint32_t mode = MODE_READ );

};

//=============================================================================
//
//
//	PACKAGE FILESYSTEM - PACKAGE CLASS
//
//
//=============================================================================

class PackageFS : public IPackage
{
private:
	std::string							m_path;
	Attributes							m_attributes;
	std::mutex							m_mutex;		// Mutex for exclusive access.
	std::vector<directory_shared_ptr>	m_directories;

	// Prevent the object from being copied.
	PackageFS(void);
	PackageFS(const PackageFS &);
	PackageFS & operator=(const PackageFS &);

public:
	PackageFS(	const std::string &		path,
				Attributes				attributes = ATTR_READ );
	~PackageFS(void);

	//-------------------------------------------------------------------------
	//	PACKAGE INTERFACE FUNCTIONS
	//-------------------------------------------------------------------------
	int								mount(MountPoint * p_mountpoint);
	int								scan()				{m_directories.clear();return scan(m_path);}
	Attributes						attributes() const	{return m_attributes;}

private:
	int								scan(const std::string & path);

};


} // namespace package_fs
} // namespace adefs

#endif // ! defined GUARD_ADEFS_PACKAGE_FS_H
