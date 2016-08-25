		/*********************************************************
	  **					C O N F I D E N T I A L             **
	 **						  Copyright 2002-2010                **
	  **                         Red Bend Software              **
	   **********************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdarg.h>
#include <fcntl.h>

//Added for Selinux
#include <selinux/selinux.h>
#include <sys/capability.h>
#include <sys/xattr.h>
#include <linux/xattr.h>

//Added for vRM 9.0
#include <limits.h>
#include <string.h>
#include <utime.h>

#include "common.h"

#if 0
#include "MultiFilesApi.h"
#include "RB_ImageUpdate.h"
#include "RB_vRM_Errors.h"
#include "vRM_PublicDefines.h"
#include "vRM_Languages.h"
#include "RB_vRM_ImageUpdate.h"
#endif

#include "RB_ImageUpdate.h"
#include "RB_vRM_Errors.h"
#include "RB_vRM_Update.h"
#include "RB_FileSystemUpdate.h"

//#include "vRM_PublicDefines.h"


#define FS_U_RAM_SIZE	3*1024*1024
#define FS_ID_MAX_LEN 4
#ifndef MAX_PATH
#define MAX_PATH 1024
#endif

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

extern void LOG_INFO(const char *msg, ...);
extern void LOG_ERROR(const char *msg, ...);
extern void LOG_HEX(const char *str, const char *p, int len);
extern void convert_unicode_to_char(const char *src, char *dest);
extern void convert_char_to_unicode(const char *src, unsigned short *dest);

long RB_GetFileType(void *pbUserData, const char *pLinkName, enumFileType *fileType);


/************************************************************
 *                     common functions
 ************************************************************/
