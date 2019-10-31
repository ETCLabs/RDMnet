
/* rdmEtcConsts.h -- Constants needed for general parsing of RDM packets, etc.*/

#ifndef _RDMETCCONSTS_H_
#define _RDMETCCONSTS_H_

/* The ETC manufacturer id for the UID, both bytes */
#define RDM_ETC_MANUFACTURERS	0x6574		/*Our Manufacturer ID: "et" */

/* estardm.h defines E120_BROADCAST_ALL_DEVICES_ID, we want it broken up */
#define RDM_ALL_MANUFACTURERS 0xffff
#define RDM_ALL_DEVICES 0xffffffff		/*Note that this also applies to all of our devices, since manufacturer is not part of it*/

/* This is the Manufacturer Name -- Maximum of 32 characters */
#define RDM_ETC_LABEL   "ETC"

/*** Constants used to index into the RDM packet ***/
#define RDM_OFFSET_STARTCODE            0
#define RDM_OFFSET_SUBSTART             1
#define RDM_OFFSET_CHECKSUMOFFSET       2	/* The length of the message, excluding the checksum */
#define RDM_OFFSET_DEST_MANUFACTURER    3   /* The manufacturer portion of the destination id */
#define RDM_OFFSET_DEST_DEVICE          5   /* The device id portion of the destination id */
#define RDM_OFFSET_SRC_MANUFACTURER     9   /* The manufacturer portion of the source id */
#define RDM_OFFSET_SRC_DEVICE           11  /* The device id portion of the destination id */
#define RDM_OFFSET_TRANSACTION          15  /* Sequential transaction number */
#define RDM_OFFSET_RESPONSE_TYPE        16
#define RDM_OFFSET_MSGCOUNT             17
#define RDM_OFFSET_SUBDEVICE            18
#define RDM_OFFSET_COMMAND              20  /* The Command class of the parameter */
#define RDM_OFFSET_PARAM_ID             21
#define RDM_OFFSET_PARAM_SLOTCOUNT      23  /* PDL (Parameter Data Length) */
#define RDM_OFFSET_PARAM_DATA           24  /* Parameter data starts here */
/*
-- The packet checksum, a 16-bit sum of bytes in the packet, follows the
-- variable-sized parameter data.  The offset to it is given in slot 2
-- (third byte in packet.)
*/

/*** Constants used to index into DISC_UNIQUE_BRANCH (DUB) Response packet ***/
/*
-- Index is based on first Encoded UID byte (EUID11).
-- The (0-7) Response Preamble bytes (0xFE) & Preamble Separator byte (0xAA) must be stripped off.
*/
#define RDM_DUB_RESP_OFFSET_EUID11      0   /* Encoded UID - Manufacturer ID1 bit-wise OR with 0xAA */
#define RDM_DUB_RESP_OFFSET_EUID10      1   /* Encoded UID - Manufacturer ID1 bit-wise OR with 0x55 */
#define RDM_DUB_RESP_OFFSET_EUID9       2   /* Encoded UID - Manufacturer ID0 bit-wise OR with 0xAA */
#define RDM_DUB_RESP_OFFSET_EUID8       3   /* Encoded UID - Manufacturer ID0 bit-wise OR with 0x55 */
#define RDM_DUB_RESP_OFFSET_EUID7       4   /* Encoded UID - Device ID3 bit-wise OR with 0xAA */
#define RDM_DUB_RESP_OFFSET_EUID6       5   /* Encoded UID - Device ID3 bit-wise OR with 0x55 */
#define RDM_DUB_RESP_OFFSET_EUID5       6   /* Encoded UID - Device ID2 bit-wise OR with 0xAA */
#define RDM_DUB_RESP_OFFSET_EUID4       7   /* Encoded UID - Device ID2 bit-wise OR with 0x55 */
#define RDM_DUB_RESP_OFFSET_EUID3       8   /* Encoded UID - Device ID1 bit-wise OR with 0xAA */
#define RDM_DUB_RESP_OFFSET_EUID2       9   /* Encoded UID - Device ID1 bit-wise OR with 0x55 */
#define RDM_DUB_RESP_OFFSET_EUID1       10  /* Encoded UID - Device ID0 bit-wise OR with 0xAA */
#define RDM_DUB_RESP_OFFSET_EUID0       11  /* Encoded UID - Device ID0 bit-wise OR with 0x55 */
#define RDM_DUB_RESP_OFFSET_ECS3        12  /* Encoded Checksum - Checksum1 bit-wise OR with 0x55 */
#define RDM_DUB_RESP_OFFSET_ECS2        13  /* Encoded Checksum - Checksum1 bit-wise OR with 0x55 */
#define RDM_DUB_RESP_OFFSET_ECS1        14  /* Encoded Checksum - Checksum0 bit-wise OR with 0x55 */
#define RDM_DUB_RESP_OFFSET_ECS0        15  /* Encoded Checksum - Checksum0 bit-wise OR with 0x55 */
/*
-- Checksum is the 16-bit sum of the previous 12 EUID slots.
*/

