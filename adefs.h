//=============================================================================
//	FILE:			adefs.h
//	SYSTEM:			Ade's Virtual File System
//	DESCRIPTION:	
//-----------------------------------------------------------------------------
//  COPYRIGHT:		(C) Copyright 2013-2017 Adrian Purser. All Rights Reserved.
//	LICENCE:		MIT - See LICENSE file for details
//	MAINTAINER:		Adrian Purser <ade@adrianpurser.co.uk>
//	CREATED:		22-JUL-2013 Adrian Purser <ade@adrianpurser.co.uk>
//=============================================================================
#ifndef GUARD_ADEFS_H
#define GUARD_ADEFS_H

#include <fstream>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <vector>
#include <map>
#include <string>
#include <cassert>
#include <mutex>

#define ADEFS_VERSION			0x010000
#define ADEFS_VERSION_STRING	"1.0.0"

namespace adefs
{

//*****************************************************************************
//
//
//	TYPES
//
//
//*****************************************************************************

typedef std::uint32_t	Handle;
typedef std::uint16_t	Attributes;
typedef std::int32_t	fileoffset;
typedef std::uint32_t	filepos;

enum
{
	ATTR_READ	=	0x00001,
	ATTR_WRITE	=	0x00002,
	ATTR_RANDOM	=	0x00004,
	ATTR_DIR	=	0x00008
};

enum
{
	MODE_READ		=	0x00001,
	MODE_WRITE		=	0x00002,
	MODE_APPEND		=	0x00004,
	MODE_AT_END		=	0x00008,
	MODE_TRUNCATE	=	0x00010
};

enum ErrorCode
{
	SUCCESS,
	FILE_IS_LOCKED,
	FILE_NOT_FOUND,
	FILE_IS_READ_ONLY
};


enum class Seek
{
	BEGINNING,
	CURRENT,
	END
};


//*****************************************************************************
//
//
//	INLINE FUNCTIONS
//
//
//*****************************************************************************

static inline void					
split_path(	const std::string & in_path,
			std::string &		out_path,
			std::string &		out_filename )
{
	std::string::size_type pos = in_path.find_last_of("/");

	if(pos == std::string::npos)
	{
		out_path.clear();
		out_filename	= in_path;
	}
	else
	{
		out_path		= in_path.substr(0,pos);
		out_filename	= in_path.substr(pos+1);
	}
}

//*****************************************************************************
//
//
//	CLASSES & INTERFACES
//
//
//*****************************************************************************


//=============================================================================
//
//	FILE
//
//=============================================================================

class IFile
{
public:
	virtual int					get() = 0;
	virtual	size_t				read(char * p_buffer,size_t size) = 0;
	virtual	void				write(const char * p_data,size_t size) = 0;
	virtual void				ignore(size_t count,int delimeter = -1) = 0;
	virtual void				seek(filepos pos) = 0;
	virtual void				seek(fileoffset offset,adefs::Seek dir) = 0;
	virtual size_t				tell() = 0;

	virtual bool				is_fail() = 0;
	virtual bool				is_eof() = 0;
	virtual size_t				count() = 0;
	virtual size_t				size() = 0;
};

//=============================================================================
//
//
//	FILE - IN MEMORY
//
//
//=============================================================================

class FileInMemory : public IFile
{
private:
	std::vector<char>			m_data;
	size_t						m_count		= 0;
	std::uint32_t				m_mode		= MODE_READ | MODE_WRITE;
	filepos						m_position	= 0;
	bool						m_b_fail	= false;

public:
	FileInMemory(const FileInMemory &) = delete;
	FileInMemory & operator=(const FileInMemory &) = delete;

	FileInMemory(void) = default;
	explicit FileInMemory(std::uint32_t	mode) : m_mode(mode) {}

	FileInMemory(	std::uint32_t	mode,
					const char *	p_data,
					size_t			size );
	~FileInMemory(void) = default;

	//-------------------------------------------------------------------------
	//	INTERFACE FUNCTIONS
	//-------------------------------------------------------------------------
	int			get();
	size_t		read(char * p_buffer,size_t size);
	void		write(const char * p_data,size_t size);
	void		ignore(size_t count,int delimeter = -1);
	void		seek(filepos pos);
	void		seek(fileoffset offset,adefs::Seek dir);
	size_t		tell();
	bool		is_fail() 		{return m_b_fail;}
	bool		is_eof() 		{return !!(m_position > m_data.size());}
	size_t		count() 		{return m_count;}

