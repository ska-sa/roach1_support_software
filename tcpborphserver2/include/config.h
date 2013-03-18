#ifndef POCO_CONFIG_H_
#define POCO_CONFIG_H_

/* what is reported when a client connects */
#define TCPBORPHSERVER_BUILD      "tcpborphserver-" VERSION

/* path to search for bof files in raw mode */
#define POCO_RAW_PATH             "/boffiles"

/* image to load when entering poco mode */
#define POCO_POCO_IMAGE            POCO_RAW_PATH "/poco.bof"

/* how many seconds to wait before giving up on an upload */
#define POCO_UPLOAD_TIMEOUT        60

/* fudge factor - cycles below which we assume that we see two pps pulses */
#define POCO_DSP_CLOCK_TOL          5

/* processing clock in Hz, the rate at which the counters tick over */
#define POCO_DSP_CLOCK      200000000
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

#define POCO_FFT_WINDOW           512
#define POCO_PRE_ACCUMULATION     128

/* the sync value has to be a multiple of this */
#define POCO_SYNC_MULTIPLE       8192
#define POCO_DEFAULT_SYNC_PERIOD   ((POCO_DSP_CLOCK / POCO_SYNC_MULTIPLE) * POCO_SYNC_MULTIPLE)

#define POCO_DEFAULT_SYNC_MODE          0  /* 0 to use external pps, 1 for manual where we generate own pps */
#define POCO_DEFAULT_UPDATE_BEHAVIOUR   1  /* 0 for initial, 1 for continuous */
#define POCO_DEFAULT_FFT_SHIFT 0xffffffff  /* fft shift pattern */

#define POCO_DEFAULT_ACCUMULATION 100

#define POCO_ANT_COUNT              2
#define POCO_POLARISATION_COUNT     2
#define POCO_CHANNEL_COUNT          POCO_FFT_WINDOW

/* maxium coarse delay (units: 1/POCO_DSP_CLOCK) */
#define POCO_MAX_COARSE_DELAY    2048

#define POCO_FINE_DELAY_TABLE   65536

/* give ourselves 20ms before an event needs to take */
#define POCO_DEFAULT_LEAD       20000

/* */

#define FINE_DELAY_BITS            16
#define COARSE_DELAY_BITS          16
#define FINE_DELAY_RATE_BITS       16
#define FRINGE_OFFSET_BITS         16
#define FRINGE_RATE_BITS           16

#define FIXER_SCHEDULE             23

#endif
