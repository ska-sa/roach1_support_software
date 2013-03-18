#ifndef HOLO_CONFIG_H_
#define HOLO_CONFIG_H_

/* path to search for bof files in raw mode */
#define POCO_RAW_PATH       "/boffiles"

/* image to load when entering holo mode */
#define POCO_HOLO_IMAGE     POCO_RAW_PATH "/holo.bof"
//#define POCO_HOLO_IMAGE     POCO_RAW_PATH "/holo-2010-05-04-r3021-morebits"

#define HOLO_BASELINE_VALUE 1

//#define HOLO_ACC_BLOCKS  6 
/*Changed to 8 for software crowd compatability*/
#define HOLO_ACC_BLOCKS  8 

/* how many seconds to wait before giving up on an upload */
#define POCO_UPLOAD_TIMEOUT        60

/* processing clock in Hz, the rate at which the counters tick over */
#define HOLO_DSP_CLOCK      524000000

#define HOLO_DOWNSCALE_FACTOR  18

#define HOLO_DEFAULT_ACCUMULATION 500 

#define MSECPERSEC_HOLO 1000UL
#define USECPERSEC_HOLO 1000000UL
#define NSECPERSEC_HOLO 1000000000UL

#define HOLO_FFT_WINDOW  512
#define HOLO_ANT_COUNT     2
#define HOLO_CENTER_FREQUENCY 57023437/*58MHz*/
#define HOLO_TIMESTAMP_SCALE_FACTOR (8192 * 16)

#define HOLO_TOTAL_BANDWIDTH (HOLO_DSP_CLOCK )/(16 * 16)

/* the factor by which the ADC is faster than the DSP clock */
#define POCO_ADC_DSP_FACTOR         4

#define POCO_LO_FREQ_MIN     1000000000UL
#define POCO_LO_FREQ_MAX     2500000000UL

/* number of times we attempt to get into the middle of a second */
#define POCO_RETRY_ATTEMPTS         8

/* how many times we read pps offset in order to compute average */
#define POCO_BENCHMARK_TRIES       16

/* how many times we try to get valid times from gateware to check sync */
#define POCO_TIME_CHECK_TRIES        3
/* time (in us) by which cpu and fpga may differ */
#define POCO_TIME_TOLERANCE      25000 

#define HOLO_FFT_WINDOW           512
#define POCO_PRE_ACCUMULATION     128

/* the sync value has to be a multiple of this */
#define POCO_SYNC_MULTIPLE       8192
#define POCO_DEFAULT_SYNC_PERIOD   ((POCO_DSP_CLOCK / POCO_SYNC_MULTIPLE) * POCO_SYNC_MULTIPLE)

#define HOLO_DEFAULT_SYNC_MODE      1  /* 1 for manual mode, generate own pps *//*IMP: Set this to 1*/


#define POCO_ANT_COUNT              2
#define POCO_POLARISATION_COUNT     2
#define POCO_CHANNEL_COUNT          POCO_FFT_WINDOW

/* maxium coarse delay (units: 1/POCO_DSP_CLOCK) */
#define POCO_MAX_COARSE_DELAY    2048

/* give ourselves 20ms before an event needs to take */
#define POCO_DEFAULT_LEAD       20000

/* give ourselves 20ms before an event needs to take */
#define HOLO_DEFAULT_LEAD       20000

#endif
