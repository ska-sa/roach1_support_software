
/*BRIEF  : MMC Code:Sends commands,receives responses to the point where it reads a single block of data*/
/*AUTHOR : Shanly Rajan*/

#include <malloc.h>
#include <common.h>
#include <command.h>
#include <configs/roach.h>
#include <roach/roach_cpld.h>
#include "mmc-dev.h"

#ifdef CONFIG_CMD_MMC

int mmc_unpack_r1(unsigned char *buf,enum card_state state)
{
    unsigned char cmd;
    unsigned int status;

    cmd    = buf[0];/*start,tx bit would be 00 always for response*/
    status = PARSE_U32(buf,1);/*32bit wide card status*/

    debug(" cmd=%d status=%08x\n", cmd, status);

    if (R1_STATUS(status)) {
        if ( status & R1_OUT_OF_RANGE )       return MMC_ERROR_OUT_OF_RANGE;
        if ( status & R1_ADDRESS_ERROR )      return MMC_ERROR_ADDRESS;
        if ( status & R1_BLOCK_LEN_ERROR )    return MMC_ERROR_BLOCK_LEN;
        if ( status & R1_ERASE_SEQ_ERROR )    return MMC_ERROR_ERASE_SEQ;
        if ( status & R1_ERASE_PARAM )        return MMC_ERROR_ERASE_PARAM;
        if ( status & R1_WP_VIOLATION )       return MMC_ERROR_WP_VIOLATION;
        if ( status & R1_CARD_IS_LOCKED )     return MMC_ERROR_CARD_IS_LOCKED;
        if ( status & R1_LOCK_UNLOCK_FAILED ) return MMC_ERROR_LOCK_UNLOCK_FAILED;
        if ( status & R1_COM_CRC_ERROR )      return MMC_ERROR_COM_CRC;
        if ( status & R1_ILLEGAL_COMMAND )    return MMC_ERROR_ILLEGAL_COMMAND;
        if ( status & R1_CARD_ECC_FAILED )    return MMC_ERROR_CARD_ECC_FAILED;
        if ( status & R1_CC_ERROR )           return MMC_ERROR_CC;
        if ( status & R1_ERROR )              return MMC_ERROR_GENERAL;
        if ( status & R1_UNDERRUN )           return MMC_ERROR_UNDERRUN;
        if ( status & R1_OVERRUN )            return MMC_ERROR_OVERRUN;
        if ( status & R1_CID_CSD_OVERWRITE )  return MMC_ERROR_CID_CSD_OVERWRITE;
        if ( status & R1_SWITCH_ERROR )       return MMC_ERROR_SWITCH;
    }

    if(state == CARD_STATE_IDENT){ 
        if( buf[0] != cmd3.cmd_index ) 
            return MMC_ERROR_HEADER_MISMATCH;
        else
            debug("Passed the cmd3 header matching\n");
    }
    else if(state == CARD_STATE_STBY){ 
        if( buf[0] != cmd7.cmd_index ) 
            return MMC_ERROR_HEADER_MISMATCH;
        else
            debug("Passed the cmd7 header matching\n");
    }
    else if(state == CARD_STATE_TRAN){
        if( buf[0] == cmd6.cmd_index || buf[0] == cmd8.cmd_index || buf[0] == cmd16.cmd_index || buf[0] == cmd12.cmd_index || buf[0] == cmd17.cmd_index || buf[0] == cmd13.cmd_index) 
            debug("Passed the cmd6/8/16/17/12 header matching\n");
        else
            return MMC_ERROR_HEADER_MISMATCH;
    }
    /* This should be last - it's the least dangerous error */
    if ( R1_CURRENT_STATE(status) != state ){
        debug("The state reported is %d\n",R1_CURRENT_STATE(status));
        return MMC_ERROR_STATE_MISMATCH;
    }
    else{
        debug("No r1 error state mismatch\n");
    }

    return 0;
}

int mmc_unpack_r3( unsigned char *buf )
{

    r3.ocr = PARSE_U32(buf,1);
    debug(" ocr=%08x\n", r3.ocr);

    if ( buf[0] != 0x3f )  return MMC_ERROR_HEADER_MISMATCH;
    return 0;
}



int mmc_unpack_cid(unsigned char *buffer)
{
    int i;

    /*Populate CID structure*/
    cid.mid    = buffer[1];
    cid.cbx    = ((buffer[2] & 0x03) >> 2);   
    cid.oid    = buffer[3];
    for(i = 0 ; i < 6 ; i++){
        cid.pnm[i] = buffer[4 + i];
    }
    cid.pnm[6] = 0;
    cid.prv    = buffer[10];
    cid.psn    = PARSE_U32(buffer,11);
    cid.mdt    = buffer[15];
    debug("info: CID Contents\n");
    debug("      MID:%d\t",cid.mid);
    debug("      CBX :%d->%s\t",cid.cbx,(cid.cbx == 0 ? "Card Removable":"BGA"));
    debug("      OID:%d\t",cid.oid);
    debug("      PNM:%s\n",cid.pnm);
    debug("      PRV:%d.%d\t",(cid.prv >> 4),(cid.prv & 0xF));
    debug("      PSN:%08x\t",cid.psn);
    debug("      MDT:%d/%d\n",(cid.mdt >> 4),((cid.mdt & 0xF) + 1997));

#ifdef DEBUG_DATA
    debug("Debugger:Value in buffer for CID case is %x\n",buffer[0]);
#endif

    if ( buffer[0] != 0x3f ){ 
        printf("error: Header Mismatch:CID unpack:Value in buffer is %x\n",buffer[0]);
        return MMC_ERROR_HEADER_MISMATCH;
    }
    return 0;

}

