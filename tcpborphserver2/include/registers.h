#ifndef POCO_REGISTERS_H_
#define POCO_REGISTERS_H_


/********************************************************************/
#define               STATUS_POCO_REGISTER      "status"

#define           OVERRANGE_ADC_STATUS(p) ((p)         & 0x0000000f)   
#define     COARSE_DELAY_MISSED_STATUS(p) (((p) <<  4) & 0x000000f0)   
#define           OVERRANGE_FFT_STATUS(p) (((p) <<  8) & 0x00000f00)   
#define           OVERRANGE_QNT_STATUS(p) (((p) << 16) & 0x000f0000)   
#define    COARSE_DELAY_PENDING_STATUS(p) (((p) << 24) & 0x0f000000)   
#define                IS_RESET_STATUS                   0x80000000

/********************************************************************/
#define              CONTROL_POCO_REGISTER      "ctrl"

#define       COARSE_DELAY_ARM_CONTROL(p) ((p)         & 0x0000000f) /* pos */
#define      COARSE_CLEAR_MISS_CONTROL(p) (((p) <<  4) & 0x000000f0) /* pos */
#define              RESET_PPS_CONTROL                   0x00010000  /* pos */
#define               SYNC_ARM_CONTROL                   0x00020000  /* pos */
#define            SYNC_MANUAL_CONTROL                   0x00040000  /* pos */
#define           SYSTEM_RESET_CONTROL                   0x00080000  /* level */
#define              CLEAR_ADC_CONTROL(p) (((p) << 20) & 0x00f00000) /* pos */
#define              CLEAR_FFT_CONTROL(p) (((p) << 24) & 0x0f000000) /* pos */
#define              CLEAR_QNT_CONTROL(p) (((p) << 28) & 0xf0000000) /* pos */

#define              CLEAR_ALL_CONTROL                   0xfff00000

/********************************************************************/
#define              STATUS0_POCO_REGISTER      "status0"

#define           IS_DONE_VACC_STATUS0                   0x00000001    /* data available */
#define IS_BUFFER_OVERFLOW_VACC_STATUS0                  0x00000002    /* permanent internal misalignment */
#define      IS_OVERWRITE_VACC_STATUS0                   0x00000004    /* software not fast enough */
#define IS_ACCUMULATOR_OVERFLOW_VACC_STATUS0             0x00000008    /* values clipped */
#define      IS_READY_PHY_VACC_STATUS0                   0x00000010    /* major failure if not set */

#define     FINE_DELAY_PENDING_STATUS0(p) (((p) << 20) & 0x00f00000)
#define      FINE_DELAY_MISSED_STATUS0(p) (((p) << 24) & 0x0f000000)

/********************************************************************/
#define             CONTROL0_POCO_REGISTER      "ctrl0"

#define    SOURCE_SELECT_SNAP_CONTROL0(p)         ((p) & 0x000000f0)   /* level */
#define    SOURCE_SELECT_VACC_CONTROL0                   0x00010000    /* level */

#define          CLEAR_SOURCE_CONTROL0(p)         ((p) & 0xfffff00f)   /* pos */

#define            SOURCE_ADC_CONTROL0                   0x00000000
#define          SOURCE_QUANT_CONTROL0                   0x00000100

#define               SOURCE0_CONTROL0                   0x00000000
#define               SOURCE1_CONTROL0                   0x00000010
#define               SOURCE2_CONTROL0                   0x00000020
#define               SOURCE3_CONTROL0                   0x00000030

#define        FINE_DELAY_ARM_CONTROL0(p) (((p) << 20) & 0x00f00000)   /* pos */
#define       FINE_CLEAR_MISS_CONTROL0(p) (((p) << 24) & 0x0f000000)   /* pos */

/********************************************************************/
#define            SNAP_CTRL_POCO_REGISTER      "snap_ctrl"
#define            SNAP_ADDR_POCO_REGISTER      "snap_addr"
#define        SNAP_BRAM_LSB_POCO_REGISTER  "snap_bram_lsb"
#define        SNAP_BRAM_MSB_POCO_REGISTER  "snap_bram_msb"

#define       PPS_PERIOD_MAX_POCO_REGISTER "pps_period_max"
#define       PPS_PERIOD_MIN_POCO_REGISTER "pps_period_min"
#define            PPS_COUNT_POCO_REGISTER      "pps_count"
#define           PPS_OFFSET_POCO_REGISTER     "pps_offset"

#define        ACC_TIMESTAMP_POCO_REGISTER  "acc_timestamp"
#define              ACC_LEN_POCO_REGISTER        "acc_len"

#define          DRAM_MEMORY_POCO_REGISTER    "dram_memory"

#define            FFT_SHIFT_POCO_REGISTER      "fft_shift"

#define          LD_MSW_SKEL_POCO_REGISTER  "ld_time_msw%d"
#define          LD_LSW_SKEL_POCO_REGISTER  "ld_time_lsw%d"

#define       LD_STATUS_SKEL_POCO_REGISTER "delay_tr_status%d"

#define          COARSE_SKEL_POCO_REGISTER "coarse_delay%d"
#define            FINE_SKEL_POCO_REGISTER   "fine_delay%d"
#define           PHASE_SKEL_POCO_REGISTER        "phase%d"

#define            GAIN_SKEL_POCO_REGISTER           "eq%d"

#if 0
#define          CD_MSB_SKEL_POCO_REGISTER "cd_tr%d_ld_time_msb"
#define          CD_LSB_SKEL_POCO_REGISTER "cd_tr%d_ld_time_lsb"
#endif

#if 0
#define          FD_MSB_SKEL_POCO_REGISTER "fd_tr%d_ld_time_msb"
#define          FD_LSB_SKEL_POCO_REGISTER "fd_tr%d_ld_time_lsb"
#endif

#if 0
#define VACC_DONE_POCO_FLAG  0x01
#define VACC_OF_POCO_FLAG    0x02 
#define VACC_ERROR_POCO_FLAG 0x04
#define VACC_READY_POCO_FLAG 0x08
#define VACC_EN_POCO_FLAG    0x08
#define VACC_RST_POCO_FLAG   0x10

#define TIMING_ARM_FLAG      0x01
#define TIMING_TRIGGER_FLAG  0x02
#endif

#endif
