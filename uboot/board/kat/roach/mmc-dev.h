
//#undef DEBUG_DATA /*For debugging*/
//#define DEBUG_DATA /*For debugging*/

#define DATA_BUFFER_SIZE    513/*Last byte for CRC and stop bit*/

/*Custom defines*/
#define MMC_SLOW_CLOCK      1
#define MMC_FAST_CLOCK      0

#define MMC_IDENT           0x10
#define MMC_CMD_EN          0x1
#define MMC_DATA_EN         0x2 

#define MMC_DAT3_HIGH       0x08

#define MMC_DETECT          0x01
#define MMC_WRITE_PROTECT   0x02
#define TRANS_DONE          0x10


#define ADV_TYPE_MANUAL     0x0
#define ADV_TYPE_CMD_RD     0x1
#define ADV_TYPE_CMD_WR     0x2
#define ADV_TYPE_DATA_RD    0x3
#define ADV_TYPE_DATA_WR    0x4

#define MMC_CARD_BUSY      0x80000000

#define START               0
#define TRANSMIT            1
#define END                 1

#define    MMC_GO_IDLE_STATE         0
#define    MMC_SEND_OP_COND          1 
#define    MMC_ALL_SEND_CID          2
#define    MMC_SET_RELATIVE_ADDR     3 
#define    MMC_SEND_CSD              4 
#define    MMC_TRANSFER_STATE        5
#define    MMC_SEND_EXT_CSD          6 
#define    MMC_SWITCH                7 
#define    MMC_VERIFY_STATUS         8 
#define    MMC_SET_BLOCK_LENGTH      9 
#define    MMC_READ_SINGLE_BLOCK    10 
#define    MMC_STOP_TRANSMISSION    11 
#define    INACTIVE                 12 

/* CSD field definitions */

#define		 CSD_STRUCT_VER_1_0 	 0           /* Valid for system specification 1.0 - 1.2 */
#define		 CSD_STRUCT_VER_1_1 	 1           /* Valid for system specification 1.4 - 2.2 */
#define		 CSD_STRUCT_VER_1_2 	 2           /* Valid for system specification 3.1 - 3.2 - 3.31 - 4.0 - 4.1 */
#define		 CSD_STRUCT_EXT_CSD 	 3          /* Version is coded in CSD_STRUCTURE in EXT_CSD */

/*EXT_CSD Fields*/ 

#define		 EXT_CSD_BUS_WIDTH 		   183  /*RW*/
#define 	 EXT_CSD_HS_TIMING		   185  /*RW*/
#define 	 EXT_CSD_CARD_TYPE 		   196  /*RO*/
#define 	 EXT_CSD_REV       		   192  /*RO*/
#define 	 EXT_CSD_SEC_CNT  		   212  /*RO,4 bytes*/
#define          EXT_S_CMD_SET                     504  /*RO*/

/*EXT_CSD field definitions*/

#define		EXT_CSD_CMD_SET_NORMAL            (1<<0)
#define		EXT_CSD_CMD_SET_SECURE            (1<<1)
#define 	EXT_CSD_CMD_SET_CPSECURE          (1<<2)

#define 	EXT_CSD_CARD_TYPE_26  		  (1<<0)  /* Card can run at 26MHz */
#define 	EXT_CSD_CARD_TYPE_52   		  (1<<1)  /* Card can run at 52MHz */

#define 	EXT_CSD_BUS_WIDTH_1   		  0       /* Card is in 1 bit mode */
#define 	EXT_CSD_BUS_WIDTH_4    		  1       /* Card is in 4 bit mode */
#define 	EXT_CSD_BUS_WIDTH_8   		  2       /* Card is in 8 bit mode */

/*MMC_SWITCH access modes*/

#define 	MMC_SWITCH_MODE_CMD_SET        	 0x00    /* Change the command set */
#define 	MMC_SWITCH_MODE_SET_BITS       	 0x01    /* Set bits which are 1 in value */
#define 	MMC_SWITCH_MODE_CLEAR_BITS     	 0x02    /* Clear bits which are 1 in value */
#define 	MMC_SWITCH_MODE_WRITE_BYTE     	 0x03    /* Set target to value */


#define DELAY(n) for(z=0 ;z < n * 10000 ;z++);

#define PARSE_U16(buf,index) (((u16)buf[index]) << 8) | ((u16)buf[index+1]);
#define PARSE_U32(buf,index) (((u32)buf[index]) << 24) | (((u32)buf[index+1]) << 16) | (((u32)buf[index+2]) << 8) | ((u32)buf[index+3]);

enum mmc_rsp_t {
    RESPONSE_NONE    = 0,
    RESPONSE_R1      = 1,
    RESPONSE_R1B     = 2,
    RESPONSE_R2_CID  = 3,
    RESPONSE_R2_CSD  = 4,
    RESPONSE_R3      = 5,
    RESPONSE_R4      = 6,
    RESPONSE_R5      = 7
};