long RecursiveFolderCreater(
	const char*	folderpath,
	const		mode_t mode)
{
	int ret = 0;
	char temppath[MAX_PATH] = {'\0'};
	int pathOffset = strlen(folderpath);// For counting back until the '/' delimiter


	LOG_INFO("[%s] path: %s", __FUNCTION__, folderpath);

	if(pathOffset == 0)
		return -1;//if from some reason we got to the end return error!!!.

    while(pathOffset && folderpath[pathOffset] != '/')// get to the next '/' place
        pathOffset--;

	strncpy(temppath, folderpath, pathOffset);// copy one depth below till and without the char '/'
	LOG_INFO("[%s] temppath: %s", __FUNCTION__, temppath);

	ret = mkdir(temppath, mode);
	LOG_INFO("[%s] mkdir result: %d errno: %d", __FUNCTION__, ret, errno);
	if (ret == 0 || ((ret == -1) && (errno == EEXIST)))
	{
		return 0;//meaning the depth creation is success.
	}
	else if((ret == -1) && (errno == ENOENT))
	{
		ret = RecursiveFolderCreater(temppath, mode);
		if (ret == 0)
		{
			ret = mkdir(temppath, mode);
		}
		return ret;
	}
	else
	{
		return -1;
	}

	return 0;
}

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
	void*		pbUserData,
	const char*	strFromPath,
	const char*	strToPath)
{
	FILE* fp1 = NULL;
	FILE* fp2 = NULL;
	unsigned int readCount = 0, writeCount = 0;
	const unsigned long BUFFER_SIZE = 4096; // M{ 10240;
	char buf[BUFFER_SIZE];
	long ret = 0;
	char path1[MAX_PATH] = {'\0'};
	char path2[MAX_PATH] = {'\0'};

	LOG_INFO("[%s] %s %s", __FUNCTION__, strFromPath, strToPath);

	enumFileType fileType = FT_MISSING;

    if (!strFromPath || !strToPath)
    {
        LOG_ERROR("[%s] NULL file name find. Abort.", __FUNCTION__);
        return -1;			//should never happen
    }

    convert_unicode_to_char(strFromPath, path1);
    convert_unicode_to_char(strToPath, path2);
	LOG_INFO("%s: %s -> %s ", __FUNCTION__, path1, path2);

	ret = RB_GetFileType(pbUserData, strFromPath, &fileType);

	if (fileType == FT_SYMBOLIC_LINK)
	{
		char linkedPath[MAX_PATH] = {'\0'};
		ret = readlink (path1, linkedPath, MAX_PATH);
		if (ret < 0)
		{
            LOG_ERROR("[%s] readlink failed with return value: %d", __FUNCTION__, ret);
            return -2;
        }
        if (symlink (linkedPath, path2))
            return -3;
        return 0;
    }

	fp1 = fopen(path1, "r");
	if (!fp1)
	{
		LOG_ERROR("[%s] Open %s ENOENT %d", __FUNCTION__, path1, errno);
		LOG_ERROR("[%s] Open %s failed. Abort.", __FUNCTION__, path1);
		return E_RB_OPENFILE_ONLYR;
	}

	fp2 = fopen(path2, "w");
	if (!fp2)
	{
        //unsigned short shortfolderpath [MAX_PATH];
        char* folder = strrchr(path2,'/');
        int path_len = folder - (char *) &path2;
        //char* folderPath = (char *) malloc(folder - path2 + 1);
        char* folderPath = (char *) malloc(path_len + 1);

        //memset(folderPath,'\0',folder - path2 + 1);
        memset(folderPath,'\0', path_len + 1);
        //strncpy(folderPath,path2,folder - path2);
        strncpy(folderPath, path2, path_len);

		
        //convert_char_to_unicode(folderPath, shortfolderpath);     //Modify for vRM 9.0
        //free(folderPath);							    //Modify for vRM 9.0
        
        if ( RB_CreateFolder(NULL,folderPath) != S_RB_SUCCESS )
        {
            fclose(fp1);
			LOG_ERROR("[%s] Open %s failed. Abort.", __FUNCTION__, path2);
            return E_RB_OPENFILE_WRITE;
        }
        else
        {
            fp2 = fopen(path2, "w");
            if(!fp2)
            {
                fclose(fp1);
                LOG_ERROR("[%s]Open %s failed. Abort.", __FUNCTION__, path2);
                return E_RB_OPENFILE_WRITE;
            }
        }
    }

	while( (readCount = fread(buf, 1, BUFFER_SIZE, fp1)) > 0)
	{
		writeCount = fwrite(buf, 1, readCount, fp2);
        //fflush(fp2);
        //fsync(fileno(fp2));
		if (writeCount != readCount)
		{
			LOG_ERROR("[%s] read %d, but write %d, abort.", __FUNCTION__, readCount, writeCount);
			ret = E_RB_WRITE_ERROR;
 			break;
		}
	}

	fclose(fp1);
    //fflush(fp2);
    //fsync(fileno(fp2));
	fclose(fp2);

	return ret;
}

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
	void*					pbUserData,
	const char*	strPath)
{
	char path[MAX_PATH]={'\0'};
	int ret = 0;


    LOG_INFO("[%s] %s", __FUNCTION__, strPath);

    if (!strPath)
    {
        LOG_ERROR("[%s] NULL file name find. Abort.", __FUNCTION__);
        return -1;
    }

    convert_unicode_to_char(strPath, path);
	LOG_INFO("[%s] %s", __FUNCTION__, path);

	ret = unlink(path);
	LOG_INFO("[%s] unlink value: %d, errno: %d", __FUNCTION__, ret, errno);
	if (ret == 0)
		return S_RB_SUCCESS;

	if (ret < 0 && errno == ENOENT)	//if file does not exist then we can say that we deleted it successfully
		return S_RB_SUCCESS;

        LOG_ERROR("[%s] Can not delete %s", __FUNCTION__,  path);

  	return E_RB_DELETEFILE;
}

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
	void*					pbUserData,
	const char*	strPath)
{
	int ret = 0;
	char path[MAX_PATH]={'\0'};


    LOG_INFO("[%s] %s", __FUNCTION__, strPath);

    if (!strPath)
    {
        LOG_ERROR("[%s] NULL file name find. Abort.", __FUNCTION__);
        return -1;
    }

    convert_unicode_to_char(strPath, path);
    LOG_INFO("[%s] %s", __FUNCTION__, path);

	ret = rmdir(path);
 	LOG_INFO("[%s] rmdir value: %d, errno: %d", __FUNCTION__, ret, errno);

    if ((ret == 0) || ((ret < 0) && ((errno == ENOENT) || (errno == ENOTEMPTY ))))
        return S_RB_SUCCESS;

    return E_RB_FAILURE;
}

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
	void*					pbUserData,
	const char*	strPath)
{
	mode_t mode = 0;
	int ret = 0;
	char path[MAX_PATH] = {'\0'};


    LOG_INFO("[%s] %s", __FUNCTION__, strPath);

	if (!strPath)  {
        LOG_ERROR("[%s] NULL file name find. Abort.", __FUNCTION__);
        return -1;
    }

    convert_unicode_to_char(strPath, path);
    mode =
        S_IRUSR /*Read by owner*/ |
        S_IWUSR /*Write by owner*/ |
        S_IXUSR /*Execute by owner*/ |
        S_IRGRP /*Read by group*/ |
		S_IWGRP /*Write by group*/ |
		S_IXGRP /*Execute by group*/ |
		S_IROTH /*Read by others*/ |
		S_IWOTH /*Write by others*/ |
		S_IXOTH /*Execute by others*/;

    LOG_INFO("[%s] %s, mode:0x%x", __FUNCTION__, path, mode);

	ret = mkdir(path, mode);

	if (ret == 0 || ((ret == -1) && (errno == EEXIST)))
	{
		return S_RB_SUCCESS;
	}
	else if((ret == -1) && (errno == ENOENT))//maybe multi directory problem
	{
		ret = RecursiveFolderCreater(path, mode);
		if(ret == 0)
		{
			ret = mkdir(path, mode);//After creating all the depth Directories we try to create the Original one again.
			if(ret == 0)
				return S_RB_SUCCESS;
			else
				return E_RB_FAILURE;
		}
		else
		{
			return E_RB_FAILURE;
		}
	}
	else
	{
		return E_RB_FAILURE;
	}
}

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
mode_t get_mode(E_RW_TYPE wFlag)
{
    //LOG_INFO("[%s]", __FUNCTION__);

	switch (wFlag)
	{
	case ONLY_R:
		LOG_INFO("[%s] RDONLY", __FUNCTION__);
		return O_RDONLY;
	case ONLY_W:
		LOG_INFO("[%s] WRONLY", __FUNCTION__);
		return O_WRONLY | O_CREAT;
	case BOTH_RW:
		LOG_INFO("[%s] RDWR", __FUNCTION__);
		return O_RDWR | O_CREAT;
    //default:
    //    LOG_INFO("[%s] Unknown", __FUNCTION__);
    //    return 0;
	}

    LOG_INFO("[%s] Unknown", __FUNCTION__);

    return 0;
}

