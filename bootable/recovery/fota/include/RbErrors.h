#ifndef	__RB_ERRORS_H__
#define	__RB_ERRORS_H__
/*____________  BASIC  TYPES _________________________________________*/
		/*********************************************************
	  **					C O N F I D E N T I A L             **
	 **						  Copyright ® 2002-2010             **
	  **                         Red Bend Software              **
		**********************************************************/

typedef long RB_RETCODE;

/*
 *  Baundries for the RB_RETCODE code range
 */
#define RB_MAX_CODE_RANGE			(0xFFFFFF)

/*________________  Return code definition  __________________*/


/* 
 * Return value is a 32 bit size layout:
 *
 *   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 *  +-+-+-+-+-+-+-+-+-----------------------------------------------+
 *  |S|r|r|r|r|r|r|r|                C  o  d  e                     |
 *  +-+-+-+-+-+-+-+-+-----------------------------------------------+
 *
 * Where:
 *
 * S        - Status bit indicates success/error
 *
 * r        - reserved bit
 *  
 *   
 * Code     - is the status code,  range: 0x0000 to RB_MAX_CODE_RANGE
 */


/*
 * indication for success in RB_RETCODE
 */
#define RB_SUCCESS_STATUS		(0)

/*
 * indication for error in RB_RETCODE
 */
#define RB_ERROR_STATUS	        (1)

/*
 *	Compose RB_RETCODE:
 */
#define  RB_MAKE_RETCODE(status, code)	(long)(((unsigned long)(status)<<31)|(code))

/*
 *	compose success and error codes
 */
#define  RB_MAKE_SUCCESS(rc)    RB_MAKE_RETCODE(RB_SUCCESS_STATUS, rc)
#define  RB_MAKE_ERROR(rc)      RB_MAKE_RETCODE(RB_ERROR_STATUS, rc)


/* _____________ return codes querying ________________ */

/*
 *	Test if composed returned value is error
 */
#define  RB_IS_ERROR(rc)        ( ((rc)>>31) == RB_ERROR_STATUS)
#define  RB_IS_SUCCESS(rc)		( ((rc)>>31) == RB_SUCCESS_STATUS)
#define  RB_CODE(rc)            ((rc)&RB_MAX_CODE_RANGE)


/* __________________________ general codes ________________________________________*/
#define  S_RB_SUCCESS               RB_MAKE_SUCCESS(0)
#define  E_RB_FAILURE               RB_MAKE_ERROR(0)

/* __________________________ specific success codes  ______________________________*/
#define  S_RB_NO_UPDATE             RB_MAKE_SUCCESS(0)
#define  S_RB_DONE                  RB_MAKE_SUCCESS(0)

/* __________________________ invocation errors ____________________________________*/
#define  E_RB_BAD_PARAMS			RB_MAKE_ERROR(2)    /* error in a run parameter */
#define  E_RB_NO_VALID_UPI			RB_MAKE_ERROR(3)    /* no valid UPI found       */
#define  E_RB_BUSY					RB_MAKE_ERROR(4)    /* future use               */

/* __________________________ update package errors ________________________________*/
#define  E_RB_PKG_TOO_SHORT			RB_MAKE_ERROR(11)	/* expected length error    */
#define  E_RB_PKG_TOO_LONG			RB_MAKE_ERROR(12)	/* expected length error    */
#define  E_RB_PKG_CORRUPTED			RB_MAKE_ERROR(13)	/* structural error         */
#define  E_RB_SOURCE_CORRUPTED		RB_MAKE_ERROR(14)	/* signature error          */
#define  E_RB_PKG_NOT_AUTHORIZED    RB_MAKE_ERROR(15)   /* foreign key not signed   */
#define  E_RB_WRONG_UPDATE			RB_MAKE_ERROR(16)	/* not for current version  */
#define  E_RB_WRONG_UPI_VER			RB_MAKE_ERROR(17)	/* non-compatible UPI		*/
#define  E_RB_WRONG_UPI_UPDATE		RB_MAKE_ERROR(18)   /* update for UPI does not
													       match its version        */
#define  E_RB_UPDATE_SECTOR_SIG		RB_MAKE_ERROR(19)   /* update for UPI does not
													       match its version        */
/*___________________________ resources errors _____________________________________*/
#define  E_RB_NOT_ENOUGH_RAM		RB_MAKE_ERROR(30)	/* given RAM is not enough  */ 
#define  E_RB_BAD_RAM				RB_MAKE_ERROR(31)	/* does not behave as RAM   */
#define  E_RB_NO_ROOM_FOR_NEW		RB_MAKE_ERROR(32)	/* new version is too big   */
#define  E_RB_WRITE_ERROR			RB_MAKE_ERROR(33)	/* flash writing failure    */
#define  E_RB_ERASE_ERROR			RB_MAKE_ERROR(34)	/* flash erasing failure    */
#define  E_RB_READ_ERROR			RB_MAKE_ERROR(35)	/* flash reading failure    */
#define  E_RB_MALLOC_ERROR			RB_MAKE_ERROR(36)	/* memory allocation failure*/

