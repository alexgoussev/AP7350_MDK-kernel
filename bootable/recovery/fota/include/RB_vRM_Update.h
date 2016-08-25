/*
 *******************************************************************************
 * vRapid Mobile&reg; Notice
 *  
 * Copyright&copy; 1999-2014, Red Bend Software. All Rights Reserved.
 *
 * Patented: www.redbend.com/red-bend-patents.pdf
 * 
 * This Software is the property of Red Bend Ltd. and contains trade
 * secrets, know-how, confidential information and other intellectual
 * property of Red Bend Ltd.
 *
 * vRapid Mobile&reg;, Red Bend&reg;, and other Red Bend names, as well as the
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
 * \file	RB_vRM_Update.h
 *
 * \brief	UPI Update API
 *******************************************************************************
 */
#ifndef _RB_VRM_UPDATE_H
#define _RB_VRM_UPDATE_H
/**
 * Partition type
 */
typedef enum  
{
	PT_FOTA, 		//!< Image
	PT_FS			//!< File system
} PartitionType;

/**
 * In-place update
 */
typedef enum 
{
	UT_SELF_UPDATE=0,	//!< Don't update in place
	UT_NO_SELF_UPDATE,	//!< Update in place
	UT_PRIVATE,         //!< For internal usage
} UpdateType;

typedef unsigned int RB_UINT32;

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Partition data
 */
typedef struct tagCustomerPartitionData
{
/**
 * Partition name. Maximum 256 characters. Must match exactly the name used in
 * the UPG.
 */
	const char *partition_name;

/**
 * Partition flash address. Address must be sector aligned. Relevant only for
 * R/O partitions of any type; for R/W FS updates, set to 0.
 */
	RB_UINT32 rom_start_address;

/**
 * Mount point or drive letter containing the partition. Maximum size is 256
 * characters. Relevent only for R/W FS updates; otherwise set to 0.<p>
 */
	const char *mount_point;

/**
 * Source path (input) partition if the update will not be done in place.
 * Maximum 25 characters. For Image updates, set to 0.
 */
	const char *strSourcePath;

/**
 * Target path (output) partition if the update will not be done in place.
 * Maximum 25 characters. For Image updates, set to 0.
 */
	const char *strTargetPath;

/**
 * Internal use; leave null.
 */
	const void *priv;

/**
 * Partition type, a \ref PartitionType value.
 */
	PartitionType	partition_type;	

} CustomerPartitionData;

/**
 * Device data
 */
typedef struct tag_vRM_DeviceData
{
/**
 * UPI Mode. One of:
 * \li 0: scout and update. Verify that the update is applicable to the device
 * by comparing protocol versions and then install (or continue to install after
 * an interruption) the update.
 * \li 1: Scout only. Verify that the update is applicable to the device by
 * comparing protocol versions.
 * \li 2: Dry-run. Verify that the update is applicable to the device by
 * comparing protocol versions and then "run" the update without actually
 * changing any files on the device. This verification mode checks that files
 * can be uncompressed and updates can be applied. It does not verify the
 * results after the updates are applied.
 * \li 3: Update only. Install (or continue to install after an interruption)
 * the update without verification. Applying an update will fail
 * catastrophically if performed on an incompatible firmware. This mode cannot
 * be chosen if the update was created with an "optional file".
 * \li 6: Verify post-install: Verify the file contents of the updated firmware
 * (post-installation). This mode applies only to FS updates. Does not verify
 * attributes or empty folders.
 */
	RB_UINT32	ui32Operation;

/**
 * Pre-allocated RAM space.
 */
	unsigned char	*pRam;

/**
 * Size of pRam in bytes.
 */
	RB_UINT32	ui32RamSize;

/**
 * Number of backup sectors listed in pBufferBlocks.
 */
	RB_UINT32	ui32NumberOfBuffers;

/**
 * List of backup buffer sector addresses. Addresses must be
 * sector-aligned.If the update consists only of Image update, pBufferBlocks
 * is used instead of pTempPath. Otherwise pBufferBlocks should be set to 0 
 * and pTempPath (see below) will be used to allocate the backup as a file.
 */
	RB_UINT32	*pBufferBlocks;

/**
 * Number of partitions listed in pFirstPartitionData.
 */
	RB_UINT32	ui32NumberOfPartitions;

/**
 * List of partition data structures, a list of \ref CustomerPartitionData
 * values.
 */
	CustomerPartitionData  *pFirstPartitionData;

/**
 * Path to temporary storage. If the update contains FS updates, pTempPath is
 * used instead of pBufferBlocks. If the update consists only of Image updates,
 * set pTempPath to 0. The path size is limited to 256 characters. 
 * The path must be under the mount point of a partition containing a R/W file system.<p>
 * The maximum file size will not exceed the sector size x number of backup
 * sectors. 
 */
	char	*pTempPath;

/**
 * Whether or not there is a UPI self-update in the update, an
 * \ref UpdateType value.
 */
	UpdateType		enmUpdateType;

/**
 * List of customer-defined installer types. For an Image only update, use a
 * single element array with the value set to 0.<p>
 *
 * For the list of installer types, see the SWM Center documentation.
 */
	RB_UINT32	*pComponentInstallerTypes;

/**
 * Number of installer types in pComponentInstallerTypes.
 */
	RB_UINT32	ui32ComponentInstallerTypesNum;

/**
 * Update flags, if any. Used by the SWM Center.<p>
 *
 * For non SWM Center update, set to 0xFFFFFFFF
 * For the list of update flags, see the SWM Center documentation.
 */
	RB_UINT32   ui32ComponentUpdateFlags;

/**
 * Update number within the DP.
 */
	RB_UINT32	ui32OrdinalToUpdate;
	
/**
 * Deprecated.
 */
	char	*pDeltaPath;

/**
 * Additional data to pass to APIs, if any. Set to null if not used.
 */
	void			*pbUserData;
} vRM_DeviceData;