/* encoded bytes in DISC_UNIQUE_BRANCH Response (All Preamble bytes not included) */
#define RDM_DUB_RESP_ENCODED_BYTES      16

/*** Preamble constants of the DISC_UNIQUE_BRANCH Response packet ***/
#define RDM_DUB_RESP_PREAMBLE_BYTE              0xFE
#define RDM_DUB_RESP_PREAMBLE_SEPARATOR_BYTE    0xAA

/*
-- The minmum/maximum sizes for an RDM packet.
-- The min and max bytes include the two checksum bytes.
-- The valid range for the message length field (checksum offset) is 24 - 255.
-- The valid range for parameter data length is 0 - 231.
*/
#define RDM_MIN_BYTES       26
#define RDM_MAX_BYTES       257
#define RDM_MAX_PDL         231

/* This is the common max for RDM text fields, though specific cases may differ */
#define RDM_MAX_TEXT        32

/* ETC Manf Specific PIDs 0x8000 thru 0xFFDF */
#define ETC_OUTPUT_RESPONSE_TIME_DOWN                     0x8030

/* Manufacturers PIDS for GDS/ETC Fixtures */
#define E120_ETC_OUTPUT_RESPONSE_TIME_DOWN				  0x8030	
#define E120_ETC_ARCLAMP_DIMMING_STEPS					  0x8031	
#define E120_ETC_ARCLAMP_ENABLE_FTW						  0x8032	

/* For project reliant */
#define E120_ETC_EMERGENCY_ACTION						  0x8050
#define E120_ETC_EMERGENCY_ACTION_DESCRIPTION			  0x8051
#define E120_ETC_FTW_TEMP_SCALING						  0x8052
#define E120_ETC_OUTPUT_CURRENT							  0x8053
#define E120_ETC_OUTPUT_CURRENT_DESCRIPTION				  0x8054

/* 0x8100 thru 0x81FF are reserved for LED fixture use */
#define E120_ETC_LED_CURVE                                0x8101
#define E120_ETC_LED_CURVE_DESCRIPTION                    0x8102  
#define E120_ETC_LED_STROBE                               0x8103
#define E120_ETC_LED_OUTPUT_MODE                          0x8104
#define E120_ETC_LED_OUTPUT_MODE_DESCRIPTION              0x8105
#define E120_ETC_LED_RED_SHIFT                            0x8106
#define E120_ETC_LED_WHITE_POINT                          0x8107
#define E120_ETC_LED_WHITE_POINT_DESCRIPTION              0x8108
#define E120_ETC_LED_FREQUENCY                            0x8109
#define E120_ETC_DMX_LOSS_BEHAVIOR                        0x810A
#define E120_ETC_DMX_LOSS_BEHAVIOR_DESCRIPTION            0x810B
#define E120_ETC_LED_PLUS_SEVEN                           0x810C
#define E120_ETC_BACKLIGHT_BRIGHTNESS                     0x810D
#define E120_ETC_BACKLIGHT_TIMEOUT                        0x810E
#define E120_ETC_STATUS_INDICATORS                        0x810F
#define E120_ETC_RECALIBRATE_FIXTURE                      0x8110
#define E120_ETC_OVERTEMPMODE                             0x8111
#define E120_ETC_SIMPLESETUPMODE                          0x8112
#define E120_ETC_LED_STROBE_DESCRIPTION                   0x8113
#define E120_ETC_LED_RED_SHIFT_DESCRIPTION                0x8114
#define E120_ETC_LED_PLUS_SEVEN_DESCRIPTION               0x8115
#define E120_ETC_BACKLIGHT_TIMEOUT_DESCRIPTION            0x8116
#define E120_ETC_SIMPLESETUPMODE_DESCRIPTION              0x8117
#define E120_ETC_OVERTEMPMODE_DESCRIPTION                 0x8118
#define E120_ETC_LED_REQUESTED_XY	                      0x8119
#define E120_ETC_LED_CURRENT_XY	                         0x811A
#define E120_ETC_LED_CURRENT_PWM	                         0x811B
#define E120_ETC_LED_TRISTIMULUS                        	 0x811C
#define E120_ETC_LED_INFORMATION                          0x811D
#define E120_ETC_PRESETCONFIG                             0x811E
#define E120_ETC_SEQUENCE_PLAYBACK                        0x811F
#define E120_ETC_SEQUENCE_CONFIG                          0x8120
#define E120_ETC_LOW_POWER_TIMEOUT                        0x8121
#define E120_ETC_LOW_POWER_TIMEOUT_DESCRIPTION            0x8122
#define E120_ETC_LED_ENUM_FREQUENCY                       0x8123
#define E120_ETC_LED_ENUM_FREQUENCY_DESCRIPTION           0x8124
#define E120_ETC_RGBI_PRESETCONFIG                        0x8125
#define E120_ETC_CCT_PRESETCONFIG                         0x8126