/*___________________________ final-stage errors ___________________________________*/
#define  E_RB_NEW_CORRUPTED			RB_MAKE_ERROR(40)   /* can not restore new      */
#define  E_RB_NEW_NOT_SIGNED		RB_MAKE_ERROR(41)   /* signature error of new   */
#define  E_RB_NEW_NOT_AUTHORIZED	RB_MAKE_ERROR(42)	/* foreign key not signed   */
#define  E_RB_INVALID_SUPPORT_FUNC	RB_MAKE_ERROR(43)	/* one API function is not declared */
#define	 E_RB_BCK_BUFFERS_NOT_ALIGN RB_MAKE_ERROR(44)	/* bck buffer(s) not sector aligned */
#define	 E_RB_START_ADD_NOT_ALIGN	RB_MAKE_ERROR(45)	/* start address is not sector aligned */

/*________________________ recommended File System update errors ___________________*/

#define  E_RB_OPENFILE_ONLYR		RB_MAKE_ERROR(205)  /* file does not exist      */
#define  E_RB_OPENFILE_WRITE		RB_MAKE_ERROR(206)  /* RO or no access rights   */
#define  E_RB_DELETEFILE_NOFILE		RB_MAKE_ERROR(207)	/* file does not exist      */
#define  E_RB_DELETEFILE			RB_MAKE_ERROR(208)	/* no access rights         */
#define  E_RB_RESIZEFILE			RB_MAKE_ERROR(209)	/* cannot resize file       */
#define  E_RB_READFILE_SIZE			RB_MAKE_ERROR(210)	/* cannot read specified size*/
#define  E_RB_CLOSEFILE_ERROR		RB_MAKE_ERROR(211)	/* cannot close file handle */
#define  E_RB_CANT_LINK_REGULAR_FILE RB_MAKE_ERROR(212)	/* cannot link regular file */	
#define  E_RB_CAN_UNLINK_ONLY_SYMBOLIC_LINK RB_MAKE_ERROR(213)/* cannot unlink regular file */	

#define	E_RB_BAD_FS_OPERATION					RB_MAKE_ERROR(300)	/* bad operation number for FS update*/
#define E_RB_INVALID_FW_OPERATION				RB_MAKE_ERROR(301)	/* bad operation number for FW update */
#define	E_RB_UNSUPPORTED_COMPRESSION			RB_MAKE_ERROR(302)	/* unsupported compression */
#define	E_RB_NO_REVERSE_IN_DELTA				RB_MAKE_ERROR(303)	/* Can not apply reverse update for delta not generated as reverse delta */
#define E_RB_NUM_BCK_LESS_THAN_IN_DELTA			RB_MAKE_ERROR(304)	/* number of backup buffers given to UPI does not match number in delta file */
#define E_RB_SECTOR_SIZE_MISMATCH				RB_MAKE_ERROR(305)	/* Sector size mismatch between UPI and delta */
#define E_RB_UPI_NOT_SUPPORT_REVERSE_UPDATE		RB_MAKE_ERROR(306)	/* UPI was not compiled to support reverse update */
#define E_RB_UPI_NOT_SUPPORT_IFS_ON_COMPRESSED	RB_MAKE_ERROR(307)	/* UPI was not compiled to support IFS on compressed images */
#define E_RB_UPI_FOR_BGU_MUST_SUPPORT_IFS		RB_MAKE_ERROR(308)	/* UPI was not compiled to support IFS */
#define E_RB_IMAGE_IS_NOT_SOURCE				RB_MAKE_ERROR(309)	/* Image verified is not source image */
#define E_RB_IN_SCOUT_ONLY_VERIFY				RB_MAKE_ERROR(310)	/* In scout only operation we should do only verify of image */
#define E_RB_NOT_ENOUGH_RAM_FOR_OPERATION2		RB_MAKE_ERROR(311)	/* There is not enough RAM to run with operation=2 */
#define E_RB_DELTA_FILE_TOO_LONG				RB_MAKE_ERROR(312)	/* Delta file too long - curropted */
#define E_RB_ERROR_IN_DELETES_SIG				RB_MAKE_ERROR(313)	/* Mismatch between deletes sig and delta deletes buffers signature */
#define E_RB_PKG_NUM_OF_FRAGMENTS_MISMATCH		RB_MAKE_ERROR(314)	/* Number of fragments in section is not 1 */
#define E_RB_OVERALL_NUM_BCK_SECTS_TOO_BIG		RB_MAKE_ERROR(315)	/* Over all number of backup sects too big */
#define E_RB_DELTA_IS_CORRUPT					RB_MAKE_ERROR(316)	/* Delta file is corrupt: signature mismatch between delta header signature and calculated signature */
#define E_RB_FILE_SIZE_MISMATCH					RB_MAKE_ERROR(317)	/* Source file size mismatch from file on device to delta file size */
#define E_RB_SOURCE_FILE_SIG_MISMATCH			RB_MAKE_ERROR(318)	/* File signature does not match signature */
#define E_RB_TARGET_SIG_MISMATCH				RB_MAKE_ERROR(319)	/* Signature for the target buffer does not match the one stored in the delta file */
#define E_RB_INVALID_BACKUP						RB_MAKE_ERROR(320)	/* Too many dirty buffers */
#define E_RB_UPI_VERSION_MISMATCH				RB_MAKE_ERROR(321)	/* UPI version mismatch between UPI and delta */
#define E_RB_SCOUT_VERION_MISMATCH				RB_MAKE_ERROR(322)	/* Scout version mismatch between UPI and delta */
#define E_RB_PARTITION_NAME_IS_DIFFRENT_BETWEEN_DELTA_AND_DEVICE_DATA	RB_MAKE_ERROR(323)	/* Partition name is different in delta and in UPI data */


#endif //__RB_ERRORS_H__