int mmc_unpack_csd(unsigned char *buf)
{
    /*Populate CSD structure*/

    debug("CSD Contents\n");
    csd.csd_structure      = (buf[1] & 0xc0) >> 6;
    csd.spec_vers          = (buf[1] & 0x3c) >> 2;
    csd.taac               = buf[2];
    csd.nsac               = buf[3];
    csd.tran_speed         = buf[4];
    csd.ccc                = (((u16)buf[5]) << 4) | ((buf[6] & 0xf0) >> 4);
    csd.read_bl_len        = buf[6] & 0x0f;
    csd.read_bl_partial    = (buf[7] & 0x80) ? 1 : 0;
    csd.write_blk_misalign = (buf[7] & 0x40) ? 1 : 0;
    csd.read_blk_misalign  = (buf[7] & 0x20) ? 1 : 0;
    csd.dsr_imp            = (buf[7] & 0x10) ? 1 : 0;
    csd.c_size             = ((((u16)buf[7]) & 0x03) << 10) | (((u16)buf[8]) << 2) | (((u16)buf[9]) & 0xc0) >> 6;
    csd.vdd_r_curr_min     = (buf[9] & 0x38) >> 3;
    csd.vdd_r_curr_max     = buf[9] & 0x07;
    csd.vdd_w_curr_min     = (buf[10] & 0xe0) >> 5;
    csd.vdd_w_curr_max     = (buf[10] & 0x1c) >> 2;
    csd.c_size_mult        = ((buf[10] & 0x03) << 1) | ((buf[11] & 0x80) >> 7);
    switch ( csd.csd_structure ) {
        case CSD_STRUCT_VER_1_0:
        case CSD_STRUCT_VER_1_1:
            csd.erase.v22.sector_size    = (buf[11] & 0x7c) >> 2;
            csd.erase.v22.erase_grp_size = ((buf[11] & 0x03) << 3) | ((buf[12] & 0xe0) >> 5);
            break;
        case CSD_STRUCT_VER_1_2:
        default:
            csd.erase.v31.erase_grp_size = (buf[11] & 0x7c) >> 2;
            csd.erase.v31.erase_grp_mult = ((buf[11] & 0x03) << 3) | ((buf[12] & 0xe0) >> 5);
            break;
    }
    csd.wp_grp_size         =  buf[12] & 0x1f;
    csd.wp_grp_enable       = (buf[13] & 0x80) ? 1 : 0;
    csd.default_ecc         = (buf[13] & 0x60) >> 5;
    csd.r2w_factor          = (buf[13] & 0x1c) >> 2;
    csd.write_bl_len        = ((buf[13] & 0x03) << 2) | ((buf[14] & 0xc0) >> 6);
    csd.write_bl_partial    = (buf[14] & 0x20) ? 1 : 0;
    csd.file_format_grp     = (buf[15] & 0x80) ? 1 : 0;
    csd.copy                = (buf[15] & 0x40) ? 1 : 0;
    csd.perm_write_protect  = (buf[15] & 0x20) ? 1 : 0;
    csd.tmp_write_protect   = (buf[15] & 0x10) ? 1 : 0;
    csd.file_format         = (buf[15] & 0x0c) >> 2;
    csd.ecc                 =  buf[15] & 0x03;

    debug("  csd_structure=%d\t  spec_vers=%d\t  taac=%02x\t  nsac=%02x\t  tran_speed=%02x\n"
            "  ccc=%04x\t  read_bl_len=%d\t  read_bl_partial=%d\t  write_blk_misalign=%d\n"
            "  read_blk_misalign=%d\t  dsr_imp=%d\t  c_size=%d\t  vdd_r_curr_min=%d\n"
            "  vdd_r_curr_max=%d\t  vdd_w_curr_min=%d\t  vdd_w_curr_max=%d\t  c_size_mult=%d\n",
            csd.csd_structure, csd.spec_vers, 
            csd.taac, csd.nsac, csd.tran_speed,
            csd.ccc, csd.read_bl_len, 
            csd.read_bl_partial, csd.write_blk_misalign,
            csd.read_blk_misalign, csd.dsr_imp, 
            csd.c_size, csd.vdd_r_curr_min,
            csd.vdd_r_curr_max, csd.vdd_w_curr_min, 
            csd.vdd_w_curr_max, csd.c_size_mult);

    debug("  wp_grp_size=%d\t  wp_grp_enable=%d\t  default_ecc=%d\t  r2w_factor=%d\n"
            "  write_bl_len=%d\t  write_bl_partial=%d\t  file_format_grp=%d\t  copy=%d\n"
            "  perm_write_protect=%d\t  tmp_write_protect=%d\t  file_format=%d\t  ecc=%d\n",
            csd.wp_grp_size, csd.wp_grp_enable,
            csd.default_ecc, csd.r2w_factor, 
            csd.write_bl_len, csd.write_bl_partial,
            csd.file_format_grp, csd.copy, 
            csd.perm_write_protect, csd.tmp_write_protect,
            csd.file_format, csd.ecc);

    switch (csd.csd_structure) {
        case CSD_STRUCT_VER_1_0:
        case CSD_STRUCT_VER_1_1:
            debug(" V22 sector_size=%d\t erase_grp_size=%d\n", 
                    csd.erase.v22.sector_size, 
                    csd.erase.v22.erase_grp_size);
            break;
        case CSD_STRUCT_VER_1_2:
        default:
            debug(" V31 erase_grp_size=%d\t erase_grp_mult=%d\n", 
                    csd.erase.v31.erase_grp_size,
                    csd.erase.v31.erase_grp_mult);
            break;

    }

    if ( buf[0] != 0x3f ){
        debug("Header Mismatch:CSD unpack:Value in buffer is %x\n",buf[0]);
        return MMC_ERROR_HEADER_MISMATCH;
    }

    return 0;
}

unsigned char register_get(unsigned short address)
{
    unsigned char data;

    data = *((volatile unsigned char *)(CONFIG_SYS_CPLD_BASE + address));

    return data;
}

void register_set(unsigned short address, unsigned char data)
{
    *((volatile unsigned char *)(CONFIG_SYS_CPLD_BASE + address)) = data;
}

