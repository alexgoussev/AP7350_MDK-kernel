#ifndef _RB_VRM_IMAGE_UPDATE_H
#define _RB_VRM_IMAGE_UPDATE_H


    /*********************************************************
     **               C O N F I D E N T I A L               **
     **                  Copyright ® 2010                   **
     **    Red Bend Software Inc.    All rights reserved    **
     *********************************************************/ 

#include "vRM_PublicDefines.h"
#include "vRM_Languages.h"

#ifdef __cplusplus
extern "C" {
#endif



typedef struct tagCustomerPartitionData
{
	unsigned short *partition_name;		// unique identifier for the partition which must comply with information given to the generator or CMS
	unsigned short *base_partition_name;   // the name of the ROFS to which ROFX relates
	unsigned long sector_size;			// erase sector size
	unsigned long page_size;			// write page size
	unsigned long rom_start_address;	// address in flash of the partition beginning
	unsigned long rom_end_address;		// address in flash of the partition ending

	// offset from the beginning of the partition of the main flash area of the directory tree (the area that the
	// operating systems starts reading the directory tree from)
	unsigned long dir_tree_offset; 

	unsigned short *mount_point;		// NULL terminated string representing the mount point of the partition on the device 
										// or its drive letter
	unsigned short *ui16StrSourcePath;	// NULL terminated string representing the source path to update. 
										// This path will be concatenated to the mount point of the partition.
	unsigned short *ui16StrTargetPath;	// NULL terminated string representing the target path where the updated files will be written.
										// This path will be concatenated to the mount point of the partition.
	unsigned short *ui16StrSourceFileAttr; // NULL terminated string representing the source file attributes
	unsigned short *ui16StrTargetFileAttr; // NULL terminated string representing the target file attributes

	PartitionType			partition_type;		// type of the partition (FOTA, File System, modular)
	PartitionFileSystemType file_system_type;	// the file system used on the partition
	RomType					rom_type;			// the type of ROM used on the partition
	CompressionType			compression_type;	// compression type of files on the partition
	unsigned long updated;			// output parameter: has the value 1 if the partition contains file 
									// deltas that are relevant to the current update with the given 
									// installer types and update type
} CustomerPartitionData;


typedef struct tag_vRM_DeviceData
{
	unsigned long	ui32Operation;			// scout only | scout and update | update only
	unsigned long	ui32DeviceCaseSensitive;// set 1 for case sensitive file system and 0 otherwise 
	unsigned char	*pRam;					// pointer to pre-allocated RAM space for the UPI usage (if 0 the UPI will allocate using RB_malloc)
	unsigned long	ui32RamSize;			// size of the RAM allocated or to be allocated for the UPI usage
	unsigned long	ui32NumberOfBuffers;	// the number of the used Backup Buffers
	unsigned long	*pBufferBlocks;			// vectors of Backup Buffers addresses
	unsigned long	ui32NumberOfPartitions;	// the number of partition on this device
	CustomerPartitionData  *pFirstPartitionData; // here are the partitions data for the handset
	unsigned long	ui32NumberOfLangs;		// number of items in pLanguages
	vRM_Languages	*pLanguages;			// languages supported by the device
	unsigned short	*pTempPath;				// a null terminated temporary path available for the UPI usage
	unsigned char	**pSupplementaryInfo;	// *pSupplementaryInfo will be set by the UPI to be a pointer to a buffer of supplementary info to be sent to the DM server
	unsigned long	*pSupplementaryInfoSize;	// pointer to an integer where the UPI can store the size of the supplementary info for the DM server
	unsigned long	*pComponentInstallerTypes; 
	unsigned long	ui32ComponentInstallerTypesNum;
	unsigned long   ui32ComponentUpdateFlags;// uses for matching component
	UpdateType		enmUpdateType;	
	unsigned long	ui32Flags;				// various parameter flags, according to the flags masks.
	unsigned long	ui32OrdinalToUpdate;	// ordinal of delta to be updated
	
	// relevant for a File-System update only
	unsigned short	*pDeltaPath;			// a null terminated path of the deployment package if it is stored as a file. null otherwise.
	
	// for user usage
	void			*pbUserData;
} vRM_DeviceData;


typedef struct tag_vRM_InstallerRecreationData
{
	unsigned short *pDeltaPath;
	unsigned short *pPrevInstallationFolder;
	unsigned short *pNewInstallationFolder;
	unsigned char  *pRam;				
	unsigned long   ui32RamSize;	

} vRM_InstallerRecreationData;


/* --------------------------------------------- 
   Additional vRM functions - exported functions
   --------------------------------------------- */

long RB_CheckDPStructure(void* pbUserData);
long RB_GetNumberOfDeltas(void* pbUserData, unsigned long* num_deltas, unsigned long *installer_types, unsigned long installer_types_num, unsigned long component_flags);
long RB_GetSignedDeltaOffset (void* pbUserData, unsigned long delta_ordinal, unsigned long* offset, unsigned long* size, unsigned long *installer_types, unsigned long installer_types_num, unsigned long component_flags);
long RB_GetUnsignedDeltaOffset(void* pbUserData, unsigned long delta_ordinal, unsigned long* offset, unsigned long* size, unsigned long *installer_types, unsigned long installer_types_num, unsigned long component_flags);

long RB_GetDeploymentDataDetails(void* pbUserData, unsigned long delta_ordinal, CompDeltaType* type, unsigned long* size, unsigned long *installer_types, unsigned long installer_types_num, unsigned long component_flags);
long RB_GetDeploymentData(void* pbUserData, unsigned long delta_ordinal, unsigned char* buffer, unsigned long size, unsigned long *installer_types, unsigned long installer_types_num, unsigned long component_flags); 

long RB_vRM_Update(vRM_DeviceData *pDeviceData);

long RB_vRM_GetDpRamUse(unsigned long *ui32pRamUse, vRM_DeviceData *pDeviceData);

long RB_vRM_RecreateInstallerFile(vRM_InstallerRecreationData *pInstallerRecreationData);

// returns the DP protocol version - that is, the protocol version of the first delta in the first valid component. 
// A valid component is one that has valid installer type and update type, and contains at least one delta. 
// (Similar to RB_GetDeltaProtocolVersion, but for the vRM case). 
long RB_GetDPProtocolVersion(void* pbUserData, void* pbyRAM, unsigned long dwRAMSize, 
							 unsigned long *installer_types, unsigned long installer_types_num, unsigned long component_flags,
							 unsigned long *dpProtocolVersion);

// returns the DP Scout protocol version - that is, the scout protocol version of the first delta in the first valid component. 
// A valid component is one that has valid installer type and update type, and contains at least one delta.
// (Similar to RB_GetDeltaScoutProtocolVersion, but for the vRM case). 
long RB_GetDPScoutProtocolVersion(void* pbUserData, void* pbyRAM, unsigned long dwRAMSize, 
								  unsigned long *installer_types, unsigned long installer_types_num, unsigned long component_flags,
								  unsigned long *dpScoutProtocolVersion);

/* ------------------------------------------- 
   Additional vRM functions - system interface
   ------------------------------------------- */

long RB_GetAvailableFreeSpace(void *pbUserData, const unsigned short* partition_name, unsigned long* available_flash_size);
long RB_Unlink(void *pbUserData, unsigned short *pLinkName);
long RB_Link(void *pbUserData, unsigned short *pLinkName, unsigned short *pReferenceFileName);
long RB_SetFileAttributes(void *pbUserData, const unsigned short *ui16pFilePath, const unsigned long ui32AttribSize, const unsigned char *ui8pAttribs);


#ifdef __cplusplus
}
#endif



#endif // _RB_VRM_IMAGE_UPDATE_H