/* Error codes */
enum mmc_result_t {
    MMC_NO_RESPONSE        = -1,
    MMC_NO_ERROR           = 0,
    MMC_ERROR_OUT_OF_RANGE,
    MMC_ERROR_ADDRESS,
    MMC_ERROR_BLOCK_LEN,
    MMC_ERROR_ERASE_SEQ,
    MMC_ERROR_ERASE_PARAM,
    MMC_ERROR_WP_VIOLATION,
    MMC_ERROR_CARD_IS_LOCKED,
    MMC_ERROR_LOCK_UNLOCK_FAILED,
    MMC_ERROR_COM_CRC,
    MMC_ERROR_ILLEGAL_COMMAND,
    MMC_ERROR_CARD_ECC_FAILED,
    MMC_ERROR_CC,
    MMC_ERROR_GENERAL,
    MMC_ERROR_UNDERRUN,
    MMC_ERROR_OVERRUN,
    MMC_ERROR_CID_CSD_OVERWRITE,
    MMC_ERROR_STATE_MISMATCH,
    MMC_ERROR_HEADER_MISMATCH,
    MMC_ERROR_TIMEOUT,
    MMC_ERROR_CRC,
    MMC_ERROR_DRIVER_FAILURE,
    MMC_ERROR_SWITCH,
};

/*
   MMC status in R1
   Type
e : error bit
s : status bit
r : detected and set for the actual command response
x : detected and set during command execution. the host must poll
the card by sending status command in order to read these bits.
Clear condition
a : according to the card state
b : always related to the previous command. Reception of
a valid command will clear it (with a delay of one command)
c : clear by read
*/

#define R1_OUT_OF_RANGE		(1 << 31)	/* er, c */
#define R1_ADDRESS_ERROR	(1 << 30)	/* erx, c */
#define R1_BLOCK_LEN_ERROR	(1 << 29)	/* er, c */
#define R1_ERASE_SEQ_ERROR      (1 << 28)	/* er, c */
#define R1_ERASE_PARAM		(1 << 27)	/* ex, c */
#define R1_WP_VIOLATION		(1 << 26)	/* erx, c */
#define R1_CARD_IS_LOCKED	(1 << 25)	/* sx, a */
#define R1_LOCK_UNLOCK_FAILED	(1 << 24)	/* erx, c */
#define R1_COM_CRC_ERROR	(1 << 23)	/* er, b */
#define R1_ILLEGAL_COMMAND	(1 << 22)	/* er, b */
#define R1_CARD_ECC_FAILED	(1 << 21)	/* ex, c */
#define R1_CC_ERROR		(1 << 20)	/* erx, c */
#define R1_ERROR		(1 << 19)	/* erx, c */
#define R1_UNDERRUN		(1 << 18)	/* ex, c */
#define R1_OVERRUN		(1 << 17)	/* ex, c */
#define R1_CID_CSD_OVERWRITE	(1 << 16)	/* erx, c, CID/CSD overwrite */
#define R1_WP_ERASE_SKIP	(1 << 15)	/* sx, c */
#define R1_CARD_ECC_DISABLED	(1 << 14)	/* sx, a */
#define R1_ERASE_RESET		(1 << 13)	/* sr, c */
#define R1_STATUS(x)            (x & 0xFFFFE000)
#define R1_CURRENT_STATE(x)    	((x & 0x00001E00) >> 9)	/* sx, b (4 bits) */
#define R1_READY_FOR_DATA	(1 << 8)	/* sx, a */
#define R1_SWITCH_ERROR		(1 << 7)	/* sr, c */
#define R1_APP_CMD		(1 << 5)	/* sr, c */

#define ONE_BIT_MODE             1
#define FOUR_BIT_MODE            2
#define EIGHT_BIT_MODE           3

//unsigned char data_buffer[DATA_BUFFER_SIZE];

enum card_state {
    CARD_STATE_EMPTY = -1,
    CARD_STATE_IDLE	 = 0,
    CARD_STATE_READY = 1,
    CARD_STATE_IDENT = 2,
    CARD_STATE_STBY	 = 3,
    CARD_STATE_TRAN	 = 4,
    CARD_STATE_DATA	 = 5,
    CARD_STATE_RCV	 = 6,
    CARD_STATE_PRG	 = 7,
    CARD_STATE_DIS	 = 8,
};