long RB_OpenFile(
	void*					pbUserData,
	const char*				strPath,
	E_RW_TYPE				wFlag,
	long*					pwHandle)
{
	mode_t mode;
	char path[MAX_PATH] = {'\0'};

    LOG_INFO("[%s] %s", __FUNCTION__, strPath);

	if (!strPath)  {
        LOG_ERROR("[%s] NULL file name find. Abort.", __FUNCTION__);
        return -1;
    }

	//RB_FSTrace(0, "%s", strPath); // M??
	convert_unicode_to_char(strPath, path);

	mode = get_mode(wFlag);
	//ui_print("[%s] Path=%s , Mode=%x\n", __FUNCTION__, path, mode);
	LOG_INFO("[%s] Path=%s , Mode=%x", __FUNCTION__, path, mode);

    // fixme
    //if ((mode == (O_WRONLY | O_CREAT)) || (mode == (O_RDWR | O_CREAT)))  {
	//    if (!strcmp(path, "/system/bin/debuggerd"))  {
	//        LOG_INFO("[%s] temp patch", __FUNCTION__);
	//        *pwHandle = 0;
	//        return S_RB_SUCCESS;
	//    }
	//}

	*pwHandle = open(path, mode, 0755);
	if (*pwHandle == -1)
	{
		*pwHandle = 0;
		LOG_ERROR("[%s] First open() with error %d", __FUNCTION__, errno);
        if (wFlag == ONLY_R)
            return E_RB_OPENFILE_ONLYR;

        //if  we need to open the file for write or read/write then we need to create the folder (in case it does not exist)
        if ((wFlag != ONLY_R) && (errno == ENOENT))
        {
			char dir[MAX_PATH] = {'\0'};
			unsigned short dirShort[MAX_PATH] = {'\0'};
			int i = 0;
			//copy the full file path to directory path variable
			while (path[i] != '\0')
			{
				dir[i] = path[i];
				i++;
			}
			LOG_INFO("[%s] copy dir[]=%s", __FUNCTION__, dir);
			//search for the last '/' char
			while (i && (dir[i--] != '/'))
				;
			dir[i+1] = '\0';
			LOG_INFO("[%s] remove dir[]=%s", __FUNCTION__, dir);

			//convert_char_to_unicode((const char*)dir, dirShort);  // modify for vRM9.0
     
			if (RB_CreateFolder(pbUserData, dir))  // modify for vRM 9.0
			{
				LOG_ERROR("[%s] Fail create folder, Leave RB_OpenFile", __FUNCTION__);
				return E_RB_OPENFILE_WRITE;
			}

			*pwHandle = open(path, mode, 0755);
			if (*pwHandle == -1 || *pwHandle == 0)
			{
				*pwHandle = 0;
				LOG_ERROR("[%s] After successful creating folder, fail open() with error %d", __FUNCTION__, errno);
				return E_RB_OPENFILE_WRITE;
			}
		}
 	}
	LOG_INFO("[%s] Successful open %s, *pwHandle=%ld", __FUNCTION__, path, *pwHandle);

    if (!(*pwHandle))  {
	    LOG_ERROR("[%s] Handle=%ld", __FUNCTION__, *pwHandle);
        //return E_RB_BAD_PARAMS;
	}

	errno = 0;

	return S_RB_SUCCESS;
}

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
	void*			pbUserData,
	long			wHandle,
	RB_UINT32		dwSize)
{
	int ret = -1;

	LOG_INFO("[%s] handle=%ld, dwSize=%d", __FUNCTION__, wHandle, dwSize);

	if (wHandle)
		ret = ftruncate(wHandle, dwSize);

	if (ret)  {
	    LOG_ERROR("[%s] fail handle %ld, ret=0x%X, errno=0x%X", __FUNCTION__, wHandle, ret, errno);
		ret = E_RB_RESIZEFILE;
	}

	//LOG_INFO("[%s] ret 0x%X handle %ld %d", __FUNCTION__, ret, wHandle, errno);

	return ret;
}

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
	void*	pbUserData,
	long 	wHandle)
{
	int ret = E_RB_CLOSEFILE_ERROR;

	LOG_INFO("[%s] wHandle = %ld", __FUNCTION__, wHandle);

	//if (!wHandle)  {
	//    LOG_ERROR("[%s] Handle=%ld", __FUNCTION__, wHandle);
	//    // fixme
	//    return S_RB_SUCCESS;
	//}

	if (wHandle >= 0)
	//if (wHandle)
		ret = close(wHandle);

	if (ret == 0)  {
	    LOG_INFO("[%s] OK", __FUNCTION__);
		return S_RB_SUCCESS;
	}

    LOG_INFO("[%s] fail", __FUNCTION__);

	return E_RB_CLOSEFILE_ERROR;
}

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
	RB_UINT32		dwPosition,
	unsigned char*	pbBuffer,
	RB_UINT32		dwSize)
{
	int ret = 0, size = 0;

	LOG_INFO("[%s] Handle=%ld, Pos=0x%X , Size=0x%X", __FUNCTION__, wHandle, dwPosition,dwSize);

	if (!wHandle)  {
	    LOG_ERROR("[%s] Handle=%ld", __FUNCTION__, wHandle);
        return E_RB_BAD_PARAMS;
	}

	size = lseek(wHandle, 0, SEEK_END);
	/* from the guide: if dwPosition is beyond size of file the gap between end-of-file and the position should be filled with 0xff */
	if (size < (int) dwPosition)
	{
		int heap_size = dwPosition - size;
		unsigned char* p_heap = (unsigned char* ) malloc(heap_size);
		memset(p_heap, 0xFF, heap_size);
		ret = write(wHandle, p_heap, heap_size);
		//sync();
		free(p_heap);
		if (ret < 0)
			return E_RB_WRITE_ERROR;
	}
	ret = lseek(wHandle, dwPosition, SEEK_SET);
	if (ret < 0)
	{
		LOG_ERROR("[%s] lseek failed with return value: %d",__FUNCTION__, ret);
		return E_RB_WRITE_ERROR;
	}

	ret = write(wHandle, pbBuffer, dwSize);
	if (ret < 0)
	{
		LOG_ERROR("[%s] Failed with return value: %d", __FUNCTION__, ret);
		return E_RB_WRITE_ERROR;
	}
	LOG_INFO("[%s] Bytes Write: %d", __FUNCTION__, ret);

	ret = fsync(wHandle);
	if (ret < 0)
	{
		LOG_ERROR("[%s] fsync Failed with return value: %d", __FUNCTION__, ret);
		return E_RB_WRITE_ERROR;
	}
	LOG_INFO("[%s] fsync after write: %d", __FUNCTION__, ret);

	return S_RB_SUCCESS;
}

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
	RB_UINT32		dwPosition,
	unsigned char*	pbBuffer,
	RB_UINT32		dwSize)
{
	int ret = 0;

	LOG_INFO("[%s] Handle=%ld , Pos=0x%X , Size=0x%X", __FUNCTION__, wHandle, dwPosition, dwSize);

	if (!wHandle)  {
	    LOG_ERROR("[%s] Handle=%ld", __FUNCTION__, wHandle);
	    // fixme
	    return S_RB_SUCCESS;
        //return E_RB_BAD_PARAMS;
	}

	ret = lseek (wHandle, dwPosition, SEEK_SET);
	if (ret < 0)
	{
		LOG_ERROR("[%s] lseek failed : handle=%ld, ret=0x%X", __FUNCTION__, wHandle, ret);
		return E_RB_READ_ERROR;
	}
	ret = read(wHandle, pbBuffer, dwSize);
	if (ret < 0)
	{
		LOG_ERROR("[%s] read failed : handle=%ld, ret=0x%X", __FUNCTION__, wHandle, ret);
		return E_RB_READ_ERROR;
	}

	//LOG_INFO("[%s] Bytes Read: %d", __FUNCTION__, ret);
	if ((long)ret != (long)dwSize && (((long)ret + (long)dwPosition) != RB_GetFileSize(pbUserData, wHandle)))
		return E_RB_READ_ERROR;

    if (dwSize > 32)  {
        LOG_HEX("[RB_ReadFile]+ ", (const char *) pbBuffer, 16);
        LOG_HEX("[RB_ReadFile]- ", (const char *) (pbBuffer + dwSize - 16), 16);
    } else  {
        LOG_HEX("[RB_ReadFile]= ", (const char *) pbBuffer, dwSize);
    }

	return S_RB_SUCCESS;
}