	//-------------------------------------------------------------------------
	//	BUFFER FUNCTIONS
	//-------------------------------------------------------------------------
	void		resize(size_t size);
	char *		data()			{return (m_data.empty() ? nullptr : &m_data[0]);}
	size_t		size() 			{return m_data.size();}
};

//=============================================================================
//
//
//	FILE - ON DISK
//
//
//=============================================================================

class FileOnDisk : public IFile
{
private:
	std::string					m_path;
	std::uint32_t				m_mode;
	std::fstream				m_stream;

public:
	FileOnDisk(void) = delete;
	FileOnDisk(const FileOnDisk &) = delete;
	FileOnDisk & operator=(const FileOnDisk &) = delete;

	// ftream can not currently be move contructed or move assigned with GCC
	// so we cannot currently inplement them here.
/*
	FileOnDisk(FileOnDisk && rhs)
	{
		m_path		= std::move(rhs.m_path);
		m_mode		= rhs.m_mode;
		m_stream	= std::move(rhs.m_stream);
	}

	FileOnDisk & operator=(FileOnDisk && rhs)	
	{
		m_path		= std::move(rhs.m_path);
		m_mode		= rhs.m_mode;
		m_stream	= std::move(rhs.m_stream);
	}
*/
	FileOnDisk(	const std::string &	path,std::uint32_t mode )
		: m_path(path)
		, m_mode(mode)
		, m_stream(path,std::ios_base::openmode((mode & MODE_READ ?		std::ios_base::in	: 0) |
												(mode & MODE_WRITE ?	std::ios_base::out	: 0) |
												(mode & MODE_APPEND ?	std::ios_base::app	: 0) |
												(mode & MODE_AT_END ?	std::ios_base::ate	: 0) |
												(mode & MODE_TRUNCATE ?	std::ios_base::trunc: 0) |
																		std::ios_base::binary ) )
	{
	}

	~FileOnDisk(void) = default;

	int			get()							{return m_stream.get();}
	size_t		read(	char * p_buffer,
						size_t size )			{return static_cast<size_t>(m_stream.read(p_buffer,size).gcount());}
	void		write(	const char * p_data,
						size_t size)			{if(m_mode & MODE_WRITE) m_stream.write(p_data,size);}
	void		ignore(	size_t count,
						int delimeter = -1)		{m_stream.ignore(count,delimeter);}
	size_t		tell()							{return static_cast<size_t>(m_stream.tellg());}
	bool		is_fail()						{return m_stream.fail();}
	bool		is_eof()						{return m_stream.eof();}
	size_t		count()							{return static_cast<size_t>(m_stream.gcount());}
	void		seek(filepos pos)				{m_stream.seekg(pos);}

	void		seek(fileoffset offset,
					 adefs::Seek dir )
				{
					switch(dir)
					{
						case		adefs::Seek::BEGINNING :	m_stream.seekg(offset,std::ios_base::beg);	break;
						case		adefs::Seek::CURRENT :		m_stream.seekg(offset,std::ios_base::cur);	break;
						case		adefs::Seek::END :			m_stream.seekg(offset,std::ios_base::end);	break;
						default :	break;
					}
				}

	size_t		size()	
				{
					auto pos=m_stream.tellg();
					m_stream.seekg(0,std::ios_base::end);
					auto size=m_stream.tellg();
					m_stream.seekg(pos);
					return (size_t)size;
				}

};

//=============================================================================
//
//
//	DIRECTORY
//
//
//=============================================================================

class IDirectory
{
public:
										// Get the size of the specified file in bytes.
	virtual size_t						file_size(const std::string & filename) = 0;

										// Get the attributes of the specified file.
	virtual Attributes					file_attr(const std::string & filename) = 0;

										// Get the attributes of this directory.
	virtual Attributes					dir_attr() = 0;

										// Test whether the specified file exists.
	virtual bool						file_exists(const std::string & filename) = 0;

	virtual std::unique_ptr<IFile>		openfile(	const std::string & filename,
													std::uint32_t		mode = MODE_READ ) = 0;

	virtual std::vector<std::string>	file_list() = 0;
};

typedef std::shared_ptr<IDirectory>	directory_shared_ptr;
typedef std::weak_ptr<IDirectory>	directory_weak_ptr;

//=============================================================================
//
//
//	MOUNTPOINT
//
//
//=============================================================================

typedef std::shared_ptr<class MountPoint>	mountpoint_shared_ptr;
typedef std::weak_ptr<class MountPoint>		mountpoint_weak_ptr;

class MountPoint
{
private:
	MountPoint *									m_p_parent;
	std::map<std::string,mountpoint_shared_ptr>		m_children;
	std::vector<directory_weak_ptr>					m_directories;
	std::string										m_name;
	Attributes										m_attributes;
	std::mutex										m_mutex;			// Mutex for exclusive access.

public:
	MountPoint(void) = delete;
	MountPoint(const MountPoint &) = delete;
	MountPoint & operator=(const MountPoint &) = delete;