/**
 *******************************************************************************
 * Validate the DP.
 *
 * \param	pbUserData			Optional opaque data-structure to pass to IPL
 *								functions
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_CheckDPStructure(void* pbUserData);

/**
 *******************************************************************************
 * Get the number of updates in a DP that match the \a installer_type and
 * \a component_flags values.
 *
 * \param	pbUserData			Optional opaque data-structure to pass to IPL
 *								functions
 * \param	num_deltas			(out) Number of updates
 * \param	installer_types		List of installer types; for an Image only
 *								update, this is a one element array with a value
 *								of 0
 * \param	installer_types_num	Size of \a installer_types
 * \param	component_flags		Update flags, if any. Used by the SWM Center.
 *								For the list of component flags, see the SWM
 *								Center documentation.
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_GetNumberOfDeltas(void* pbUserData, RB_UINT32* num_deltas, RB_UINT32 *installer_types, RB_UINT32 installer_types_num, RB_UINT32 component_flags);

/**
 *******************************************************************************
 * Get the offset in bytes of the specified update in the DP. Consider only
 * updates that match the \a installer_type and \a component_flags values.
 * 
 * \param	pbUserData			Optional opaque data-structure to pass to IPL
 *								functions
 * \param	delta_ordinal		Update number
 * \param	offset				(out) Bytes from start of DP
 * \param	size				(out) Size of update, in bytes
 * \param	installer_types		List of installer types; for an Image only
 *								update, this is a one element array with a value
 *								of 0
 * \param	installer_types_num	Size of \a installer_types
 * \param	component_flags		Update flags, if any. Used by the SWM Center.
 *								For the list of component flags, see the SWM
 *								Center documentation.
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_GetSignedDeltaOffset (void* pbUserData, RB_UINT32 delta_ordinal, RB_UINT32* offset, RB_UINT32* size, RB_UINT32 *installer_types, RB_UINT32 installer_types_num, RB_UINT32 component_flags);

/**
 *******************************************************************************
 * Get the offset in bytes of the specified update in the DP. Consider only
 * updates that match the \a installer_type and \a component_flags values.
 * 
 * \param	pbUserData			Optional opaque data-structure to pass to IPL
 *								functions
 * \param	delta_ordinal		Update number
 * \param	offset				(out) Bytes from start of DP
 * \param	size				(out) Size of update, in bytes
 * \param	installer_types		List of installer types; for an Image only
 *								update, this is a one element array with a value
 *								of 0
 * \param	installer_types_num	Size of \a installer_types
 * \param	component_flags		Update flags, if any. Used by the SWM Center.
 *								For the list of component flags, see the SWM
 *								Center documentation.
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_GetUnsignedDeltaOffset(void* pbUserData, RB_UINT32 delta_ordinal, RB_UINT32* offset, RB_UINT32* size, RB_UINT32 *installer_types, RB_UINT32 installer_types_num, RB_UINT32 component_flags);

/**
 *******************************************************************************
 * Get signed and unsigned update offsets in DP.
 *
 * This is a Porting Layer function that must be implemented.
 *
 * \param	pbUserData			Optional data-structure, if required
 * \param	signed_delta_offset	(in) Signed update offset in DP
 * \param	delta_offset		(out) Unsigned update offset in DP
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_GetRBDeltaOffset(
	void *pbUserData,
	RB_UINT32 signed_delta_offset,
	RB_UINT32* delta_offset);

/**
 *******************************************************************************
 * Install an update.<p>
 *
 * The update is packaged within a DP, which may contain multiple updates as
 * well as other components. You must call this function once for each
 * update.<p>
 *
 * Depending on the UPI mode set, this function will scout, scout and update,
 * update, perform a dry-run, or verify the update post-update.
 *
 * \param	pDeviceData		The device data, a \ref vRM_DeviceData value
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_vRM_Update(vRM_DeviceData *pDeviceData);

/**
 *******************************************************************************
 * Get the highest minimum required RAM of all updates that match the
 * \a installer_type and \a component_flags values in \a pDeviceData. <br>
 * The returned amount of memory must be pre allocated and passed to
 * \ref RB_vRM_Update through \ref vRM_DeviceData
 *
 * \param	ui32pRamUse		(out)
 * \param	pDeviceData		The device data, a \ref vRM_DeviceData value
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_vRM_GetDpRamUse(RB_UINT32 *ui32pRamUse, vRM_DeviceData *pDeviceData);

/**
 *******************************************************************************
 * Get protocol version of an update in the DP.<p>
 *
 * This returns the version of the first update that matches the
 * \a installer_type and \a component_flags values. The returned protocol
 * version must match the UPI protocol version for the update to proceed.
 *
 * \param	pbUserData			Optional opaque data-structure to pass to IPL
 *								functions
 * \param	pbyRAM				(out) Buffer to contain part of the update from
 *								which the protocol version is extracted
 * \param	dwRAMSize			Size of \a pbyRAM; minimum 0x100 bytes
 * \param	installer_types		List of installer types; for an Image only
 *								update, this is a one element array with a value
 *								of 0
 * \param	installer_types_num	Size of \a installer_types
 * \param	component_flags		Update flags, if any. Used by the SWM Center.
 *								For the list of component flags, see the SWM
 *								Center documentation.
 * \param	dpProtocolVersion	(out) Buffer to contain DP protocol version
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_GetDPProtocolVersion(void* pbUserData, void* pbyRAM, RB_UINT32 dwRAMSize, 
							 RB_UINT32 *installer_types, RB_UINT32 installer_types_num, RB_UINT32 component_flags,
							 RB_UINT32 *dpProtocolVersion);

/**
 *******************************************************************************
 * Get scout protocol version of an update in the DP.<p>
 *
 * This returns the version of the first update that matches the
 * \a installer_type and \a component_flags values. The returned scout protocol
 * version must match the UPI scout protocol version for the update to proceed.
 *
 * \param	pbUserData				Optional opaque data-structure to pass to IPL
 *									functions
 * \param	pbyRAM					(out) Buffer to contain part of the update
 *									from which the protocol version is extracted
 * \param	dwRAMSize				Size of \a pbyRAM; minimum 0x100 bytes
 * \param	installer_types			List of installer types; for an Image only
 *									update, this is a one element array with a
 *									value of 0
 * \param	installer_types_num		Size of \a installer_types
 * \param	component_flags			Update flags, if any. Used by the SWM Center.
 *									For the list of component flags, see the SWM
 *									Center documentation.
 * \param	dpScoutProtocolVersion	(out) Buffer to contain DP scout protocol
 *									version
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_GetDPScoutProtocolVersion(void* pbUserData, void* pbyRAM, RB_UINT32 dwRAMSize, 
								  RB_UINT32 *installer_types, RB_UINT32 installer_types_num, RB_UINT32 component_flags,
								  RB_UINT32 *dpScoutProtocolVersion);

/**
 *******************************************************************************
 * Get UPI version.<p>
 *
 * This is not the protocol version number but the vRapid Mobile version number.
 *
 * \param	pbVersion	(out) Buffer to contain version
 *
 * \return	S_RB_SUCCESS
 *******************************************************************************
 */