long RB_GetFileSize(
	void*	pbUserData,
	long	wHandle)
{
	int ret = 0;

	LOG_INFO("[%s] handle=%ld", __FUNCTION__, wHandle);

	if (!wHandle)  {
	    LOG_ERROR("[%s] Handle=%ld", __FUNCTION__, wHandle);
        return E_RB_BAD_PARAMS;
	}

	ret = lseek(wHandle, 0, SEEK_END);

	if (ret == -1)
	{
		LOG_ERROR("[%s] lseek errno: %d", __FUNCTION__, errno);
		return E_RB_READFILE_SIZE;
	}

	LOG_INFO("[%s] Size=0x%lx", __FUNCTION__, ret);

	return ret;
}

long RB_Unlink(
	void*			pbUserData,
	const char*		pLinkName)
{
	int ret = 0;
	char path[MAX_PATH] = {'\0'};
	enumFileType fileType;

	LOG_INFO("[%s]", __FUNCTION__);

	convert_unicode_to_char(pLinkName, path);

	ret = RB_GetFileType(pbUserData, pLinkName, &fileType);
	/*If the specified file does not exist or is not a symbolic link file, this function should return an error.*/
	if (ret != S_RB_SUCCESS || fileType == FT_MISSING || fileType == FT_REGULAR_FILE)
	{
		LOG_ERROR("[%s] cannot unlink file type %d", __FUNCTION__, fileType);
		return E_RB_FAILURE;
	}

	ret = unlink(path);
	if(ret < 0)
	{
		LOG_ERROR("[%s]  unlink failed with return value: %d", __FUNCTION__, ret);
		return E_RB_FAILURE;
	}
	LOG_INFO("[%s] unlink with return value: %d", __FUNCTION__, ret);

	return S_RB_SUCCESS;
}

