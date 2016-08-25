/*
 *******************************************************************************
 * vRapid Mobile(R) Notice
 *  
 * Copyright&copy; 1999-2014, Red Bend Software. All Rights Reserved.
 *
 * Patented: www.redbend.com/red-bend-patents.pdf
 * 
 * This Software is the property of Red Bend Ltd. and contains trade
 * secrets, know-how, confidential information and other intellectual
 * property of Red Bend Ltd.
 *
 * vRapid Mobile(R), Red Bend(R), and other Red Bend names, as well as the
 * Red Bend Logo are trademarks or registered trademarks of Red Bend
 * Ltd.
 *
 * All other names and trademarks are the property of their respective
 * owners.
 *
 * The Product contains components owned by third parties. Copyright
 * notices and terms under which such components are licensed can be
 * found at the following URL, and are hereby incorporated by
 * reference: www.redbend.com/red-bend-legal-notices.pdf
 *******************************************************************************
 */
/*!
 *******************************************************************************
 * \file	RB_FileSystemUpdate.h
 *
 * \brief	UPI FS Update API
 *******************************************************************************
 */
#ifndef _REDBEND_FILESYSTEM_UPDATE_H_
#define _REDBEND_FILESYSTEM_UPDATE_H_

#include "RB_vRM_Update.h"
#include "RB_vRM_Errors.h"

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */
/*!
 * File access modes
 */
typedef enum tag_RW_TYPE{
	ONLY_R,		//!< Read-only
	ONLY_W,		//!< Write-only
	BOTH_RW		//!< Read-write
}E_RW_TYPE;
	
/*!
 *******************************************************************************
 * Copy file.<p>
 *
 * Must create the path to the new file as required. Must overwrite any contents
 * in the old file, if any. Must check if the source file is a symbolic link.
 * If it is, instead create a new symbolic link using \ref RB_Link.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	strFromPath	Path to old file
 * \param	strToPath	Path to new file
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */

long RB_CopyFile(
	void* pbUserData,
	const char* strFromPath,
	const char* strToPath
);


/*!
 *******************************************************************************
 * Move (rename) file.<p>
 * 
 * Must return error if strFromPath does not exist or if strToPath exists.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	strFromPath	Path to old file location
 * \param	strToPath	Path to new file location
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */

long RB_MoveFile(
	void* pbUserData,
	const char* strFromPath,
	const char* strToPath
);

/*!
 *******************************************************************************
 * Delete file.<p>
 *
 * Must return error if the file does not exist.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	strPath		Path to file
 *
 * \return	S_RB_SUCCESS on success, E_RB_DELETEFILE if the file cannot be
 *			deleted, or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */

long RB_DeleteFile( void* pbUserData, const char* strPath ); 



/*!
 *******************************************************************************
 * Delete folder.<p>
 *
 * Must return success if the folder does not exist.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	strPath		Path to folder
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */

long RB_DeleteFolder( void* pbUserData, const char* strPath ); 



/*!
 *******************************************************************************
 * Create folder.<p>
 *
 * Must return success if the folder already exists. It is
 * recommended that the new folder's attributes are a copy of its parent's
 * attributes. 
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	strPath		Path to folder
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */

long RB_CreateFolder( void* pbUserData, const char* strPath ); 



/*!
 *******************************************************************************
 * Open file.<p>
 *
 * Must create the the file (and the path to the file) if it doesn't exist. Must
 * open in binary mode.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	strPath		Path to file
 * \param	wFlag		Read/write mode, an \ref E_RW_TYPE value
 * \param	pwHandle	(out) File handle
 *
 * \return	S_RB_SUCCESS on success, E_RB_OPENFILE_ONLYR if attempting to open a
 *			non-existant file in R/O mode, E_RB_OPENFILE_WRITE if there is an
 *			error opening a file for writing, or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */

long RB_OpenFile(
	void*					pbUserData,
	const char*	strPath, 
	E_RW_TYPE				wFlag,
	long*					pwHandle
);



/*!
 *******************************************************************************
 * Set file size.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	wHandle		File handle
 * \param	dwSize		New file size
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */

long RB_ResizeFile(
	void* pbUserData,
	long wHandle,
	RB_UINT32 dwSize
);



/*!
 *******************************************************************************
 * Close file.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	wHandle		File handle
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */

long RB_CloseFile(
	void* pbUserData,
	long wHandle
);


	
/*!
 *******************************************************************************
 * Write data to a specified position within a file.<p>
 *
 * Must return success if the block is written or at least resides in
 * non-volatile memory. Use \ref RB_SyncFile to commit the file to storage.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	wHandle		File handle
 * \param	dwPosition	Position within the file to which to write
 * \param	pbBuffer	Data to write
 * \param	dwSize		Size of \a pbBuffer
 *
 * \return	S_RB_SUCCESS on success, or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */

long RB_WriteFile( 
	void*			pbUserData,
	long			wHandle,
	RB_UINT32	dwPosition,
	unsigned char*	pbBuffer,
	RB_UINT32	dwSize
);

/*!
 *******************************************************************************
 * Commit file to storage.<p>
 *
 * Generally called after \ref RB_WriteFile.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	wHandle		File handle
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_SyncFile( 
	void*			pbUserData,
	long			wHandle
);


/*!
 *******************************************************************************
 * Read data from a specified position within a file.
 * If fewer bytes than requested are available in the specified position, this
 * function should read up to the end of file and return S_RB_SUCCESS.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	wHandle		File handle
 * \param	dwPosition	Position within the file from which to read
 * \param	pbBuffer	Buffer to contain data
 * \param	dwSize		Size of data to read
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */

long RB_ReadFile( 
	void*			pbUserData,
	long			wHandle,
	RB_UINT32	dwPosition,
	unsigned char*	pbBuffer,
	RB_UINT32	dwSize
);


/*!
 *******************************************************************************
 * Get file size.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	wHandle		File handle
 *
 * \return	File size, -1 if file not found, or &lt; -1 on error
 *******************************************************************************
 */
long RB_GetFileSize(
	void*	pbUserData,
	long	wHandle
);

/*!
 *******************************************************************************
 * Get free space of a mounted file system.
 *
 * \param	pbUserData				Optional opaque data-structure to pass to
 *									IPL functions
 * \param	path				    Name of any directory within the mounted
 *									file system
 * \param	available_flash_size	(out) Available space
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_GetAvailableFreeSpace(void *pbUserData, const char *path, RB_UINT32 *available_flash_size);

/*!
 *******************************************************************************
 * Remove symbolic link.<p>
 *
 * Must return success if the deleted object does not exist or is not a symbolic link.<p>
 *
 * If your platform does not support symbolic links, you do not need to
 * implement this.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	pLinkName	Path to symbolic link
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_Unlink(void *pbUserData, const char *pLinkName);

/*!
 *******************************************************************************
 * Create symbolic link.<p>
 *
 * Must create the path to the link as required. If a file already exists at the
 * named location, must return success if the file is a symbolic link or an
 * error if the file is a regular file. The non-existance of the target of the
 * link must NOT cause an error.<p>
 *
 * If your platform does not support symbolic links, you do not need to
 * implement this.
 *
 * \param	pbUserData			Optional opaque data-structure to pass to IPL
 *								functions
 * \param	pLinkName			Path to the link file to create
 * \param	pReferenceFileName	Path to which to point the link
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_Link(void *pbUserData, const char *pLinkName, const char *pReferenceFileName);

/*!
 *******************************************************************************
 * Set file attributes.<p>
 *
 * The file attributes token (\a ui8pAttribs) is defined at generation time.
 * If attributes are not defined explicitly, they are given the following, 
 * OS-dependent values:
 * \li	Windows: _redbend_ro_ for R/O files, _redbend_rw_ for R/W files
 * \li	Linux: _redbend_oooooo:xxxx:yyyy indicating the file mode, uid, and gid
 *		(uid and gid use capitalized hex digits as required)
 *
 * \param	pbUserData		Optional opaque data-structure to pass to IPL
 *							functions
 * \param	pFilePath		File path
 * \param	attribSize		Size of \a ui8pAttribs 
 * \param	pAttribs		Attributes to set
 *
 * \return	S_RB_SUCCESS on success or &lt; 0 on error
 *******************************************************************************
 */
long RB_SetFileAttributes(void *pbUserData, const char *pFilePath, const RB_UINT32 attribSize, const unsigned char *pAttribs);


/*!
 *******************************************************************************
 * Print status and debug information.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	aFormat		A NULL-terminated printf-like string with support for
 *						the following tags:
 *						\li %x:  Hex number
 *						\li %0x: Hex number with leading zeros
 *						\li %u:  Unsigned decimal
 *						\li %s:  NULL-terminated string
 * \param	...			Strings to insert in \a aFormat
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
RB_UINT32 RB_Trace(
	void*		pbUserData,
	const char*	aFormat,
	...
);

#ifdef __cplusplus
	}
#endif	/* __cplusplus */

#endif