long RB_GetUPIVersion(unsigned char *pbVersion);

/**
 *******************************************************************************
 * Get UPI protocol version.<p>
 *
 * Do not perform the update if this version does not match the DP protocol
 * version returned from \ref RB_GetDPProtocolVersion.
 *
 * \return	Protocol version, without periods (.) and with the revision number
 *			replaced by 0. For example, if the protocol version is 5.0.14.33,
 *			this returns 50140.
 *******************************************************************************
 */
RB_UINT32 RB_GetUPIProtocolVersion(void);

/**
 *******************************************************************************
 * Get UPI scout protocol version.<p>
 *
 * Do not perform the update if this version does not match the DP protocol
 * version returned from \ref RB_GetDPScoutProtocolVersion.
 *
 * \return	Scout protocol version, without periods (.) and with the revision
 *			number replaced by 0. For example, if the scout protocol version is
 *			5.0.14.33, this returns 50140.
 *******************************************************************************
 */
RB_UINT32 RB_GetUPIScoutProtocolVersion(void);

/**
 *******************************************************************************
 * Get protocol version of an update.<p>
 * 
 * \param	pbUserData		Optional opaque data-structure to pass to IPL
 *							functions
 * \param	pbyRAM			Pre-allocated RAM space
 * \param	dwRAMSize		Size of \a pbyRAM, in bytes
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */

