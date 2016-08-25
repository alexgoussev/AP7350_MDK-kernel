#ifndef _REDBEND_MULTI_UPDATE_H_
#define _REDBEND_MULTI_UPDATE_H_

	   /*********************************************************
	  **					C O N F I D E N T I A L             **
	 **						  Copyright 2002-2011                **
	  **                       Red Bend Software                **
	   **********************************************************/

/*
 *   Part of Red Bend's API definition for the UPdate Installer
 */


/*!
 ************************************************************
 *  
 *
 * @brief
 *	Redbend Interface for the Mutil files update product.
 *
 *
 *	Below you can find the API for the basic IO operation
 *  that needs to be implemented by the customer as a glue layer
 *  between the Redbend library and the customer Operating & File Systems
 *  and the library entry point API that should be called in order
 *  to invoke the update procedure.
 *  
 *	Glue Layer Operations:
 *
 *	1. RB_OpenFile		- Open any given file in the file system
 *	2. RB_CloseFile		- Close an open file in the file system
 *	3. RB_WriteFile		- Writes block of data to an open file
 *	4. RB_ReadFile		- Reads any size of data from an open file
 *	5. RB_FSTrace		- Send any kind of indication to the application.
 *	6. RB_FSProgress	- An update procedure indicator
 *
 *	The above Operation should be implememnted by the customer.
 *
 *	Invocation API:
 *		RB_FileSystemUpdate - Should be invoked be the customer's 
 *                       application, provided with system parameters
 *     
 *
 *  @note
 *  All the operations that are part of the glue layer
 *  has the ability to get a user defined structure that may
 *  contain any user defined data or pointer to functions.
 *  The library code doesn't do any use with it. 
 *  It flexable, and allow the user the freedom to implement
 *  the glue functions according to his needs.
 *  
 *  The calling application creates the data struct and supply,
 *  it to RB_FileSystemUpdate when it decides to invoke the update procedure.
 *
 ************************************************************
 */


#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 ************************************************************
 *               STRUCTURES DECLARATION
 ************************************************************
 */

/*!
 ************************************************************
 *                     E_RW_TYPE
 ************************************************************
 *
 * @brief
 *	File Access modes. For use by RB_OpenFile.
 *
 *
 ************************************************************
 */
	
typedef enum tag_RW_TYPE{
	ONLY_R,
	ONLY_W,
	BOTH_RW
}E_RW_TYPE;
	



/*
 ************************************************************
 *               GLUE API DECLARATION
 ************************************************************
 */

//get the version string
long RB_GetUPIVersion(unsigned char* pbVersion);

/*!
 ************************************************************
 *                     RB_CopyFile
 ************************************************************
 *
 * @brief
 *	This function copies a file to another file with a different name or path.
 *
 *
 *	@param pbUserData	Any user data structure, that may be useful for the user
 *						implementation If not needed set to NULL.
 *						The calling function supply you with the user data,
 *						previously supplied in the RB_FileSystemUpdate.
 *
 *	@param strFromPath	The path where the file exist.
 *	
 *	@param strToPath	New destination of the file.
 *
 *	@return				One of the return codes as defined in RbErrors.h
 *
 ************************************************************ 
 */

long RB_CopyFile(
	void* pbUserData,
	const unsigned short* strFromPath,
	const unsigned short* strToPath
);


/*!
 ************************************************************
 *                     RB_MoveFile
 ************************************************************
 *
 * @brief
 *	This function move a file to another path or name.
 *
 *
 *	@param pbUserData	Any user data structure, that may be useful for the user
 *						implementation If not needed set to NULL.
 *						The calling function supply you with the user data,
 *						previously supplied in the RB_FileSystemUpdate.
 *
 *	@param strFromPath	The path where the file exist.
 *	
 *	@param strToPath	New destination of the file.
 *
 *	@return				One of the return codes as defined in RbErrors.h
 *
 ************************************************************ 
 */

long RB_MoveFile(
	void* pbUserData,
	const unsigned short* strFromPath,
	const unsigned short* strToPath
);