	MountPoint(const std::string & name,Attributes attr,MountPoint * p_parent);
	~MountPoint(void);

	MountPoint *				get_mountpoint(const std::string & path,bool b_create=true);
	int							mount(const std::string & path,directory_shared_ptr p_dir);
	const std::string &			name() {return m_name;}
	std::string					fullpath() {if(!m_p_parent) return m_name; return m_p_parent->fullpath() + "/" + m_name;}

	size_t						load(	const std::string & filename,
										char *				p_buffer,
										size_t				buffer_size );

	size_t						load(	const std::string &									filename,
										std::function<void(	fileoffset		offset,
															const char *	p_buffer,
															size_t			buffer_size)>	func,
										char *												p_buffer,
										size_t												buffer_size
									);

	size_t						load(	const std::string &									filename,
										std::function<void(	fileoffset		offset,
															const char *	p_buffer,
															size_t			buffer_size)>	func
									)
								{
									char buffer[512];
									return load(filename,func,buffer,sizeof(buffer));
								}

	std::unique_ptr<IFile>		openfile(	const std::string & filename,
											std::uint32_t		mode = MODE_READ );



	void						write_tree(std::ostream & stream,std::string & prefix);

	void						reset();

private:
	IDirectory *				find_file_owner(const std::string & path,Attributes required_attributes = 0);

};

//=============================================================================
//
//
//	PACKAGE INTERFACE
//
//
//=============================================================================
class IPackage
{
public:
	virtual int						mount(MountPoint * p_mountpoint) = 0;
	virtual int						scan() = 0;
	virtual Attributes				attributes() const = 0;
};

typedef std::shared_ptr<IPackage>	package_shared_ptr;
typedef std::weak_ptr<IPackage>		package_weak_ptr;

//=============================================================================
//
//
//	PACKAGE FACTORY INTERFACE
//
//
//=============================================================================

class IPackageFactory
{
public:
	virtual std::string						name() const = 0;
	virtual std::string						description() const = 0;
	virtual std::vector<std::string>		file_types() const = 0;
	virtual bool							is_supported(const std::string & path) = 0;
	virtual package_shared_ptr				create_package(const std::string & path) = 0;
};

typedef std::shared_ptr<IPackageFactory>	package_factory_shared_ptr;
typedef std::weak_ptr<IPackageFactory>		package_factory_weak_ptr;

//=============================================================================
//
//
//	ADEFS - VIRTUAL FILE SYSTEM
//
//
//=============================================================================

class AdeFS
{
private:
	MountPoint											m_root;
	std::vector<package_shared_ptr>						m_owned_packages;
	std::vector<package_factory_shared_ptr>				m_package_factories;
	std::map<std::string,package_factory_shared_ptr>	m_package_factories_by_type;

	AdeFS(const AdeFS & x);
	AdeFS & operator=(const AdeFS & x);

public:
	AdeFS(void);
	~AdeFS(void);

	int							mount(const std::string & package_name,const std::string & mountpoint = "/");
	int							mount(directory_shared_ptr p_dir,const std::string & mountpoint = "/");
	MountPoint *				get_mountpoint(const std::string & path="",bool b_create=true) {return m_root.get_mountpoint(path,b_create);}

	size_t						load(	const std::string & filename,
										char *				p_buffer,
										size_t				buffer_size );

	size_t						load(	const std::string &									filename,
										std::function<void(	fileoffset		offset,
															const char *	p_buffer,
															size_t			buffer_size)>	func,
										char *												p_buffer,
										size_t												buffer_size
									);

	size_t						load(	const std::string &									filename,
										std::function<void(	fileoffset		offset,
															const char *	p_buffer,
															size_t			buffer_size)>	func
									)
									{
										char buffer[512];
										return load(filename,func,buffer,sizeof(buffer));
									}

	std::unique_ptr<IFile>		openfile(	const std::string & filename,
											std::uint32_t		mode = MODE_READ );

	void						register_package_factory(package_factory_shared_ptr p_factory);

	void						reset();

private:
	package_factory_shared_ptr	get_package_factory(const std::string & package_name);
	package_shared_ptr			create_package(const std::string & package_name);

};

} // namespace adefs

#endif // ! defined GUARD_ADEFS_H