long RB_VerifyLinkReference(
	void*		pbUserData,
	const char*	pLinkName,
	const char*	pReferenceFileName)
{
	int ret = 0;
	char path[MAX_PATH]={'\0'};
	char linkedpath[MAX_PATH]={'\0'};
	char refPath[MAX_PATH] = { '\0' };


	LOG_INFO("[%s]", __FUNCTION__);

	convert_unicode_to_char(pLinkName, path);
	convert_unicode_to_char(pReferenceFileName, refPath);

	ret = readlink(path, linkedpath, MAX_PATH);
	if (ret < 0)
	{
		LOG_ERROR("[%s] readlink failed with return value: %d", __FUNCTION__, ret);
		return E_RB_FAILURE;
	}

	if ((memcmp(&linkedpath, &refPath, ret))!=0)
	{
		LOG_ERROR("[%s] not same linked path - linkedpath[%s] pReferenceFileName[%s]", __FUNCTION__, linkedpath, refPath);
		return E_RB_FAILURE;
	}
	LOG_INFO("[%s] same linked path", __FUNCTION__);

	return S_RB_SUCCESS;
}

long RB_Link(
	void*			pbUserData,
	const char*		pLinkName,
	const char*		pReferenceFileName)
{
	int ret = 0;
	char sympath[MAX_PATH]={'\0'};
	char refpath[MAX_PATH]={'\0'};
	enumFileType fileType;

	LOG_INFO("[%s]", __FUNCTION__);

	convert_unicode_to_char(pLinkName, sympath);
	convert_unicode_to_char(pReferenceFileName, refpath);

	if(!RB_VerifyLinkReference(pbUserData, pLinkName, pReferenceFileName))
		return S_RB_SUCCESS;

	ret = RB_GetFileType(pbUserData, pLinkName, &fileType);
	/*If a file with the name pLinkName already exists ?		either as a symbolic link file or as regular file, this function should return an error. The existence of the file with the name pReferenceFileName should not be checked*/
	if (ret != S_RB_SUCCESS || (fileType != FT_MISSING && fileType != FT_SYMBOLIC_LINK))
	{
		LOG_ERROR("[%s] get file type failed with file type: %d value %d", __FUNCTION__, fileType, ret);
		return E_RB_FAILED_CREATING_SYMBOLIC_LINK;
	}

	ret = symlink(refpath, sympath);
	if (ret != 0)
	{
		LOG_ERROR("[%s] symlink failed with return value: %d, errno: %d", __FUNCTION__, ret, errno);
		if (errno == EEXIST && RB_VerifyLinkReference(pbUserData, pLinkName, pReferenceFileName))
		{
			return S_RB_SUCCESS;
		}
		return E_RB_FAILED_CREATING_SYMBOLIC_LINK;
	}
	LOG_INFO("[%s] symlink with return value: %d", __FUNCTION__, ret);

	return S_RB_SUCCESS;
}

long RB_GetFileType(
    void*			pbUserData,
    const char*		pLinkName,
	enumFileType*	fileType)
{
	int ret = 0;
	char path[MAX_PATH] = {'\0'};
	struct stat sbuf;


	LOG_INFO("[%s]", __FUNCTION__);

	convert_unicode_to_char(pLinkName, path);

	ret = lstat(path, &sbuf);
	if (ret < 0)
	{
		LOG_INFO("[%s] stat failed with return value=%d, errno=%d : %s", __FUNCTION__, ret, errno, strerror(errno));
		*fileType = FT_MISSING;
		return S_RB_SUCCESS;
	}

	if (S_ISLNK(sbuf.st_mode))
	{
		LOG_INFO("[%s] stat->st_mode = symbolic link file", __FUNCTION__);
		*fileType = FT_SYMBOLIC_LINK;
		return S_RB_SUCCESS;
	}

	if (S_ISREG(sbuf.st_mode))
	{
		LOG_INFO("[%s] stat->st_mode = regular file", __FUNCTION__);
		*fileType = FT_REGULAR_FILE;
		return S_RB_SUCCESS;
	}

	if (S_ISDIR(sbuf.st_mode))
	{
		LOG_INFO("[%s] stat->st_mode = regular file", __FUNCTION__);
		*fileType = FT_FOLDER;
		return S_RB_SUCCESS;
	}

	return E_RB_FAILURE;
}

char a2ch(int value)
{
	char set_value = 0;

	switch(value)
	{
		case '1':
			set_value = 0x01;
			break;
		case '2':
			set_value = 0x02;
			break;
		case '3':
			set_value = 0x03;
			break;
		case '4':
			set_value = 0x04;
			break;
		case '5':
			set_value = 0x05;
			break;
		case '6':
			set_value = 0x06;
			break;
		case '7':
			set_value = 0x07;
			break;
		case '8':
			set_value = 0x08;
			break;
		case '9':
			set_value = 0x09;
			break;
		case '0':
			set_value = 0x00;
			break;
		default:
			LOG_INFO("[%s] : Wrong attribute value: %d", __FUNCTION__, value);
			return 0;

	}
	LOG_INFO("[%s] : %c", __FUNCTION__, set_value);

	return set_value;
}