/*!
 ************************************************************
 *                     RB_DeleteFile
 ************************************************************
 *
 * @brief
 *	This function deletes a specified file and removes it from the File System.
 *
 *
 *	@param pbUserData	Any user data structure, that may be useful for the user
 *						implementation If not needed set to NULL.
 *						The calling function supply you with the user data,
 *						previously supplied in the RB_FileSystemUpdate.
 *
 *	@param strPath		The path of the file.
 *
 *	@return				One of the return codes as defined in RbErrors.h
 *
 ************************************************************ 
 */

long RB_DeleteFile(
	void* pbUserData,
	const unsigned short* strPath
);



/*!
 ************************************************************
 *                     RB_DeleteFolder
 ************************************************************
 *
 * @brief
 *	This function deletes a specified folder and removes it from the File System.
 *
 *
 *	@param pbUserData	Any user data structure, that may be useful for the user
 *						implementation If not needed set to NULL.
 *						The calling function supply you with the user data,
 *						previously supplied in the RB_FileSystemUpdate.
 *
 *	@param strPath		The path of the folder.
 *
 *	@return				One of the return codes as defined in RbErrors.h
 *
 ************************************************************ 
 */

long RB_DeleteFolder(
	void* pbUserData,
	const unsigned short* strPath
);



/*!
 ************************************************************
 *                     RB_CreateFolder
 ************************************************************
 *
 * @brief
 *	This function deletes a specified folder and removes it from the File System.
 *
 *
 *	@param pbUserData	Any user data structure, that may be useful for the user
 *						implementation If not needed set to NULL.
 *						The calling function supply you with the user data,
 *						previously supplied in the RB_FileSystemUpdate.
 *
 *	@param strPath		The path of the folder.
 *
 *	@return				One of the return codes as defined in RbErrors.h
 *
 ************************************************************ 
 */

long RB_CreateFolder(
	void* pbUserData,
	const unsigned short* strPath
);



/*!
 ************************************************************
 *                     RB_OpenFile
 ************************************************************
 *
 * @brief
 *	Opens a file in the file system.
 *
 *	A glue function that needs to be implemented by the customer.
 *
 *	It should follow the following restrictions:
 *
 * 1. Returning the proper error level \see RbErrors.h 
 *
 *
 *	@param pbUserData	Any user data structure, that may be useful for the user
 *						implementation If not needed set to NULL.
 *						The calling function supply you with the user data,
 *						previously supplied in the RB_FileSystemUpdate.
 *
 *	@param strPath		An absolute path to the file location in the FS.
 *	
 *	@param wFlag		Controls the access mode read, write or both.
 *						opens a file to write deletes the file content.
 *
 *	@param pwHandle		A handle to the file. 
 *
 *	@return				One of the return codes as defined in RbErrors.h
 *
 ************************************************************ 
 */

long RB_OpenFile(
	void*					pbUserData,
	const unsigned short*	strPath, 
	E_RW_TYPE				wFlag,
	long*					pwHandle
);



/*!
 ************************************************************
 *                     RB_ResizeFile
 ************************************************************
 *
 * @brief
 *	set the size of a file in the file system.
 * 
 *	@param pbUserData	Any user data structure, that may be useful for the user
 *						implementation If not needed set to NULL.
 *						The calling function supply you with the user data,
 *						previously supplied in the RB_FileSystemUpdate.
 *
 *	@param wHandle		A handle to the file.
 *
 *	@param dwSize		The new size of the file.
 *
 *	@return				One of the return codes as defined in RbErrors.h
 *
 ************************************************************ 
 */

long RB_ResizeFile(
	void* pbUserData,
	long wHandle,
	unsigned long dwSize
);



/*!
 ************************************************************
 *                     RB_CloseFile
 ************************************************************
 *
 * @brief
 *	Close a file in the file system.
 * 
 *	A glue function that needs to be implemented by the customer.
 *
 *	It should follow the following restrictions:
 *
 * 1. Returning the proper error level \see RbErrors.h
 *
 *
 *	@param pbUserData	Any user data structure, that may be useful for the user
 *						implementation, if not needed set to NULL.
 *						The calling function supply you with the user data,
 *						previously supplied in the RB_FileSystemUpdate.
 *
 *	@param wHandle		A handle to the file.
 *
 *	@return				One of the return codes as defined in RbErrors.h
 *
 ************************************************************ 
 */