RB_UINT32 RB_GetDeltaProtocolVersion(void* pbUserData, void* pbyRAM, RB_UINT32 dwRAMSize);
/**
 *******************************************************************************
 * Get scout protocol version of an update.<p>
 * 
 * \param	pbUserData		Optional opaque data-structure to pass to IPL
 *							functions
 * \param	pbyRAM			Pre-allocated RAM space
 * \param	dwRAMSize		Size of \a pbyRAM, in bytes
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */

RB_UINT32 RB_GetDeltaScoutProtocolVersion(void* pbUserData, void* pbyRAM, RB_UINT32 dwRAMSize); /* User data passed to all porting routines, pointer for the ram to use, size of the ram */

/**
 *******************************************************************************
 * Reset the watchdog timer.<p>
 *
 * This is a Porting Layer function that must be implemented.
 *
 * Called periodically to ensure that the bootstrap doesn't believe the boot to
 * be stalled during an update.
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_ResetTimerA(void);

/**
 *******************************************************************************
 * Display progress information to the end-user.<p>
 *
 * This is a Porting Layer function that must be implemented.
 *
 * Actually, you can do whatever you want with the progress information.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	uPercent	Update percentage
 *
 * \return	None
 *******************************************************************************
 */
void RB_Progress(
	void *pbUserData,
	RB_UINT32 uPercent);

/**
 *******************************************************************************
 * Print a debug message formatted using a printf-like string.<p>
 *
 * This is a Porting Layer function that must be implemented.
 *
 * Supported tags:
 * \li %x: Hex number
 * \li %0x: Hex number with leading zeroes
 * \li %u: Unsigned decimal
 * \li %s: Null-terminated string
 * \li %d: Signed decimal integer
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	aFormat		Printf-like format string
 * \param	...			Items to insert in \a aFormat
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
RB_UINT32 RB_Trace(
	void *pbUserData,
	const char *aFormat,...);

/**
 *******************************************************************************
 * Get update from DP.<p>
 *
 * This is a Porting Layer function that must be implemented.
 * A default implementation of this function is provided.
 *
 * \param	pbUserData				Optional opaque data-structure to pass to IPL
 *									functions
 * \param	pbBuffer				(out) Buffer to store the update
 * \param	dwStartAddressOffset	Update offset in DP
 * \param	dwSize					Size of \a pbBuffer
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_GetDelta(
	void *pbUserData,
	unsigned char *pbBuffer,
    RB_UINT32 dwStartAddressOffset,
    RB_UINT32 dwSize);


#ifdef __cplusplus
}
#endif



#endif // _RB_VRM_UPDATE_H