#define E120_ETC_SUPPLEMENTARY_DEVICE_VERSION             0x8130
#define	E120_ETC_START_UWB_DISCOVER						  0x8150
#define	E120_ETC_START_UWB_MEASURE						  0x8151
#define E120_ETC_POSITION								  0x8152


#define E120_ETC_MULTIVERSE_SHOWID                        0x8C00
#define E120_ETC_MULTIVERSE_INCOMPLETE_PACKETS            0x8C01
#define E120_ETC_MULTIVERSE_RX_RSSI                       0x8C02

#define E120_ETC_S4DIM_CALIBRATE                          0x9000
#define E120_ETC_S4DIM_CALIBRATE_DESCRIPTION              0x9001
#define E120_ETC_S4DIM_TEST_MODE                          0x9002
#define E120_ETC_S4DIM_TEST_MODE_DESCRIPTION              0x9003
#define E120_ETC_S4DIM_MAX_OUTPUT_VOLTAGE                 0x9004
#define E120_ETC_S4DIM_MAX_OUTPUT_VOLTAGE_DESCRIPTION     0x9005

#define E120_ETC_POWER_COMMAND                            0xA000
#define E120_ETC_POWER_COMMAND_DESCRIPTION                0xA001
#define E120_ETC_THRESHOLD_COMMAND                        0xA002
#define E120_ETC_TURNON_DELAY_COMMAND                     0xA003
#define E120_ETC_SET_DALI_SHORTADDRESS                    0xA004
#define E120_ETC_DALI_GROUP_MEMBERSHIP                    0xA005
#define E120_ETC_AUTOBIND                                 0xA006

#define E120_ETC_PACKET_DELAY                             0xB000

#define E120_ETC_HAS_ENUM_TEXT                            0xE000
#define E120_ETC_GET_ENUM_TEXT                            0xE001

#define E120_ETC_PREPAREFORSOFTWAREDOWNLOAD               0xF000

/* ETC Manf Specific Data Types 0x80 thru 0xDF */

#define E120_DS_ETC_PARAMETER_DESC	                        0x90

/* 0xA0 thru 0xAF are reserved for LED fixture use */

#define E120_DS_ETC_LED_COLOR_COORD	                        0xA0
#define E120_DS_ETC_LED_PWM_DUTYCYCLE                       0xA1
#define E120_DS_ETC_LED_TRISTIMULUS	                        0xA2
#define E120_DS_ETC_LED_INFORMATION                         0xA3
#define E120_DS_ETC_PRESETCONFIG                            0xA4
#define E120_DS_ETC_SEQUENCECONFIG                          0xA5
#define E120_DS_ETC_RGBI_PRESETCONFIG                       0xA6
#define E120_DS_ETC_CCT_PRESETCONFIG                        0xA7

// The following are typically defined in estardm_StatusSlots.h since they are NOT ETC specific
#ifndef E120_STS_CAL_FAIL

/********************************************************/
/* Table B-2: Status Message ID Definitions             */
/********************************************************/