long RB_CloseFile(
	void* pbUserData,
	long wHandle
);


	
/*!
 ************************************************************
 *                     RB_WriteFile
 ************************************************************
 *
 * @brief
 *	Writes block of data to an open file in a reliable manner.
 *
 *	A glue function that needs to be implemented by the customer.
 *
 *	It should follow the following restrictions:
 *
 *	1. Returning the proper error level \see RbErrors.h
 *	2. The writing procedure should be a transaction.
 *		In case of returning successfully after writing a block means that 
 *		the block has been written to its target location, or at least resides
 *		in a NV memory, and an automatic procedure will restore it to its target 
 *		location. e.g. a power fail right after returning from the function invocation.
 *
 *	@param pbUserData		Any user data structure, that may be useful for the user
 *							implementation, if not needed set to NULL.
 *							The calling function supply you with the user data,
 *							previously supplied in the RB_FileSystemUpdate.
 *
 *	@param wHandle			Handle to the file.
 *
 *	@param dwPosition		Position were to write
 *	
 *	@param pbBuffer			The block of data that should be written.
 *
 *	@param dwSize			The size in bytes of the block to be written. 
 *
 *	@return					One of the return codes as defined in RbErrors.h
 *
 ************************************************************ 
 */

long RB_WriteFile( 
	void*			pbUserData,
	long			wHandle,
	unsigned long	dwPosition,
	unsigned char*	pbBuffer,
	unsigned long	dwSize
);

/*!
 ************************************************************
 *                     RB_SyncFile
 ************************************************************
 *
 * @brief
 *	Synchronize a file's in-core state with storage device .
 *
 *	A glue function that needs to be implemented by the customer.
 *
 *	@param pbUserData		Any user data structure, that may be useful for the user
 *							implementation, if not needed set to NULL.
 *							The calling function supply you with the user data,
 *							previously supplied in the RB_FileSystemUpdate.
 *
 *	@param wHandle			Handle to the file.
 *
 *	@return					One of the return codes as defined in RbErrors.h
 *
 ************************************************************ 
 */
long RB_SyncFile( 
	void*			pbUserData,
	long			wHandle
);


/*!
 ************************************************************
 *                     RB_ReadFile
 ************************************************************
 *
 * @brief
 *	Reads data from an open file.
 *
 *	A glue function that needs to be implemented by the customer.
 *
 *	It should follow the following restrictions:
 *
 *	1. Returning the proper error level \see RbErrors.h
 *
 *	@param pbUserData	Any user data structure, that may be useful for the user
 *						implementation, if not needed set to NULL.
 *						The calling function supply you with the user data,
 *						previously supplied in the RB_FileSystemUpdate.
 *
 *	@param wHandle		Handle to the file.
 *
 *	@param dwPosition	The offset in the read file that should be 
 *						the starting point for the copy.
 *	
 *	@param pbBuffer		The gives buffer that the data from the open file should be.
 *						copy into.
 *
 *	@param dwSize		The size in bytes that should be copied from the open file,
 *						starting from the given position offset. 
 *
 *	@return				One of the return codes as defined in RbErrors.h
 *
 ************************************************************ 
 */

long RB_ReadFile( 
	void*			pbUserData,
	long			wHandle,
	unsigned long	dwPosition,
	unsigned char*	pbBuffer,
	unsigned long	dwSize
);


/*!
 ************************************************************
 *                     RB_GetFileSize
 ************************************************************
 *
 * @brief
 *	Returns the file size
 *
 *	A glue function that needs to be implemented by the customer.
 *
 *	@param pbUserData		Any user data structure, that may be useful for the user
 *							implementation, if not needed set to NULL.
 *							The calling function supply you with the user data,
 *							previously supplied in the RB_FileSystemUpdate.
 *
 *	@param wHandle			Handle to the file.
 *
 *	@return					File size or one of the return codes as defined in RbErrors.h
 *
 ************************************************************ 
 */
long RB_GetFileSize(
	void*	pbUserData,
	long	wHandle
);