void chtoa(
	int value,
	char* str)
{
	char *pStr = str;

	LOG_INFO("[%s] : %d", __FUNCTION__, value);

	switch(value)
	{
		case 1:
			*pStr = '1';
			break;
		case 2:
			*pStr = '2';
			break;
		case 3:
			*pStr = '3';
			break;
		case 4:
			*pStr = '4';
			break;
		case 5:
			*pStr = '5';
			break;
		case 6:
			*pStr = '6';
			break;
		case 7:
			*pStr = '7';
			break;
		case 8:
			*pStr = '8';
			break;
		case 9:
			*pStr = '9';
			break;
		case 0:
			*pStr = '0';
			break;
		default:
			LOG_ERROR("[%s] Wrong attribute value: %d", __FUNCTION__, value);
	}
}

#if 0
long RB_SetFileAttributes(void *pbUserData,
				const unsigned short *ui16pFilePath,
				const unsigned long ui32AttribSize,
				const unsigned char *ui8pAttribs)
{
	//const int ATTRSIZE = 25;
	//char tmpAttribs[ATTRSIZE];
	char *tmpAttribs;
	char *tp;
	char *endstr;
	char * rb_sig;

	uid_t setUserID		= 0;
	gid_t setGroupID	= 0;
	mode_t setFileMode	= 0;
	char setFilePath[MAX_PATH]={'\0'};
	struct stat sbuf;
	int ret = 0;
	// debug start
	int count = 0;
	// debug end


	LOG_INFO("[%s]", __FUNCTION__);

	tmpAttribs = (char *) malloc(ui32AttribSize+1);

	if(NULL == ui16pFilePath)
	{
		LOG_ERROR("[%s] ui16pFilePath NULL [error]", __FUNCTION__);
		return E_RB_BAD_PARAMS;
	}
	else if(NULL == ui8pAttribs)
	{
		LOG_ERROR("[%s] ui8pAttribs NULL [error]", __FUNCTION__);
		return E_RB_BAD_PARAMS;
	}

	convert_unicode_to_char(ui16pFilePath, setFilePath);

	ret = lstat(setFilePath, &sbuf);
	if(ret < 0)
	{
        LOG_ERROR("[%s] stat failed with return value: %d", __FUNCTION__, ret);
        return E_RB_FAILURE;
	}
	else
	{
		if(S_ISLNK(sbuf.st_mode))
		{
			LOG_INFO("[%s] stat->st_mode = symbolic link file", __FUNCTION__);
			return S_RB_SUCCESS;
		}
		if(S_ISREG(sbuf.st_mode))
		{
			LOG_INFO("[%s] stat->st_mode = regular file", __FUNCTION__);
		}
		if (S_ISDIR(sbuf.st_mode))
		{
			LOG_INFO("[%s] stat->st_mode = directory", __FUNCTION__);
		}
	}

	if(0 == ui32AttribSize)
	{
		LOG_INFO("[%s] ui32AttribSize 0 [error]", __FUNCTION__);  // M??
		return S_RB_SUCCESS;
	}

	LOG_INFO("[%s] ui16pFilePath = %s", __FUNCTION__, setFilePath);
	LOG_INFO("[%s] ui32AttribSize = %ul", __FUNCTION__, ui32AttribSize);
	LOG_INFO("[%s] ui8pAttribs = %s", __FUNCTION__, ui8pAttribs);
	// debug start
	//for(count=0; count<ATTRSIZE; count++)
	for (count=0; count < (int) ui32AttribSize; count++)
	{
		LOG_INFO("[%s] ui8pAttribs[%d] = %c", __FUNCTION__, count, ui8pAttribs[count]);
	}
	// debug end

	//memset(tmpAttribs, 0x0, ATTRSIZE);
        memset(tmpAttribs, 0x0, (size_t)ui32AttribSize+1);
	memcpy(tmpAttribs, ui8pAttribs, (size_t)ui32AttribSize);

	//Check that the structure is Valid
	if(NULL == strstr(tmpAttribs,"_redbend_"))
		return E_RB_FAILURE;

	tp = strtok((char *) tmpAttribs, ":");

	//Remove the _redbend_ SAFIX
	rb_sig = strrchr(tp,'_');
	rb_sig++;

	// Get FileMode
	setFileMode = strtol(rb_sig, &endstr, 8);
	tp = strtok(NULL, ":");

	// Get UserID
	if (tp != NULL)
	{
		setUserID = (uid_t)strtol(tp, &endstr, 16);
		tp = strtok(NULL, ":");
	}

	// Get GroupID
	if (tp != NULL)
	{
		setGroupID = (gid_t)strtol(tp, &endstr, 16);
	}

	// Set FileMode
	LOG_INFO("[%s] setFilePath = %s", __FUNCTION__, setFilePath);
	LOG_INFO("[%s] setFileMode = %d", __FUNCTION__, setFileMode);
	LOG_INFO("[%s] setFileMode = %o", __FUNCTION__, setFileMode);
	LOG_INFO("[%s] setUserID = %d", __FUNCTION__, setUserID);
	LOG_INFO("[%s] setGroupID = %d", __FUNCTION__, setGroupID);
	if( chmod(setFilePath, setFileMode) )
	{
        LOG_ERROR("[%s] chmod error", __FUNCTION__);
        return E_RB_FAILURE;
	}

	// Set UserID,GroupID
	if( chown(setFilePath, setUserID, setGroupID) )
	{
		LOG_ERROR("[%s] chown error", __FUNCTION__);
		// debug start
		LOG_ERROR("[%s] setUserID = %d", __FUNCTION__, setUserID);
		LOG_ERROR("[%s] setGroupID = %d", __FUNCTION__, setGroupID);
		LOG_ERROR("[%s] chown errno = %d", __FUNCTION__, errno);
		// debug end
		return E_RB_FAILURE;
	}

	free(tmpAttribs);

	LOG_INFO("[%s] SUCCESS", __FUNCTION__);
	return S_RB_SUCCESS;
}
#endif



