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
 * \file	RB_vRM_Errors.h
 *
 * \brief	vRM Error Codes
 *******************************************************************************
 */
#ifndef __RB_VRM_ERRORS__
#define __RB_VRM_ERRORS__

/*!
 * Return value is a 32 bit size layout:
 * <pre>
 *   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 *  +-+-+-+-+-+-+-+-+-----------------------------------------------+ 
 *  |S|r|r|r|r|r|r|r|                C  o  d  e                     | 
 *  +-+-+-+-+-+-+-+-+-----------------------------------------------+ 
 * </pre>
 *
 * Where:
 *
 * S        - Status bit indicates success/error
 *
 * r        - reserved bit
 *   
 * Code     - is the status code,  range: 0x0000 to \ref RB_MAX_CODE_RANGE
 */
typedef int RB_RETCODE;

/*!
 *  Baundries for the RB_RETCODE code range
 */
#define RB_MAX_CODE_RANGE			(0xFFFFFF)

/*Return code definition  */

/*!
 * indication for success in RB_RETCODE
 */
#define RB_SUCCESS_STATUS	(0)

/*!
 * indication for error in RB_RETCODE
 */
#define RB_ERROR_STATUS	        (1)

/*!
 *	Compose RB_RETCODE
 */
#define  RB_MAKE_RETCODE(status, code)	(RB_RETCODE)(((RB_RETCODE)(status)<<31)|(code))

/*!
 *	Compose Success Code
 */
#define  RB_MAKE_SUCCESS(rc)    RB_MAKE_RETCODE(RB_SUCCESS_STATUS, rc)

/*!
 *	Compose Error Code
 */
#define  RB_MAKE_ERROR(rc)      RB_MAKE_RETCODE(RB_ERROR_STATUS, rc)


/* Return codes querying */

/*!
 *	Test if composed returned value is error
 */
#define  RB_IS_ERROR(rc)        ( ((RB_RETCODE)(rc)>>31) == RB_ERROR_STATUS)

/*!
 *	Test if composed returned value is success
 */
#define  RB_IS_SUCCESS(rc)		( ((RB_RETCODE)(rc)>>31) == RB_SUCCESS_STATUS)


/*! General Success Code */
#define  S_RB_SUCCESS               RB_MAKE_SUCCESS(0)

/*! General Failure Code */
#define  E_RB_FAILURE               RB_MAKE_ERROR(0)

/* invocation errors */
#define  E_RB_BAD_PARAMS			RB_MAKE_ERROR(2)    /**< error in a run parameter */

/* update package errors */
#define  E_RB_PKG_TOO_LONG			RB_MAKE_ERROR(12)	/**< expected length error    */
#define  E_RB_PKG_CORRUPTED			RB_MAKE_ERROR(13)	/**< structural error         */
#define  E_RB_SOURCE_CORRUPTED		RB_MAKE_ERROR(14)	/**< signature error          */

/* Resources errors */
#define  E_RB_NOT_ENOUGH_RAM		RB_MAKE_ERROR(30)	/**< given RAM is not enough  */ 
#define  E_RB_BAD_RAM				RB_MAKE_ERROR(31)	/**< does not behave as RAM   */
#define  E_RB_MALLOC_ERROR			RB_MAKE_ERROR(36)	/**< memory allocation failure */

/* Recommended error codes to be returned by image update porting layer */
#define  E_RB_WRITE_ERROR			RB_MAKE_ERROR(33)	/**< flash writing failure    */
#define  E_RB_ERASE_ERROR			RB_MAKE_ERROR(34)	/**< flash erasing failure    */
#define  E_RB_READ_ERROR			RB_MAKE_ERROR(35)	/**< flash reading failure    */

/* Final-stage errors */
#define  E_RB_INVALID_SUPPORT_FUNC	RB_MAKE_ERROR(43)	/**< one API function is not declared */
#define	 E_RB_BCK_BUFFERS_NOT_ALIGN RB_MAKE_ERROR(44)	/**< backup buffer(s) not sector aligned */
#define	 E_RB_START_ADD_NOT_ALIGN	RB_MAKE_ERROR(45)	/**< start address is not sector aligned */

/* Recommended error codes to be returned by File System porting layer */
/* The range 200-300 is reserved for customer specific error codes */
#define  E_RB_OPENFILE_ONLYR		RB_MAKE_ERROR(205)  /**< file does not exist      */
#define  E_RB_OPENFILE_WRITE		RB_MAKE_ERROR(206)  /**< RO or no access rights   */
#define  E_RB_DELETEFILE_NOFILE		RB_MAKE_ERROR(207)	/**< file does not exist      */
#define  E_RB_DELETEFILE			RB_MAKE_ERROR(208)	/**< no access rights         */
#define  E_RB_RESIZEFILE			RB_MAKE_ERROR(209)	/**< cannot resize file       */
#define  E_RB_READFILE_SIZE			RB_MAKE_ERROR(210)	/**< cannot read specified size*/
#define  E_RB_CLOSEFILE_ERROR		RB_MAKE_ERROR(211)	/**< cannot close file handle */
#define  E_RB_FAILED_CREATING_SYMBOLIC_LINK	RB_MAKE_ERROR(212) /**< Failed creating symbolic link */
#define  E_RB_CANNOT_CREATE_DIRECTORY		RB_MAKE_ERROR(213) /**< Failed creating directory */