int check_mmc_insert(void)
{
    /*If active low then mmc card is inserted else not present*/
    if (!(register_get(MMC_STATUS) & MMC_DETECT))
        return 0;
    else
        return -1;
}


#define TRANS_DONE_TIMEOUT 1200
int trans_done_wait(void)
{
    int i;
    for (i = 0; i < TRANS_DONE_TIMEOUT; i++){
        if (register_get(MMC_STATUS) & TRANS_DONE)/*Checks whether MMC transaction is complete*/
            return 0;
    }
    return -1;
}

#define DATA_START_TIMEOUT 1200
int data_start_wait(void)
{
    int i;
    for (i = 0; i < DATA_START_TIMEOUT; i++){
        if (!(register_get(MMC_DATA_I) & 0x1)){
            debug("DETECT IN data_start_wait()\n");
            return 0;
        }
    }
    return -1;
}

/*
   Produce a 7 bit crc value Msb first.Seed is last round or initial round shift register
   value (lower 7 bits significant).  Input is an 8 bit value from byte stream being crcd.
   crc register is seeded initially with the value 0 prior to mixing in the variable packet data.
   Depth is usually 8, but the last time is 7 to shift in the augment string of 7 zeros.  */
static unsigned char crc7_calc(unsigned char Seed, unsigned char Input, unsigned char Depth)
{
    /*begin-crc7_calc.local defs: */
    register unsigned char regval;      // shift register byte.
    register unsigned char count;
    register unsigned char cc;          // data to manipulate.

#define POLYNOM (0x9)        // polynomical value to XOR when 1 pops out.

    /*BODY*/

    regval = Seed;    // get prior round's register value.
    cc = Input;       // get input byte to generate crc, MSB first.

    /* msb first of byte for Depth elements,set count to 8 or 7,for count # of bits,shift input value towards MSB to get next bit*/
    for (count = Depth; count--;cc <<= 1)
    {
        // Shift seven bit register left and put in MSB value of input to LSB.
        regval = (regval << 1) + ( (cc & 0x80) ? 1 : 0 );
        // Test D7 of shift register as MSB of byte, test if 1 and therefore XOR.
        if (regval & 0x80)
            regval ^= POLYNOM;
    } // end byte loop.
    return (regval & 0x7f);    // return lower 7 bits of crc as value to use.
}

#define MMC_TIMEOUT 1024

/*REMEMEBER:send_command,use cmd_i to clock in commands and cmd_o to collect responses*/
int send_command(struct mmc_cmd *cmd_ptr, int ident_mode)
{
    int i,j,z;
    unsigned char buffer[6];
    unsigned char value;
    unsigned char command_bit;
    int loop;    // index into argv parameter list.
    int val, nuval;    // temporary values from cmd line and crc generator.
    unsigned char crc_accum; // local storage of each round of crc.
    //unsigned char response_bit;

    /*Writing to external mmc registers*/
    /*Enable oen on CMD line and write to CMD_O to clock out bits by writing the value 2 to REG_ADV_TYPE*/
    register_set(MMC_OENS, (MMC_CMD_EN | (ident_mode ? MMC_IDENT : 0))); /*Bit 0 enables cmd out,01*/
    register_set(REG_ADV_TYPE, ADV_TYPE_CMD_WR);  

    buffer[0]  = cmd_ptr->start ? 0x80 : 0x0;    
    buffer[0] |= cmd_ptr->tx ? 0x40 : 0x0;
    buffer[0] |= (cmd_ptr->cmd_index & 0x3F);
    buffer[1]  = ((cmd_ptr->arg >> 24) & 0xFF); 
    buffer[2]  = ((cmd_ptr->arg >> 16) & 0xFF); 
    buffer[3]  = ((cmd_ptr->arg >>  8) & 0xFF); 
    buffer[4]  = ((cmd_ptr->arg)  & 0xFF); 


    // start w/seed of 0 per algorithm.
    crc_accum = 0;

    for (loop = 0; loop < 5; loop++)
    {
        val = (int)(buffer[loop]);
        nuval = crc7_calc(crc_accum, val, 8);
        //printf(" crc remainder of 0x%02X for 0x%02X input byte.\n", nuval, value );
        crc_accum = nuval;    // reload crc accum.
    }
    // mix in last 7 bits of 0s to augment
    nuval = crc7_calc(crc_accum, 0, 7);
    //printf("CALCULATED crc : 0x%02X.\n" , nuval);

    cmd_ptr->crc7 = nuval;

    buffer[5]  = ((((cmd_ptr->crc7) << 1) & 0xFE) | cmd_ptr->end );

    for(i = 0; i < 6; i++){
        value = buffer[i];
        for(j = 8; j > 0; j--){
            command_bit = ((value & 0x80) >> 7);
            register_set(MMC_CMD_O, command_bit);
            if (trans_done_wait()){
                printf("warning: timeout on transaction done bit\n");
                DELAY(5); 
            }
            /*Delaying a bit for transfer to be done:similar to polling trans_done bit*/
            value = value << 1;
        }
    }

    register_set(MMC_OENS, 0); /*Reset the cmd_en after sending command*//*NOTE:Changed to clear MMC_OENS,was disabled*/

    return 0;
}

void clear_buffer(unsigned char *buf)
{
    int i;
    for(i = 0 ; i < 18; i++){
        buf[i] = 0;
    }
}

int group_data_bits(struct mmc_rsp_info *info_ptr_d,unsigned char data_bit, int data_counter, int bit_count, int mode)
{

    switch(mode){
        case ONE_BIT_MODE:
            //info_ptr_d->data_buffer[data_counter] = (info_ptr_d->data_buffer[data_counter] | (data_bit << (8 - bit_count)));
            info_ptr_d->data_buffer[data_counter] = ((info_ptr_d->data_buffer[data_counter] & (~( 1 << (8 - bit_count))))  | (data_bit << (8 - bit_count)));
            break;
        case FOUR_BIT_MODE:
            info_ptr_d->data_buffer[data_counter] = (info_ptr_d->data_buffer[data_counter] | data_bit << 4);
            break;
        case EIGHT_BIT_MODE:
            info_ptr_d->data_buffer[data_counter] = ((info_ptr_d->data_buffer[data_counter] & 0x00) | data_bit);
            break;
    }

    return 0;
}


