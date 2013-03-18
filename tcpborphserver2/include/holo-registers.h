#ifndef HOLO_REGISTERS_H_
#define HOLO_REGISTERS_H_

/********************************************************************/
#define     CONTROL_HOLO_REGISTER    "control"
#define                CLEAR_DONE    (1 << 16) /* data available */
#define                CLEAR_DONEBIT_OVERFLOW (0xF << 16)
#define                CLEAR_OVERRANGE_UNUSED (0xF << 8) 
#define                SET_DELAY_CONTROL_1    (1 << 12) /* set delay register 1  */
#define                SET_DELAY_CONTROL_2    (1 << 13) /* set delay register 2 */
/*WARNING:SETTING ALL THOSE NIBBLE BITS TO 0*/
#define          CLEAR_DELAY_CONTROL(p)         ((p) & 0xffff0fff)   /* pos */
#define           SYSTEM_RESET_CONTROL      0x00000001  /* level */
/********************************************************************/
#define      STATUS_HOLO_REGISTER  "status"
#define       IS_DONE_VACC_STATUS   0x00010000    /* data available */
/********************************************************************/
#define    ACC_LEN_HOLO_REGISTER "acc_len"
#define    ACC_TIMESTAMP_HOLO_REGISTER "acc_timestamp"

#define    IS_RESET_STATUS                   0x00000001

/********************************************************************/
#define SNAP_CTRL  "snap_ctrl"
#define SNAP_ADDR  "snap_addr"
#define SNAP_BRAM_LSB  "snap_bram_lsb"
#define SNAP_BRAM_MSB  "snap_bram_msb"

/*32bit considered only*/
#define ACC_BRAM1 "acc_0x0_real_msb"
#define ACC_BRAM2 "acc_0x0_imag_msb"
#define ACC_BRAM3 "acc_1x1_real_msb"
#define ACC_BRAM4 "acc_1x1_imag_msb"
#define ACC_BRAM5 "acc_0x1_real_msb"
#define ACC_BRAM6 "acc_0x1_imag_msb"
#define ACC_BRAM7 "acc_1x0_real_msb"
#define ACC_BRAM8 "acc_1x0_imag_msb"

#define DELAY_REGISTER_1 "cd_val0"
#define DELAY_REGISTER_2 "cd_val1"

/********************************************************************/

#define           PPS_PERIOD_POCO_REGISTER     "pps_period"
#define       PPS_PERIOD_MAX_POCO_REGISTER "pps_period_max"
#define       PPS_PERIOD_MIN_POCO_REGISTER "pps_period_min"
#define            PPS_COUNT_POCO_REGISTER      "pps_count"
#define           PPS_OFFSET_POCO_REGISTER     "pps_offset"

#endif