/* Continue with error code 300 and above */
#define E_RB_INVALID_OPERATION					RB_MAKE_ERROR(301)	/**< bad operation number for update */
#define	E_RB_UNSUPPORTED_COMPRESSION			RB_MAKE_ERROR(302)	/**< unsupported compression */
#define	E_RB_NO_REVERSE_IN_DELTA				RB_MAKE_ERROR(303)	/**< Can not apply reverse update for delta not generated as reverse delta */
#define E_RB_NUM_BCK_LESS_THAN_IN_DELTA			RB_MAKE_ERROR(304)	/**< number of backup buffers given to UPI does not match number in delta file */
#define E_RB_SECTOR_SIZE_MISMATCH				RB_MAKE_ERROR(305)	/**< Sector size mismatch between UPI and delta */
#define E_RB_UPI_NOT_SUPPORT_REVERSE_UPDATE		RB_MAKE_ERROR(306)	/**< UPI was not compiled to support reverse update */
#define E_RB_UPI_NOT_SUPPORT_IFS_ON_COMPRESSED	RB_MAKE_ERROR(307)	/**< UPI was not compiled to support IFS on compressed images */
#define E_RB_UPI_NOT_SUPPORT_IFS				RB_MAKE_ERROR(308)	/**< UPI was not compiled to support IFS */
#define E_RB_IN_SCOUT_ONLY_VERIFY				RB_MAKE_ERROR(310)	/**< Source mismatch in scout only operation */
#define E_RB_NOT_ENOUGH_RAM_FOR_OPERATION2		RB_MAKE_ERROR(311)	/**< There is not enough RAM to run with operation=2 (Dry update) */
#define E_RB_DELTA_FILE_TOO_LONG				RB_MAKE_ERROR(312)	/**< Delta file too long - curropted */
#define E_RB_ERROR_IN_DELETES_SIG				RB_MAKE_ERROR(313)	/**< Mismatch between deletes sig and delta deletes buffers signature */
#define E_RB_PKG_NUM_OF_FRAGMENTS_MISMATCH		RB_MAKE_ERROR(314)	/**< Number of fragments in section is not 1 */
#define E_RB_OVERALL_NUM_BCK_SECTS_TOO_BIG		RB_MAKE_ERROR(315)	/**< Over all number of backup sects too big */
#define E_RB_DELTA_IS_CORRUPT					RB_MAKE_ERROR(316)	/**< Delta file is corrupt: signature mismatch between delta header signature and calculated signature */
#define E_RB_SOURCE_FILE_SIG_MISMATCH			RB_MAKE_ERROR(318)	/**< File signature does not match signature */
#define E_RB_TARGET_SIG_MISMATCH				RB_MAKE_ERROR(319)	/**< Signature for the target buffer does not match the one stored in the delta file */
#define E_RB_INVALID_BACKUP						RB_MAKE_ERROR(320)	/**< Too many dirty buffers */
#define E_RB_UPI_VERSION_MISMATCH				RB_MAKE_ERROR(321)	/**< UPI version mismatch between UPI and delta */
#define E_RB_SCOUT_VERION_MISMATCH				RB_MAKE_ERROR(322)	/**< Scout version mismatch between UPI and delta */
#define E_RB_PARTITION_NAME_NOT_FOUND			RB_MAKE_ERROR(323)	/**< Partition name is different in delta and in UPI data */
#define E_RB_NO_SPACE_LEFT						RB_MAKE_ERROR(324)	/**< There is not enough flash to update or install the files */
#define	E_RB_PERFORM_UPDATE_FAILED_NOT_ENOUGH_BACKUP	RB_MAKE_ERROR(0x10015) /**< There is not enough backup space on device */
#define E_RB_READ_WRITE_DELTA_NOT_SUPPORTED		RB_MAKE_ERROR(0x100F4) /**< UPI does not support RW file system update */
#define E_RB_IMAGE_DELTA_NOT_SUPPORTED			RB_MAKE_ERROR(0x100F5) /**< UPI does not support image update */

/* Deployment Package errors */
#define	E_RB_INVALID_DP_HEADER			RB_MAKE_ERROR(0x10025) /**< Deployment Package header is invalid */
#define	E_RB_INVALID_DP_WRONG_SIGNATURE	RB_MAKE_ERROR(0x10026) /**< Deployment Package signature is invalid */
#define	E_RB_UNSUPPORTED_DP_VERSION		RB_MAKE_ERROR(0x1002A) /**< Deployment Package version is not supported */
#define E_RB_INVALID_COMPONENT_ORDINAL	RB_MAKE_ERROR(0X1006C) /**< Requested ordinal does not exist in Deployment Package */
#define	E_RB_NO_MORE_COMPONENT_DELTAS	RB_MAKE_ERROR(0x10098) /**< Requested component was not found in Deployment Package  */

#define E_RB_FAILED_DURING_BACKGROUND_UPDATE	RB_MAKE_ERROR(0x100F3)


typedef enum 
{
	FT_REGULAR_FILE, 
	FT_SYMBOLIC_LINK,
	FT_FOLDER,
	FT_MISSING 
} enumFileType;


#endif //__RB_VRM_ERRORS__