//int get_response(struct mmc_rsp *resp_ptr, int ident_mode , enum mmc_rsp_t rtype, enum card_state ctype, int buf_len)
int get_response(struct mmc_rsp_info *info_ptr)
{
    int i,j,z;
    unsigned char value = 0;
    unsigned char buffer[18];
    unsigned char response_bit;
    unsigned char data_bit;
    int data_counter = 0, bit_count = 0;
    //int bus_width_flag = 0; /*setting the bus_width_flag to zero activates 1-bit mode*/
    int got_data = 0;
    int val_ret;
    //unsigned char *data_ptr;

    register_set(MMC_OENS,(info_ptr->ident_mode ? MMC_IDENT : 0)); /*No outputs enabled,cmd_en need not be set since its reading in cmd_i*/
    register_set(REG_ADV_TYPE, ADV_TYPE_CMD_RD); /*advance mmc clock*/ 

    for (i = 0; i < MMC_TIMEOUT; i++){
        response_bit = (register_get(MMC_CMD_I) & 0x01);//i am interested in 0th bit
        if (trans_done_wait()){
            printf("warning: timeout on transaction done bit\n");
            DELAY(5); 
        }
        if(!response_bit)
            break;
        /* if (!(response_bit = (register_get(MMC_CMD_I) & 0x01))){
           break;
           }*/
    }

    if(response_bit){
        printf("\nError: no response retrieved\n");
        return -1; //no response
    }

    /* TODO: why is "value" uninitialized */
    if(info_ptr->rtype == RESPONSE_R2_CID || info_ptr->rtype == RESPONSE_R2_CSD){/*Activity on CMD line only*/
        for(i = 0; i < 17; i++){
            for(j = 8; j > 0; j--){
                value = value << 1;
                value = (value | response_bit);
                response_bit = (register_get(MMC_CMD_I) & 0x01);//i am interested in 0th bit
                if (trans_done_wait()){
                    printf("warning: timeout on transaction done bit\n");
                    DELAY(5); 
                }
            }
            buffer[i] = value;
        }
#ifdef DEBUG_DATA
        debug("Debug 2:Value in buffer for CID case is %x\n",buffer[0]);
#endif
        if(info_ptr->rtype == RESPONSE_R2_CID){
            if(mmc_unpack_cid(buffer))
                return -1;
        }
        else if(info_ptr->rtype == RESPONSE_R2_CSD){
            if(mmc_unpack_csd(buffer))
                return -1;
        }
    }
    else if(info_ptr->rtype == RESPONSE_R3){/*Activity on the CMD line only*/
        for(i = 0 ; i < 6 ; i++){
            for(j = 8; j > 0; j--){
                value = value << 1;
                value = (value | response_bit);
                response_bit = (register_get(MMC_CMD_I) & 0x01);//i am interested in 0th bit
                if (trans_done_wait()){
                    printf("warning: timeout on transaction done bit\n");
                    DELAY(5); 
                }
            }
            buffer[i] = value;
        }

        /*
           if(mmc_unpack_r3(buffer))
           return -1;
           */

        /*Populating the response in mmc_rsp structure*/
        info_ptr->rsp_ptr->start = (buffer[0] >> 7);
        info_ptr->rsp_ptr->transmit = ((buffer[0] << 1) >> 7);
        info_ptr->rsp_ptr->check = (buffer[0] & 0x3F);
        info_ptr->rsp_ptr->ocr  = PARSE_U32(buffer,1);
        info_ptr->rsp_ptr->crc  = ((buffer[5] & 0xFE) >> 1); 
        info_ptr->rsp_ptr->end  = (buffer[5] & 0x01);

    }
    else if(info_ptr->rtype == RESPONSE_R1){/*Activity on CMD and DATA lines for rest of commands except CMD3*/


        register_set(REG_ADV_TYPE, ADV_TYPE_DATA_RD); /*advance mmc clock on data reads*/ /*NOTE:Added this*/

        /*Normal response command:48bits wide*/
        for(i = 0 ; i < 6 ; i++){
            for(j = 8; j > 0; j--){
                value = value << 1;
                value = (value | response_bit);
                response_bit = (register_get(MMC_CMD_I) & 0x01);
                data_bit = (register_get(MMC_DATA_I) & 0x01);
                if(!data_bit && !got_data){
                    debug("Got start databit-[%d] in response rec time at j = %d and i value being=%d\n",data_bit,j,i);
                    got_data = 1;
                }
                if (trans_done_wait()){
                    printf("warning: timeout on transaction done bit\n");
                    DELAY(5); 
                }
            }
            buffer[i] = value;
        }

        /*Checks for the card status,which is bits [39:8] of the response collected(32bit wide) and decodes it*/
        if(mmc_unpack_r1(buffer,info_ptr->ctype)){
            debug("NOT GOOD AT THIS POINT\n");
            return -1;
        }

        /*Continuing to collect the data after response length finished if got_data=1*/
        if(got_data){
            debug("Entered second grouping loop with data_counter=%d and bit_count = %d\n",data_counter,bit_count);
            /* TODO: why is this data_counter uninitialized */
            for(i = data_counter; i < DATA_BUFFER_SIZE; i++){
                for(j = (bit_count + 1); j <= 8; j++){
                    data_bit = (register_get(MMC_DATA_I) & 0x01);
                    bit_count++;
                    group_data_bits(info_ptr,data_bit, data_counter, bit_count, ONE_BIT_MODE);
                    if(bit_count == 8){
                        bit_count = 0;
                    }
                }	
            }
        } 
        else{
            //	register_set(REG_ADV_TYPE, ADV_TYPE_DATA_RD); /*advance mmc clock on data reads*//*NOTE:Disabled this*/ 

            for (i = 0; i < 100000; i++){
                if (!(register_get(MMC_DATA_I) & 0x01)){
                    got_data = 1;
                    debug("Start Bit detected at i=%d\n",i);
                    break;
                }
            }
            //if(got_data && !bus_width_flag){
            if(got_data && (info_ptr->bus_width == ONE_BIT_MODE)){
                debug("1-bit mode\n");
                debug("DATA ACTIVITY:Detected Start Bit\n");
                for(i = 0; i < DATA_BUFFER_SIZE; i++){
                    for(j = 0; j < 8; j++){
                        data_bit = (register_get(MMC_DATA_I) & 0x01);
                        group_data_bits(info_ptr,data_bit, i, j + 1, ONE_BIT_MODE);
                    }	
                }
                debug("DATA COLLECTION:Finished at i=%d\n");
                if((register_get(MMC_DATA_I) & 0x01) == 1)
                    debug("DATA ACTIVITY:Detected Stop Bit\n");

            }
            else if(got_data && (info_ptr->bus_width == FOUR_BIT_MODE)){
                debug("4-bit mode\n");
                debug("4-bit DATA ACTIVITY:Detected Start Bit\n");
                for(i = 0; i < DATA_BUFFER_SIZE; i++){
                    for(j = 0; j < 4; j++){
                        data_bit = (register_get(MMC_DATA_I) & 0x0F);
                        group_data_bits(info_ptr,data_bit, i, j + 1, FOUR_BIT_MODE);
                    }	
                }
                debug("4-bit DATA COLLECTION:Finished\n");
                if((register_get(MMC_DATA_I) & 0x01) == 1)
                    debug("DATA ACTIVITY:Detected Stop Bit\n");

            }
            //else if(got_data && bus_width_flag){
            else if(got_data && (info_ptr->bus_width == EIGHT_BIT_MODE)){
                debug("8-bit mode\n");
                debug("8-bit DATA ACTIVITY:Detected Start Bit\n");
                for(i = 0; i < DATA_BUFFER_SIZE; i++){
                    data_bit = (register_get(MMC_DATA_I) & 0xFF);
                    group_data_bits(info_ptr,data_bit, i, j + 1, EIGHT_BIT_MODE);
                }
                debug("8-bit DATA COLLECTION:Finished\n");

            }
        }
        got_data = 0;


        }
            else if(info_ptr->rtype == RESPONSE_R1B){

                /*No response,Check for the DAT0 line,detect start bit and exit when it becomes FF*/
                register_set(REG_ADV_TYPE, ADV_TYPE_DATA_RD); /*advance mmc clock on data reads*/ 

                /*Normal response command:48bits wide*/
                for(i = 0 ; i < 6 ; i++){
                    for(j = 8; j > 0; j--){
                        value = value << 1;
                        value = (value | response_bit);
                        response_bit = (register_get(MMC_CMD_I) & 0x01);
                        data_bit = (register_get(MMC_DATA_I) & 0x01);
                        if(!data_bit && !got_data){
                            debug("Got start databit-[%d] in response rec time at j = %d and i value being=%d\n",data_bit,j,i);
                            got_data = 1;
                        }
                        if (trans_done_wait()){
                            printf("warning: timeout on transaction done bit\n");
                            DELAY(5); 
                        }
                    }
                    buffer[i] = value;
                }


                if(got_data){
                    while((register_get(MMC_DATA_I) & 0x01) != 1){
                        if((register_get(MMC_DATA_I) & 0x01) == 1)
                            break;
                    }
                    debug("DATA ACTIVITY:Detected Stop Bit\n");


                }
                else{
                    for (i = 0; i < 100000; i++){
                        if (!(register_get(MMC_DATA_I) & 0x01)){
                            got_data = 1;
#ifdef DEBUG_DATA
                            debug("Start Bit detected in CMD6 response at i=%d\n",i);
#endif
                            break;
                        }
                    }
                    if(got_data){
                        debug("DATA ACTIVITY:Detected Start Bit\n");
                        while((register_get(MMC_DATA_I) & 0x01) != 1){
                            if((register_get(MMC_DATA_I) & 0x01) == 1)
                                break;
                        }
                        debug("DATA ACTIVITY:Detected Stop Bit\n");
                    }
                }
                got_data = 0;
                /*Checks for the card status,which is bits [39:8] of the response collected(32bit wide) and decodes it*/
                val_ret = mmc_unpack_r1(buffer,info_ptr->ctype);
                if(val_ret){
                    debug("NOT GOOD AT THIS POINT and the val_ret= %d\n",val_ret);
                    return -1;
                }
            }
            else
                debug("rtype failure\n");
#ifdef DEBUG_DATA
            clear_buffer(buffer);
#endif
            return 0; /*response retrieved*/
        }

        void initialise_mmc(void)
        {
            int i;

            debug("MMC Initialisation Process Started\n");

            /*Disable all OENs,Drive the clock at lower speeds*/
            register_set(MMC_OENS, MMC_IDENT | MMC_CMD_EN );

            /*Put DATA/CMD to all 1's (idle);Keep DAT3 high during the step when sending CMD0*/
            register_set(MMC_DATA_O, 0xFF);
            register_set(MMC_CMD_O,  0xFF);

            //Transmit 74 clock edges 
            register_set(REG_ADV_TYPE, ADV_TYPE_MANUAL); /*advance mmc clock*/ 

            for(i = 0; i < 100; i++){
                register_set(REG_ADV_MAN, 1); /*advance mmc clock*/ 
                if (trans_done_wait())/*Checks whether MMC transaction is complete*/
                    printf("warning: timeout on transaction done bit\n");
            }

            /*send CMD0*/
            if(!(send_command(&cmd0, MMC_SLOW_CLOCK)))
                debug("Software Reset:CMD0 Issued\n");
            else
                debug("CMD0 not send successfully\n");

            //Transmit 10 clock edges 
            register_set(REG_ADV_TYPE, ADV_TYPE_MANUAL); /*advance mmc clock*/ 

            for(i = 0; i < 10; i++){
                register_set(REG_ADV_MAN, 1); /*advance mmc clock*/ 
                if (trans_done_wait())
                    printf("warning: timeout on transaction done bit\n");
            }

            debug("MMC Initialisation Done\n");

        }




