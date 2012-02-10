#ifndef POCO_OPTIONS_H_
#define POCO_OPTIONS_H_

#define MAGIC_HEADER_POCO    0x4b52
#define VERSION_HEADER_POCO        0x0002

#define                      NULL_OPTION_POCO 0x0000

#define           INSTRUMENT_TYPE_OPTION_POCO 0x0001
#define       NULL_INSTRUMENT_TYPE_POCO             0x0000
#define       POCO_INSTRUMENT_TYPE_POCO             0x0001
#define    COMPLEX_INSTRUMENT_TYPE_POCO                   0x80000000
#define  BIGENDIAN_INSTRUMENT_TYPE_POCO                   0x40000000
#define  UINT_TYPE_INSTRUMENT_TYPE_POCO      ((0 << 24) & 0x3f000000)
#define  SINT_TYPE_INSTRUMENT_TYPE_POCO      ((1 << 24) & 0x3f000000)
#define FLOAT_TYPE_INSTRUMENT_TYPE_POCO      ((2 << 24) & 0x3f000000)

#define SET_VALBITS_INSTRUMENT_TYPE_POCO(p) (((p) << 16) & 0x00ff0000) 
#define GET_VALBITS_INSTRUMENT_TYPE_POCO(v) (((v) >> 16) & 0x000000ff)

#define SET_VALITMS_INSTRUMENT_TYPE_POCO(p) (((p)      ) & 0x0000ffff)
#define GET_VALITMS_INSTRUMENT_TYPE_POCO(v) (((v)      ) & 0x0000ffff)


#define               INSTANCE_ID_OPTION_POCO 0x0002
#define                 TIMESTAMP_OPTION_POCO 0x0003
#define            PAYLOAD_LENGTH_OPTION_POCO 0x0004
#define            PAYLOAD_OFFSET_OPTION_POCO 0x0005
#define           ADC_SAMPLE_RATE_OPTION_POCO 0x0007
#define        FREQUENCY_CHANNELS_OPTION_POCO 0x0009
#define                  ANTENNAS_OPTION_POCO 0x000a
#define                 BASELINES_OPTION_POCO 0x000b

#define            STREAM_CONTROL_OPTION_POCO 0x000d
#define                    LAST_STREAM_CONTROL_POCO 0x8000
#define                         START_STREAM_CONTROL_POCO 0x00000000
#define                          SYNC_STREAM_CONTROL_POCO 0x00000001
#define                          STOP_STREAM_CONTROL_POCO 0x00000002
#define                        CHANGE_STREAM_CONTROL_POCO 0x00000003

#define             META_COUNTER_OPTION_POCO 0x000e
#define                SYNC_TIME_OPTION_POCO 0x000f
#define             BANDWIDTH_HZ_OPTION_POCO 0x0013
#define            ACCUMULATIONS_OPTION_POCO 0x0015
#define          TIMESTAMP_SCALE_OPTION_POCO 0x002f


struct header_poco{
  uint16_t h_magic;
  uint16_t h_version;
  uint16_t h_reserved;
  uint16_t h_options;
} __attribute__ ((packed));

struct option_poco{
  uint16_t o_label;
  uint16_t o_msw;
  uint32_t o_lsw;
} __attribute__ ((packed));

#endif