#define E120_STS_CAL_FAIL                                 0x0001
#define E120_STS_SENS_NOT_FOUND                           0x0002
#define E120_STS_SENS_ALWAYS_ON                           0x0003
#define E120_STS_FEEDBACK_ERROR                           0x0004
#define E120_STS_INDEX_ERROR                              0x0005
#define E120_STS_LAMP_DOUSED                              0x0011
#define E120_STS_LAMP_STRIKE                              0x0012
#define E120_STS_LAMP_ACCESS_OPEN                         0x0013
#define E120_STS_LAMP_ALWAYS_ON                           0x0014
#define E120_STS_OVERTEMP                                 0x0021
#define E120_STS_UNDERTEMP                                0x0022
#define E120_STS_SENS_OUT_RANGE                           0x0023
#define E120_STS_OVERVOLTAGE_PHASE                        0x0031
#define E120_STS_UNDERVOLTAGE_PHASE                       0x0032
#define E120_STS_OVERCURRENT                              0x0033
#define E120_STS_UNDERCURRENT                             0x0034
#define E120_STS_PHASE                                    0x0035
#define E120_STS_PHASE_ERROR                              0x0036
#define E120_STS_AMPS                                     0x0037
#define E120_STS_VOLTS                                    0x0038
#define E120_STS_DIMSLOT_OCCUPIED                         0x0041
#define E120_STS_BREAKER_TRIP                             0x0042
#define E120_STS_WATTS                                    0x0043
#define E120_STS_DIM_FAILURE                              0x0044
#define E120_STS_DIM_PANIC                                0x0045
#define E120_STS_LOAD_FAILURE                             0x0046
#define E120_STS_READY                                    0x0050
#define E120_STS_NOT_READY                                0x0051
#define E120_STS_LOW_LIST                                 0x0052
#define E120_STS_EEPROM_ERROR                             0x0060
#define E120_STS_RAM_ERROR                                0x0061
#define E120_STS_FPGA_ERROR                               0x0062
#define E120_STS_PROXY_BOARD_CAST_DROPPED                 0x0070
#define E120_STS_ASC_RXOK                                 0x0071
#define E120_STS_ASC_DROPPED                              0x0072
#define E120_STS_DMXNSCNONE                               0x0080
#define E120_STS_DMXNSCLOSS                               0x0081
#define E120_STS_DMXNSCERROR                              0x0082
#define E120_STS_DMXNSCOK                                 0x0083


/********************************************************/
/* Table C-1: Slot Type                                 */
/********************************************************/
#define E120_ST_PRIMARY                                        0x00
#define E120_ST_SEC_FINE                                       0x01
#define E120_ST_SEC_TIMING                                     0x02
#define E120_ST_SEC_SPEED                                      0x03
#define E120_ST_SEC_CONTROL                                    0x04
#define E120_ST_SEC_INDEX                                      0x05
#define E120_ST_SEC_ROTATION                                   0x06
#define E120_ST_SEC_INDEX_ROTATE                               0x07
#define E120_ST_SEC_UNDEFINED                                  0xFF


/********************************************************/
/* Table C-2: Slot ID Definitions                       */
/********************************************************/

#define E120_SD_INTENSITY                                      0x0001
#define E120_SD_INTENSITY_MASTER                               0x0002
#define E120_SD_PAN                                            0x0101
#define E120_SD_TILT                                           0x0102
#define E120_SD_COLOR_WHEEL                                    0x0201
#define E120_SD_COLOR_SUB_CYAN                                 0x0202
#define E120_SD_COLOR_SUB_YELLOW                               0x0203
#define E120_SD_COLOR_SUB_MAGENTA                              0x0204
#define E120_SD_COLOR_ADD_RED                                  0x0205
#define E120_SD_COLOR_ADD_GREEN                                0x0206
#define E120_SD_COLOR_ADD_BLUE                                 0x0207
#define E120_SD_COLOR_CORRECTION                               0x0208
#define E120_SD_COLOR_SCROLL                                   0x0209
#define E120_SD_COLOR_SEMAPHORE                                0x0210
#define E120_SD_STATIC_GOBO_WHEEL                              0x0301
#define E120_SD_ROTO_GOBO_WHEEL                                0x0302
#define E120_SD_PRISM_WHEEL                                    0x0303
#define E120_SD_EFFECTS_WHEEL                                  0x0304
#define E120_SD_BEAM_SIZE_IRIS                                 0x0401
#define E120_SD_EDGE                                           0x0402
#define E120_SD_FROST                                          0x0403
#define E120_SD_STROBE                                         0x0404
#define E120_SD_ZOOM                                           0x0405
#define E120_SD_FRAMING_SHUTTER                                0x0406
#define E120_SD_SHUTTER_ROTATE                                 0x0407
#define E120_SD_DOUSER                                         0x0408
#define E120_SD_BARN_DOOR                                      0x0409
#define E120_SD_LAMP_CONTROL                                   0x0501
#define E120_SD_FIXTURE_CONTROL                                0x0502
#define E120_SD_FIXTURE_SPEED                                  0x0503
#define E120_SD_MACRO                                          0x0504
#define E120_SD_UNDEFINED                                      0xFFFF

#endif

/* ETC Manufacturer Specific NACK Reason Codes 0x8000 thru 0xFFDF */

#define E120_ETC_NR_TIMEOUT                              0x8001 /* The responder timed out without sending a response.          */

#endif  /* _RDMETCCONSTS_H_ */