static int hex_digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else
		return -1;
}

#define ISOCTAL(v) ((v) && (v) >= '0' && (v) <= '7')
static int xattr_text_decode(const char *value, char *decoded)
{
	char *d = decoded, *v = (char*)value;

	while(*v)
	{
		if (v[0] == '\\')
		{
			if (v[1] == '\\' || v[1] == '\"')
			{
				*d++ = *++v; v++;
			}
			else if (ISOCTAL(v[1]) &&
				ISOCTAL(v[2]) && ISOCTAL(v[3]))
			{
				*d++ = ((v[1] - '0') << 6) +
					   ((v[2] - '0') << 3) +
					   ((v[3] - '0'));
				v += 4;
				
			}
			else
				return -1;
		}
		else
			*d++ = *v++;
	}
	return (d-decoded);
}

static int xattr_hex_decode(const char *value, char *decoded)
{
	char *d = decoded, *end, *v = (char*)value;
	end = (char*)value + strlen(value);

	while (v < end)
	{
		int d1, d0;
		d1 = hex_digit(*v++);
		if (!*v)
			return -1;
		d0 = hex_digit(*v++);
		if (d1 < 0 || d0 < 0)
			return -1;

		*d++ = ((d1 << 4) | d0);
	}
	return (d - decoded);
}

#define _REDBEND_PREFIX "_redbend_"
//#if !defined(ANDROID) || defined(ANDROID_KK)
#if 1
static int removeXattr(const char *fileName)
{
	char listXattr[1024];
	char *curXattr = NULL;
	int xattrRet = llistxattr(fileName, listXattr, 1024);

	if (xattrRet < 0)
		return -1;
	curXattr = listXattr;
	while (curXattr < listXattr + xattrRet )
	{
		if (strcmp(curXattr, "security.selinux") &&
			lremovexattr(fileName, curXattr) < 0)
		{
			//printf_err("Error - lremovexattr failed for %s. Errno %d\n",fileName, errno);
			LOG_ERROR("[%s] Error - lremovexattr failed for %s. Errno %d\n", __FUNCTION__, fileName, errno);
			return -1;
		}
		curXattr += strlen(curXattr) + 1;
	}
	return 0;
}
#endif
static int xattrDecode (char *encoded, char *decoded, size_t *dsz)
{
		size_t osz = strlen(encoded);

		if (encoded[0]=='"' && encoded[osz-1] == '"')
		{
			encoded[osz-1] = '\0';
			*dsz = xattr_text_decode(++encoded, decoded);
		}
		else if (encoded[0] == '0' && encoded[1] == 'x')
			*dsz = xattr_hex_decode(encoded+2, decoded);
		else
		{
			printf("Error - Invalid encoded\n");
			return -1;
		}
		if (*dsz == (size_t)-1)
		{
			printf("Error - Invalid encoded\n");
			return -1;
		}
		decoded[*dsz] = '\0';
		return 0;
}