static char * mmc_result_strings[] = {
    "NO_RESPONSE",
    "NO_ERROR",
    "ERROR_OUT_OF_RANGE",
    "ERROR_ADDRESS",
    "ERROR_BLOCK_LEN",
    "ERROR_ERASE_SEQ",
    "ERROR_ERASE_PARAM",
    "ERROR_WP_VIOLATION",
    "ERROR_CARD_IS_LOCKED",
    "ERROR_LOCK_UNLOCK_FAILED",
    "ERROR_COM_CRC",
    "ERROR_ILLEGAL_COMMAND",
    "ERROR_CARD_ECC_FAILED",
    "ERROR_CC",
    "ERROR_GENERAL",
    "ERROR_UNDERRUN",
    "ERROR_OVERRUN",
    "ERROR_CID_CSD_OVERWRITE",
    "ERROR_STATE_MISMATCH",
    "ERROR_HEADER_MISMATCH",
    "ERROR_TIMEOUT",
    "ERROR_CRC",
    "ERROR_DRIVER_FAILURE",
    "ERROR_MMC_SWITCH",
};

char * mmc_result_to_string( int i )
{
    return mmc_result_strings[i + 1];
}

struct mmc_cmd{
    u8  start:1;
    u8  tx:1;
    u8  cmd_index:6;
    u32 arg:32;
    u8  crc7:7;
    u8  end:1;
};

struct mmc_cmd cmd0    = {START,TRANSMIT, 0,0x00000000,0x00,END};//set ocr Reg
struct mmc_cmd cmd1    = {START,TRANSMIT, 1,0x00FF8000,0x00,END};//set ocr Reg
struct mmc_cmd cmd2    = {START,TRANSMIT, 2,0x00000000,0x00,END};//rqst CID
struct mmc_cmd cmd3    = {START,TRANSMIT, 3,0x00040000,0x00,END};//set RCA:Default value 1,set less than 21
struct mmc_cmd cmd7    = {START,TRANSMIT, 7,0x00040000,0x00,END};//select Card
struct mmc_cmd cmd8    = {START,TRANSMIT, 8,0x00000000,0x00,END};//send Ext_CSD
struct mmc_cmd cmd9    = {START,TRANSMIT, 9,0x00040000,0x00,END};//address card with RCA to get csd
//struct mmc_cmd cmd6    = {START,TRANSMIT, 6,0x03b70200,0x00,END};//switches the mode of operation and program EXT_CSD,b7-ext_csd register 183,8bit mode
//struct mmc_cmd cmd6    = {START,TRANSMIT, 6,0x03b70101,0x00,END};//switches the mode of operation and program EXT_CSD,b7-ext_csd register 183,8bit mode
struct mmc_cmd cmd6    = {START,TRANSMIT, 6,0x03b70001,0x00,END};//switches the mode of operation and program EXT_CSD,b7-ext_csd register 183,1bit mode
struct mmc_cmd cmd13   = {START,TRANSMIT,13,0x00040000,0x00,END};//send status reg of specified RCA
struct mmc_cmd cmd16   = {START,TRANSMIT,16,0x00000200,0x00,END};//set block size to 0x200=512bytes
struct mmc_cmd cmd17   = {START,TRANSMIT,17,0x00000000,0x00,END};//single block read,arg=data_address
struct mmc_cmd cmd18   = {START,TRANSMIT,18,0x00000000,0x00,END};//multiple block read,arg=data_address
struct mmc_cmd cmd12   = {START,TRANSMIT,12,0x00000000,0x00,END};//stop Transmission,R1 for read and R1B for write


struct mmc_rsp{
    u8 start:1;
    u8 transmit:1;
    u8 check:6;
    u32 ocr:32;
    u8 crc:7;
    u8 end:1;
}r3;

struct mmc_rsp_info{
    struct mmc_rsp *rsp_ptr;
    int ident_mode;
    enum mmc_rsp_t rtype;
    enum card_state ctype;
    unsigned char *data_buffer;
    int bus_width;
};


struct mmc_cid{
    u8  mid:8;
    u8  cbx:2;
    u8  oid:8;
    u8  pnm[7];
    u8  prv:8;
    u32 psn:32;
    u8  mdt:8;
}cid;


struct mmc_csd {
    u8  csd_structure;
    u8  spec_vers;
    u8  taac;
    u8  nsac;
    u8  tran_speed;
    u16 ccc;
    u8  read_bl_len;
    u8  read_bl_partial;
    u8  write_blk_misalign;
    u8  read_blk_misalign;
    u8  dsr_imp;
    u16 c_size;
    u8  vdd_r_curr_min;
    u8  vdd_r_curr_max;
    u8  vdd_w_curr_min;
    u8  vdd_w_curr_max;
    u8  c_size_mult;
    union{
        struct { /* MMC system specification version 3.1 */
            u8  erase_grp_size;  
            u8  erase_grp_mult; 
        } v31;
        struct { /* MMC system specification version 2.2 */
            u8  sector_size;
            u8  erase_grp_size;
        } v22;

    }erase;
    u8  wp_grp_size;
    u8  wp_grp_enable;
    u8  default_ecc;
    u8  r2w_factor;
    u8  write_bl_len;
    u8  write_bl_partial;
    u8  file_format_grp;
    u8  copy;
    u8  perm_write_protect;
    u8  tmp_write_protect;
    u8  file_format;
    u8  ecc;
}csd;