/*!
 ************************************************************
 *                     RB_FSTrace
 ************************************************************
 *
 * @brief
 *	Generic log
 *
 *	A glue function that needs to be implemented by the customer.
 *	Gives the Update procedure the ability to send out status indications and debug
 *	information.
 *
 *	@param pbUserData	Any user data structure, that may be useful for the user
 *						implementation, if not needed set to NULL.
 *						The calling function supply you with the user data,
 *						previously supplied in the RB_FileSystemUpdate.
 *
 *	@param aFormat		a NULL terminated string that support a subset of the known 
 *						standard c library printf.
 *						Supports:
 *							%x - hex number
 *							%0x - hex number with leading zeros
 *							%u - unsigned decimal
 *							%s - null terminated string
 *
 *	@param ...			List of the format corresponding variable
 *
 *	@return				One of the return codes as defined in RbErrors.h
 *
 ************************************************************ 
 */

long RB_FSTrace(
	void*					pbUserData, 
	const unsigned short*	aFormat,
	...
);

/*!
 ************************************************************
 *                     RB_FSTrace
 ************************************************************
 *
 * @brief
 *	Generic log
 *
 *	A glue function that needs to be implemented by the customer.
 *	Gives the Update procedure the ability to send out status indications and debug
 *	information.
 *
 *	@param pbUserData	Any user data structure, that may be useful for the user
 *						implementation, if not needed set to NULL.
 *						The calling function supply you with the user data,
 *						previously supplied in the RB_FileSystemUpdate.
 *
 *	@param aFormat		a NULL terminated string that support a subset of the known 
 *						standard c library printf.
 *						Supports:
 *							%x - hex number
 *							%0x - hex number with leading zeros
 *							%u - unsigned decimal
 *							%s - null terminated string
 *
 *	@param ...			List of the format corresponding variable
 *
 *	@return				One of the return codes as defined in RbErrors.h
 *
 ************************************************************ 
 */
unsigned long RB_Trace(
	void*		pbUserData,
	const char*	aFormat,
	...
);


/*
 ************************************************************
 *               REDBEND UPDATE API DECLARATION
 ************************************************************
 */


/*! 
 ************************************************************
 *                     RB_FileSystemUpdate
 ************************************************************
 *
 * @brief
 *	The Update procedure invoker
 *
 *	Call this function with the proper parameters in order to run the Update
 *	procedure.
 * 
 *	@param pbUserData			Any user data structure, that may be useful for the user
 *								implementation, if not needed set to NULL.
 *								While in run time the Update procedure supply this given 
 *								structure to the caller such as RB_OpenFile, RB_CloseFile etc.
 *
 *	@param strRootSourcePath	Pointer to a NULL-terminated Unicode (wide char) string 
 *								with the root directory of the File System to be updated.
 *
 *	@param strRootTargetPath	Pointer to a NULL-terminated Unicode (wide char) string 
 *								with the root directory where the updated File System will 
 *								be created. Can be the same as strRootSourcePath.
 *
 *	@param strTempPath			Pointer to a NULL-terminated Unicode (wide char) string with 
 *								the directory that will be used for temporary files during the update process.
 *
 *	@param strDeltaPath			If the File System delta-update is stored in the file system, 
 *								this parameter should hold a pointer to a NULL-terminated 
 *								Unicode (wide char) string with the full path of the delta-update file.
 *
 *	@param pbyRAM				Pointer to the RAM area that is used for FS update process.
 *
 *	@param dwRAMSize			Size of available RAM to be used by UPI.
 *
 *	@operation					Type of operation for FS UPI to preform.
 *
 *	@return				One of the return codes as defined in RbErrors.h
 * 
 ************************************************************ 
 */


long
RB_FileSystemUpdate(
	void*					pbUserData,
	const unsigned short*	strRootSourcePath,
	const unsigned short*	strRootTargetPath,
	const unsigned short*	strTempPath,
	const unsigned short*	strPartitionName,
	unsigned char*			pbyRAM, 
	unsigned long			dwRAMSize,
	unsigned long			wOperation
);


#ifdef __cplusplus
	}
#endif	/* __cplusplus */

#endif