long RB_SetFileAttributes(
	void*				 pbUserData,
	const char*			 FilePath,
	const unsigned int   ui32AttribSize,
	const unsigned char* ui8pAttribs)
{
	int mode, uid, gid, i;
	struct stat sbuf;
	int ret = E_RB_FAILURE;
	// debug start
	int count = 0;
	char *localAttrib = NULL, *decoded_val = NULL, *decoded_key = NULL;
	char *pattr;
	
	// Skip the redbend prefix
	if (!strncmp((char *)ui8pAttribs, _REDBEND_PREFIX, sizeof(_REDBEND_PREFIX)-1))
		ui8pAttribs += sizeof(_REDBEND_PREFIX)-1;

	count = sscanf((char *)ui8pAttribs, "%6o:%4x:%4x",
			(unsigned int*)&mode, &uid, &gid);

	// Maybe _redbend_ro/w
	if (count != 3)
	{
		char s[3];
		count = sscanf((char *)ui8pAttribs, "%2s", s);
		if (count == 1 && (!strcmp(s, "ro") || !strcmp(s, "rw"))) 
		{
			ret = S_RB_SUCCESS; // Just ignored
		}
		goto End;
	}

	if (lstat(FilePath, &sbuf))
	{	
		printf("Error - lstat failed. Errno %d\n", errno);
		goto End;
	}

	if(lchown(FilePath, (uid_t)uid, (gid_t)gid))
	{
		printf("Error - lchown failed. Errno %d\n", errno);
		goto End;
	}

	if (!S_ISLNK(sbuf.st_mode) && chmod(FilePath, (mode_t)mode))	
	{
		printf("Error - chmod failed. Errno %d\n", errno);
		goto End;
	}

	localAttrib = strdup((char*)ui8pAttribs);
	for (i = 1, pattr = strtok(localAttrib, ":"); 
		pattr && (i < 3); i++)
	{
		ui8pAttribs += strlen(pattr) + 1; // 1 for colon
		pattr = strtok(NULL, ":");
	}
	if (i < 3)
	{
		printf("Bad format for attributes %s\n", ui8pAttribs);
		goto End;
	}
	if (pattr)
		ui8pAttribs += strlen(pattr);

	if (ui8pAttribs[0] != ':')
	{
		ret = S_RB_SUCCESS;
		goto End;
	}
	ui8pAttribs++;
//#if !defined(ANDROID) || defined(ANDROID_KK)
#if 1
	if (removeXattr(FilePath) < 0)
	{
		printf("Failed to remove all xattr for %s\n", FilePath);
		goto End;
	}
#endif
    if (ui8pAttribs[0])
    {
		char *saveptr, *xattr;
		if (localAttrib)
			free(localAttrib);
		localAttrib = strdup((char*)ui8pAttribs);
		decoded_val = (char*)malloc(XATTR_SIZE_MAX);
		decoded_key = (char*)malloc(XATTR_SIZE_MAX);
		if (!localAttrib || !decoded_val || !decoded_key)
		{
			printf("Error - Unable to allocated memory for xattr\n");
			goto End;
		}

		for (xattr = strtok_r((char*)localAttrib, ";", &saveptr); xattr;
				xattr = strtok_r(NULL, ";", &saveptr))
		{
			char *key, *value;
			size_t dsz;
			key = strtok(xattr, "=");
			value = xattr + strlen(key) + 1;
			if (xattrDecode(key, decoded_key, &dsz))
				goto End;
			if (xattrDecode(value, decoded_val, &dsz))
				goto End;

//#if !defined(ANDROID) || defined(ANDROID_KK)
#if 1
			if (lsetxattr(FilePath, decoded_key, decoded_val, dsz, 0) < 0)
			{
				printf("Error - lsetxattr failed. Errno %d\n", errno);
				goto End;
			}
#else
			printf("lsetxattr is not defined, skipping %s=%s\n", key, value);
#endif
		}
    }

	ret = S_RB_SUCCESS;

End:
	if (localAttrib)
		free(localAttrib);

	if (decoded_val)
		free(decoded_val);
	if (decoded_key)
		free(decoded_key);

	if(ret != S_RB_SUCCESS)
	{
		printf ("RB_SetFileAttributes failed. %s %s\n",	FilePath, ui8pAttribs);
	}
	else
	{
		printf("RB_SetFileAttributes %s: %o %d %d\n", FilePath, mode, uid, gid);
	}
	return ret;
}

long RB_CompareFileAttributes(
	void*			pbUserData,
	unsigned short*	pFilePath,
	unsigned char*	pAdditionalAttribs,
	unsigned long	iAddiInfoSize)
{
    LOG_INFO("[%s]", __FUNCTION__);
	return S_RB_SUCCESS;
}

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
 *							%fx - hex number with leading spaces
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
	void*					pUser,
	const unsigned short*	aFormat,
	...)
{
    int err = errno;
    va_list args;
    va_start(args, aFormat);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), (const char *) aFormat, args);
    va_end(args);

    fprintf(stdout, "[%s] %s", __FUNCTION__, buf);
    fflush(stdout);

    return S_RB_SUCCESS;
}

long RB_MoveFile(
	void* pbUserData,
	const char* strFromPath,
	const char* strToPath)
{
    long ret = 0;
    long pwHandle = 0;
    char path1[MAX_PATH]={'\0'};
    char path2[MAX_PATH]={'\0'};

    convert_unicode_to_char(strFromPath, path1);
    convert_unicode_to_char(strToPath, path2);

	LOG_INFO ("%s: %s -> %s ", __FUNCTION__, path1, path2);

	if (!strFromPath || !strToPath)
	{
		LOG_INFO("NULL file name find. Abort.\n");
		return -1;			//should never happen
	}

	if (RB_OpenFile(pbUserData, strToPath, ONLY_R, &pwHandle) == S_RB_SUCCESS)
	{
		RB_CloseFile(pbUserData, pwHandle);
		return S_RB_SUCCESS;
	}

	ret = rename (path1,path2);
	if (ret < 0)
	{
			LOG_INFO ("failed to rename file %s: %s -> %s ", __FUNCTION__, path1, path2);
			return -2;
	}

	return S_RB_SUCCESS;
}

long RB_SyncFile(
	void*	pbUserData,
	long	wHandle)
{
	long ret = -1;
	ret = fsync(wHandle);
	if (ret < 0)
	{
		LOG_INFO("fsync Failed with return value: %d\n",ret);
		return E_RB_WRITE_ERROR;
	}
	LOG_INFO("fsync after write: %d\n",ret);

	return S_RB_SUCCESS;
}

//long RB_ResetTimerA(void)
//{
//    LOG_INFO("%s \n", __FUNCTION__);
//    return S_RB_SUCCESS;
//}

///////////////////
// Not In Spec
long RB_SetFileReadOnlyAttr(
	void*					pbUserData,
	const unsigned short*	strPath,
	unsigned long			wReadOnlyAttr
)
{
    LOG_INFO("[%s]", __FUNCTION__);
    return 0;
}

#ifdef __cplusplus
}
#endif	/* __cplusplus */
