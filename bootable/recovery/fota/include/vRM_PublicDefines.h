// vRM_PublicDefines.h
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __VRM_PUBLICDEFINES_H__
#define __VRM_PUBLICDEFINES_H__

    /*********************************************************
     **               C O N F I D E N T I A L               **
     **                  Copyright ® 2010                   **
     **    Red Bend Software Inc.    All rights reserved    **
     *********************************************************/ 

// amount of RAM that should be given to RB_vRM_GetDpRamUse
#define RAM_SIZE_FOR_GET_DP_RAM_USE 0x100

typedef enum  
{	
	CDT_UPDATE  = 100,
	CDT_INSTALL = 101,
	CDT_REMOVE  = 102
} CompDeltaType;

typedef enum  
{	
	FS_CRAMFS,
	FS_SQUASHFS,
	FS_ROFS,
	FS_ROFX,
	FS_SYMBIAN_ROM,
	FS_MONOLITH,
	FS_SYMBIAN_RW,
	FS_MONOLITH_DUMMY,
	FS_JOURNALING_RW,
	FS_JOURNALING_RW_CBSU,
	FS_RW_FAT,
	FS_CAB_RECREATION
} PartitionFileSystemType;


/*
typedef enum  
{
	PT_FOTA, 
	PT_FS, 
	PT_MODULAR
} PartitionType;
*/

typedef enum  
{
	ROM_TYPE_NOR, 
	ROM_TYPE_NAND, 
	ROM_TYPE_READ_WRITE, 
	ROM_TYPE_READ_WRITE_STATICALLY_LINKED,
	ROM_TYPE_EMPTY,
	ROM_TYPE_FLATTENED_DUMMY
} RomType;


// the enum describes the image type 
// from historical reasons it is called CompressionType, although it
// enumerates also types like APK and ABI which are image types. 
typedef enum 
{
	ZLIB_CRAMFS,
	ZLIB_ROFS,
	DEFLATE_S = ZLIB_ROFS,
	NO_COMPRESSION,
	BYTE_PAIR,
	ZLIB ,
	SE_LNG , 
	EXT_COMP, 
	APK,
	ABI,
    EXT_LFS
} CompressionType;


typedef enum 
{
	UT_SELF_UPDATE=0,
	UT_NO_SELF_UPDATE
} UpdateType;

typedef enum 
{
	FT_REGULAR_FILE, 
	FT_SYMBOLIC_LINK,
	FT_FOLDER,
	FT_MISSING 
} enumFileType;


#endif // __VRM_PUBLICDEFINES_H__