int mmc_init(int verbose)/*Naming scheme adapted to suit the existing uboot macro functions*/
{
    int i = 0;
    int flag = 1;	  	      /*flag to start and end statemachine*/
    int check_insert;	      /*MMC presence detect*/
    int retval;		          /*Response ret value*/
    unsigned int state;  	  /*state machine state*/
    struct mmc_rsp response; 
    struct mmc_rsp_info info = {NULL,MMC_SLOW_CLOCK,0,0,NULL,ONE_BIT_MODE};

    /*Check whether card is inserted and proceed*/
    check_insert = check_mmc_insert();

    /*If MMC card inserted*/
    if(!check_insert)
    {
        printf("info: MMC inserted, performing initialization\n");

        /*Initialisation process*/
        initialise_mmc();

        state = MMC_SEND_OP_COND;

        while(flag){
            switch(state){
                case MMC_GO_IDLE_STATE:
                    if(flag)
                        state = MMC_SEND_OP_COND;
                    else
                        debug("STATE MACHINE HALTED\n");
                    break;
                case MMC_SEND_OP_COND:
                    /*Issue command 1*/
                    debug("Sending CMD1:MMC_SEND_OP_COND\n");
                    if(!(send_command(&cmd1, MMC_SLOW_CLOCK))){

                        info.rsp_ptr = &response; 
                        info.ident_mode = MMC_SLOW_CLOCK;
                        info.rtype = RESPONSE_R3;
                        info.ctype = CARD_STATE_IDLE;
                        info.data_buffer = NULL;

                        if(!(get_response(&info))){

                            /*The host must repeat CMD1 until the busy bit is cleared*/
                            /*ocr[31] bit is set to LOW if the card has not finished power up routine*/
                            if(response.ocr & MMC_CARD_BUSY){

                                /*Checking 8th bit,ocr[7]*/
                                if(response.ocr == 0x80FF8000)
                                    printf("info: High Voltage Multimedia Card\n");
                                else if(response.ocr == 0x80FF8080)
                                    printf("info: Dual Voltage Multimedia Card\n");
                                else
                                    printf("warning: No category to assign\n");  

                                state = MMC_ALL_SEND_CID;
                            }
                            else{
                                state = MMC_SEND_OP_COND;
                                /*NOTE:For how long we need to resend CMD1*/
                            }
                        }
                        else{
                            debug("Response Failure\n");
                            state = MMC_SEND_OP_COND;
                        }
                    }
                    else{
                        debug("Sending Error:MMC_SEND_OP_COND\n");
                        state = MMC_SEND_OP_COND;
                    }
                    break;
                case MMC_ALL_SEND_CID:
                    /*Issue command 2*/
                    debug("Sending CMD2:MMC_ALL_SEND_CID\n");
                    if(!(send_command(&cmd2, MMC_FAST_CLOCK))){

                        info.rsp_ptr = &response; 
                        info.ident_mode = MMC_FAST_CLOCK;
                        info.rtype = RESPONSE_R2_CID;
                        info.ctype = CARD_STATE_IDLE;
                        info.data_buffer = NULL;

                        if(!(get_response(&info))){
                            state = MMC_SET_RELATIVE_ADDR;
                        }
                        else{
                            debug("No response received in MMC_ALL_SEND_CID state\n");
                            state = MMC_GO_IDLE_STATE;
                            flag = 0;
                        }
                    }
                    else{
                        state = MMC_GO_IDLE_STATE;
                        debug("Send Error:MMC_ALL_SEND_CID STATE\n");
                        flag = 0;
                    }
                    break;
                case MMC_SET_RELATIVE_ADDR:
                    /*Issue command 3*/
                    debug("Sending CMD3:MMC_SET_RELATIVE_ADDR\n");
                    if(!(send_command(&cmd3, MMC_FAST_CLOCK))){

                        info.rsp_ptr = &response; 
                        info.ident_mode = MMC_FAST_CLOCK;
                        info.rtype = RESPONSE_R1;
                        info.ctype = CARD_STATE_IDENT;
                        info.data_buffer = NULL;

                        retval = (get_response(&info)); 

                        if(!(retval)){
                            state = MMC_SEND_CSD;
                        }
                        else{
                            printf("error: Unable to ALL_SEND_CID error=%d (%s)\n",retval,mmc_result_to_string(retval));
                            state = MMC_GO_IDLE_STATE;
                            flag = 0;
                        }
                    }
                    else{
                        state = MMC_GO_IDLE_STATE;
                        printf("error: MMC_SET_RELATIVE_ADDR\n");
                        flag = 0;
                    }
                    break;
                case MMC_SEND_CSD:
                    /*Issue command 9*/
                    debug("Sending CMD9:MMC_SEND_CSD\n");
                    if(!(send_command(&cmd9, MMC_FAST_CLOCK))){

                        info.rsp_ptr = &response; 
                        info.ident_mode = MMC_FAST_CLOCK;
                        info.rtype = RESPONSE_R2_CSD;
                        info.ctype = CARD_STATE_IDLE;
                        info.data_buffer = NULL;

                        if(!(get_response(&info))){
                            if(csd.spec_vers >= 4){
                                printf("info: Support for mmc version 4.0 and above\n");
                                state = MMC_TRANSFER_STATE;
                            }
                        }
                        else{
                            state = MMC_GO_IDLE_STATE;
                            printf("error: No response received in MMC_SEND_CSD state,state set to MMC_ALL_SEND_CID\n");
                            flag = 0;
                        }
                    }
                    else{
                        state = MMC_GO_IDLE_STATE;
                        printf("error:MMC_SEND_CSD\n");
                        flag = 0;
                    }
                    break;
                case MMC_TRANSFER_STATE:
                    /*Issue command 7*/
                    debug("Sending CMD7:MMC_TRANSFER_STATE\n");
                    if(!(send_command(&cmd7, MMC_FAST_CLOCK))){

                        info.rsp_ptr = &response; 
                        info.ident_mode = MMC_FAST_CLOCK;
                        info.rtype = RESPONSE_R1;
                        info.ctype = CARD_STATE_STBY;
                        info.data_buffer = NULL;

                        retval = (get_response(&info)); 

                        if(!(retval)){
                            debug("MMC TRANSFER STATE complete\n");
                            state = MMC_SEND_EXT_CSD;
                        }
                        else{
                            printf("error: No response received in MMC_TRANSFER_STATE Error=%d (%s)\n",retval,mmc_result_to_string(retval));
                            state = MMC_GO_IDLE_STATE;
                            flag = 0;
                        }
                    }
                    else{
                        state = MMC_GO_IDLE_STATE;
                        printf("error:MMC_SEND_CSD\n");
                        flag = 0;
                    }
                    break;
                case MMC_SEND_EXT_CSD: 
                    /*Issue command 8*/
                    debug("Sending CMD8:MMC_SEND_EXT_CSD \n");
                    if(!(send_command(&cmd8, MMC_FAST_CLOCK))){

                        info.rsp_ptr = &response; 
                        info.ident_mode = MMC_FAST_CLOCK;
                        info.rtype = RESPONSE_R1;
                        info.ctype = CARD_STATE_TRAN;
                        info.data_buffer = (u8 *)malloc(DATA_BUFFER_SIZE);/*Dynamically allocating 512 bytes of block*/

                        /*Check whether successful in allocation*/
                        if(info.data_buffer == NULL){
                            printf("error: Data Buffer Allocation for EXT_CSD:Unsuccessful");
                            retval = -1;	
                        }

                        retval = (get_response(&info)); 

                        if(!(retval)){

                            debug("Default Bus width mode=[%d]\n",info.data_buffer[EXT_CSD_BUS_WIDTH]);

                            if(info.data_buffer[EXT_CSD_CARD_TYPE])
                                printf("info: High Speed Multimedia Card @ 52MHz\n");
                            else
                                printf("info: High Speed Multimedia Card @ 26MHz\n");

                            debug("Default S_CMD_SET=[%d]:%s\n",info.data_buffer[EXT_S_CMD_SET],(info.data_buffer[EXT_S_CMD_SET] == 0)?"Standard MMC":"Allocated by MMCA");

#ifdef DEBUG_DATA		
                            debug("Finished Data Collection Test\n");
                            for(i = 0; i < 150; i++)
                                debug("array values[%d]=%x%c",i,info.data_buffer[i],((i % 6) == 0)?'\n':'\t');
                            for(i = 150; i < 250; i++)
                                debug("array values[%d]=%x%c",i,info.data_buffer[i],((i % 6) == 0)?'\n':'\t');
                            for(i = 250; i < 512; i++)
                                debug("array values[%d]=%x%c",i,info.data_buffer[i],((i % 6) == 0)?'\n':'\t');
#endif	
                            state = MMC_SWITCH;

                            /*FREEEEEING THE BUFFER AND SETTING BUFFER TO 0xEE to make sure its not corrupted*/
                            for(i = 0 ; i< DATA_BUFFER_SIZE; i++){
                                info.data_buffer[i] = 0xee;
                            }

                            /*FREEING THE POINTER*/
                            free(info.data_buffer);

                            info.data_buffer = NULL;
#ifdef DEBUG_DATA		
                            debug("Freed the EXT_CSD data buffer allocated dynamically\n");
#endif

                        }
                        else{
                            state = MMC_GO_IDLE_STATE;
                            printf("error: No response received in MMC_SEND_EXT_CSD STATE\n");
                            flag = 0;
                        }
                    }
                    else{
                        state = MMC_GO_IDLE_STATE;
                        printf("error:MMC_SEND_CSD\n");
                        flag = 0;
                    }
                    break;
                case MMC_SWITCH: 
                    /*Issue command 6*/
                    debug("Sending CMD6:MMC_SWITCH\n");
                    if(!(send_command(&cmd6, MMC_FAST_CLOCK))){

                        info.rsp_ptr = &response; 
                        info.ident_mode = MMC_FAST_CLOCK;
                        info.rtype = RESPONSE_R1B;
                        info.ctype = CARD_STATE_TRAN;
                        info.data_buffer = NULL;

                        retval = (get_response(&info)); 

                        if(!(retval)){
                            debug("Programmed now in 8bit mode,havnt verified\n");
                            /*TO DO:Verify status mode to test*/
                            //state = MMC_VERIFY_STATUS;
                            state = MMC_SET_BLOCK_LENGTH;
                        }
                        else{
                            state = MMC_GO_IDLE_STATE;
                            printf("error: No response received in MMC_SWITCH\n");
                            flag = 0;

                        }
                    }
                    else{
                        state = MMC_GO_IDLE_STATE;
                        printf("error: Send Error:MMC_SWITCH\n");
                        flag = 0;

                    }
                    break;
                case MMC_VERIFY_STATUS: 
                    /*Issue command 13*/
                    debug("Sending CMD13:MMC_VERIFY_STATUS\n");

                    if(!(send_command(&cmd13, MMC_FAST_CLOCK))){

                        info.rsp_ptr = &response; 
                        info.ident_mode = MMC_FAST_CLOCK;
                        info.rtype = RESPONSE_R1;
                        info.ctype = CARD_STATE_TRAN;
                        info.data_buffer = NULL;

                        retval = (get_response(&info)); 

                        if(!(retval)){
                            debug("Verifying 8-bit mode configured\n");
                            debug("Bus width mode=[%d]\n",info.data_buffer[EXT_CSD_BUS_WIDTH]);
                            debug("Card Type=[%d]\n",info.data_buffer[EXT_CSD_CARD_TYPE]);
                            debug("S_CMD_SET=[%d]\n",info.data_buffer[EXT_S_CMD_SET]);
                            state = MMC_SET_BLOCK_LENGTH;
                        }
                        else{
                            state = MMC_GO_IDLE_STATE;
                            printf("error: No response received in MMC_SWITCH\n");
                            flag = 0;

                        }
                    }
                    else{
                        state = MMC_GO_IDLE_STATE;
                        printf("error: Send Error:MMC_SWITCH\n");
                        flag = 0;


                    }
                    break;
                case MMC_SET_BLOCK_LENGTH: 
                    /*Issue command 16*/
                    debug("Sending CMD16:MMC_SET_BLOCK_LENGTH\n");

                    if(!(send_command(&cmd16, MMC_FAST_CLOCK))){

                        info.rsp_ptr = &response; 
                        info.ident_mode = MMC_FAST_CLOCK;
                        info.rtype = RESPONSE_R1;
                        info.ctype = CARD_STATE_TRAN;
                        info.data_buffer = NULL;

                        retval = (get_response(&info)); 

                        if(!(retval)){
                            debug("Block length set to 512bytes\n");
                            state = MMC_READ_SINGLE_BLOCK;
                        }
                        else{
                            state = MMC_GO_IDLE_STATE;
                            printf("error: No response received in MMC_SET_BLOCK_LENGTH\n");
                            flag = 0;

                        }
                    }
                    else{
                        state = MMC_GO_IDLE_STATE;
                        printf("error:MMC_SET_BLOCK_LENGTH\n");
                        flag = 0;
                    }
                    break;
                case MMC_READ_SINGLE_BLOCK: 
                    /*Issue command 17:Reads a block of the size selected by the SET_BLOCK_LENGTH command*/
                    debug("Sending CMD17\n");

                    if(!(send_command(&cmd17, MMC_FAST_CLOCK))){

                        info.rsp_ptr = &response; 
                        info.ident_mode = MMC_FAST_CLOCK;
                        info.rtype = RESPONSE_R1;
                        info.ctype = CARD_STATE_TRAN;

                        info.data_buffer = (u8 *)malloc(DATA_BUFFER_SIZE);
                        /*8 bit mode tested,outputs 0xfe*/
                        //info.bus_width = EIGHT_BIT_MODE;
                        info.bus_width = ONE_BIT_MODE;

                        /*Check whether successful in allocation*/
                        if(info.data_buffer == NULL){
                            printf("error: Data Buffer Allocation:Unsuccessful");
                            retval = -1;	
                        }

                        retval = (get_response(&info)); 

                        if(!(retval)){
                            debug("Reading Single Block Completed\n");
                            for(i = 0; i < 150; i++)
                                debug("array values[%d]=%x%c",i,info.data_buffer[i],((i % 6) == 0)?'\n':'\t');
                            for(i = 150; i < 250; i++)
                                debug("array values[%d]=%x%c",i,info.data_buffer[i],((i % 6) == 0)?'\n':'\t');
                            for(i = 250; i < 512; i++)
                                debug("array values[%d]=%x%c",i,info.data_buffer[i],((i % 6) == 0)?'\n':'\t');

                            free(info.data_buffer);
                            debug("Freed the data buffer allocated dynamically\n");

                            state = MMC_STOP_TRANSMISSION;
                            flag = 0;
                        }
                        else{
                            state = MMC_GO_IDLE_STATE;
                            printf("error: No response received in MMC_READ_SINGLE_BLOCK\n");
                            flag = 0;

                        }
                    }
                    else{
                        state = MMC_GO_IDLE_STATE;
                        debug("Send Error:MMC_READ_SINGLE_BLOCK\n");
                        flag = 0;


                    }
                    break;
                /*TO DO:Test STOP_TRANSMISSION*/    
                case MMC_STOP_TRANSMISSION: 
                    /*Issue command 12*/
                    printf("Sending CMD12\n");

                    if(!(send_command(&cmd12, MMC_FAST_CLOCK))){

                        info.rsp_ptr = &response; 
                        info.ident_mode = MMC_FAST_CLOCK;
                        info.rtype = RESPONSE_R1;
                        info.ctype = CARD_STATE_TRAN;
                        info.data_buffer = NULL;

                        retval = (get_response(&info)); 

                        if(!(retval)){
                            debug("Stop Transmission Complete:OPERATION COMPLETE\n");
                            state = INACTIVE;
                            flag = 0;
                        }
                        else{
                            state = MMC_GO_IDLE_STATE;
                            printf("error: No response received in MMC_STOP_TRANSMISSION\n");
                            flag = 0;

                        }
                    }
                    else{
                        state = MMC_GO_IDLE_STATE;
                        printf("error: MMC_STOP_TRANSMISSION\n");
                        flag = 0;


                    }
                    break;
                        case INACTIVE:
                    return 1;
                    //break;
                        default:
                    break;
                    }
                    }
                    return 0;
            }
            else
            {
                printf("error: MMC CARD not inserted\n");
                return 1;
            }
        }

/*MMC Functions included to match the calls to mmc.c*/

        int mmc_read(ulong src, uchar *dst, int size)
        {
            printf("Dummy mmc_read\n");
            return 0;
        }

        int mmc_write(uchar *src, ulong dst, int size)
        {
            printf("Dummy mmc_write\n");
            return 0;
        }

        int mmc2info(ulong addr)
        {
            printf("Dummy mmc2_info\n");
            return 0;
        }

#endif
