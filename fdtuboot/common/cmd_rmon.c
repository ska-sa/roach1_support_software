/*
 * CREATED : 25 June 2008
 * Shanly Rajan <shanly.rajan@ska.ac.za> 
 * Dave George <dgeorgester@gmail.com> 
*/

#include <common.h>
#include <command.h>
#include <i2c.h>

#include <roach/roach_bsp.h>

#define I2C_ROACH_MONITOR         0x0F

/*ROACH MONITOR Register addresses*/
#define FROM_ACM_CONFIG           0x80
#define ACM_MEM_OFFSET            0xC0
#define VALUE_STORAGE_OFFSET      0x240

#define SOURCE_PRESCALER     0x0
#define SOURCE_DIRECT        0x1
#define SOURCE_CURRENT       0x2
#define SOURCE_TEMPERATURE   0x3

#define RM_PRESCALE(x)          ((x) & (0x7 << 0))

#define RM_VSOURCE(x)           (((x) & (0x1 << 3))>>3)                   
#define RM_CSOURCE(x)           (((x) & (0x3 << 3))>>3)
#define RM_TSOURCE(x)           ((((x) & (0x3 << 3))>>3) == 0x2 ? SOURCE_TEMPERATURE : (((x) & (0x3 << 3))>>3))

#define RM_CURRENT_MON_EN(x)     ((x) & (0x1 << 4))
#define RM_TEMPERATURE_MON_EN(x) ((x) & (0x1 << 0))

/*daves r/w settings*/
#define ROACH_MONITOR_NOP         0x0
#define ROACH_MONITOR_READ        0x1
#define ROACH_MONITOR_WRITE       0x2
#define ROACH_MONITOR_TEST        0x3
#define ROACH_MONITOR_HEALTH      0x4
#define ROACH_MONITOR_PING        0x8 


#define RM_SAMPLE_TYPE_VOLTAGE     0
#define RM_SAMPLE_TYPE_CURRENT     1
#define RM_SAMPLE_TYPE_TEMPERATURE 2
#define RM_SAMPLE_TYPE_INVALID     3

#define PRESCALER_16V   0x0
#define PRESCALER_8V    0x1
#define PRESCALER_4V    0x2
#define PRESCALER_2V    0x3
#define PRESCALER_1V    0x4
#define PRESCALER_0V5   0x5
#define PRESCALER_0V25  0x6
#define PRESCALER_0V125 0x7

const unsigned int sense_conductance_value[10] = {    1, 1, 100, 213, 100,
                                                   1000, 1, 213, 455, 200};
                                          
typedef struct {
    unsigned int val;
    unsigned char type;
}rm_sample;

typedef struct{
    unsigned char AV;
    unsigned char AC;
    unsigned char AG;
    unsigned char AT;
}analog_quad;

int monitor_get_val(unsigned short addr)
{
  unsigned char buf[3]; 
  unsigned short data;

  /*Write with the register value you want to access,standard is offset*/
  buf[0] =  ROACH_MONITOR_READ; 
  buf[1] =  (addr & 0xFF);
  buf[2] =  ((addr & 0xFF00) >> 8);       


  /*Check whether making the addr and addrlen to 0 and writing from the buffer onlyie skip the addr part */
  //i2c_write(I2C_ROACH_MONITOR,addr,2,buf,3);
  i2c_write(I2C_ROACH_MONITOR,0,0,buf,3);

  /*Read the short value from register into buffer*/
 // i2c_read(I2C_ROACH_MONITOR,addr,2,buf,2);
  i2c_read(I2C_ROACH_MONITOR,0,0,buf,3);
  if(buf[0] != 0x1){
	  printf("ROACH MONITORING NO ACK RECEIVED(monitor_get_val)\n");
	  return -1;
  }
  data = (buf[1] | (buf[2] << 8));
 // data = (buf[0] | (buf[1] << 8));

  return data;
}

int monitor_set_val(unsigned short addr ,const unsigned short data)
{
	unsigned char buf[5];

	/*Set the buffer with the values to be written out on the i2c bus*/
	buf[0] = ROACH_MONITOR_WRITE;
	buf[1] = (addr & 0xFF);
	buf[2] = ((addr & 0xFF00) >> 8);
	buf[3] = (data & 0xFF);
	buf[4] = ((data & 0xFF00) >> 8);

	i2c_write(I2C_ROACH_MONITOR,0,0,buf,5);

	i2c_read(I2C_ROACH_MONITOR,0,0,buf,1);
	if(buf[0] != 0x1){
		printf("ROACH MONITORING NO ACK RECEIVED(monitor_set_val)\n");
		return -1;
	}

	return 0;
}

unsigned char rw_np(const char *s)
{
	if (strcmp(s, "r") == 0 || strcmp(s, "R") == 0) {
		return (1);
	}
	else if (strcmp(s, "w") == 0 || strcmp(s,"W") == 0) {
		return (2);
	}
	else if (strcmp(s, "t") == 0 || strcmp(s,"T") == 0) {
		return (3);
	}
	else if (strcmp(s, "h") == 0 || strcmp(s,"H") == 0) {
		return (4);
	}
	else if (strcmp(s, "p") == 0 || strcmp(s, "P") == 0) {
		return (8);
	}
	else 
		return (0);
}
/*
int i2c_read(uchar chip, uint addr, int alen, uchar *buffer, int len);
int i2c_write(uchar chip, uint addr, int alen, uchar *buffer, int len);
*/

int monitor_set_block(unsigned char cmd,unsigned short addr,unsigned short data)
{
	unsigned char buf[5];
	int data_val,data_val_temp;

	switch(cmd){
		case ROACH_MONITOR_NOP:
			printf("ISSUING ROACH_MONITOR_NOP COMMAND\n");
			return 0;
		case ROACH_MONITOR_READ:
			printf("ISSUING ROACH_MONITOR_READ COMMAND\n");
			data_val = monitor_get_val(addr);
			if(data_val == -1){
				return -1;
			}
			printf("The read data is %04x\n",data_val);
			return 0;
		case ROACH_MONITOR_WRITE:
			printf("ISSUING ROACH_MONITOR_WRITE COMMAND\n");
			if(monitor_set_val(addr,data)){
				printf("Writes not succeeding\n");
				return -1;
			}
			return 0;
		case ROACH_MONITOR_TEST:
			printf("rmon TEST SUITE\n");
			/*Reading the board ID:0xBEEF*/
			data_val = monitor_get_val(RBSP_SYS_ID / 2);
			if(data_val == -1){
				return -1;
			}
			if(data_val == 0xBEEF)
				printf("BOARD ID:Pass\n");
			/*Reading revision numbers*/
			data_val = monitor_get_val(RBSP_SYS_MAJOR / 2);
			if(data_val == -1){
				return -1;
			}
			printf("REV_MAJOR:%04x\n", data_val);
			data_val = monitor_get_val(RBSP_SYS_MINOR / 2);
			if(data_val == -1){
				return -1;
			}
			printf("REV_MINOR:%04x\n", data_val);

			data_val = monitor_get_val(RBSP_SYS_RCS / 2);
			if(data_val == -1){
				return -1;
			}
			printf("REV_RCS:%04x\n", data_val);

			data_val = monitor_get_val(0x04);
			if(data_val == -1){
				return -1;
			}
			printf("Default SYS_CONFIG:%04x\n", data_val);
			printf("Writing 0x45 to SYS_CONFIG to test write\n");
			if(monitor_set_val(0x05, 0x45)){
				printf("Writes not succeeding\n");
				return -1;
			}
			data_val_temp = monitor_get_val(0x05);
			if(data_val_temp == -1){
				return -1;
			}
			printf("Reading SYS_CONFIG:%04x\n", data_val_temp);
			if(data_val_temp == 0x45)
				printf("SYS_CONFIG Write and Read Matching\n");
			printf("Restoring Default SYS_CONFIG:%04x\n",data_val);
			if(monitor_set_val(0x05, data_val)){
				printf("Writes not succeeding\n");
				return -1;
			}
			return 0;
		case ROACH_MONITOR_HEALTH:
			printf("ROACH MONITOR HEALTH STATUS\n");
			printf("Regretfully under construction\n");
			return 0;
		case ROACH_MONITOR_PING:
			/*write a ping command,NO ACK and read the data back*/
			printf("ISSUING ROACH_MONITOR_PING COMMAND\n");
			buf[0] = ROACH_MONITOR_PING;
			buf[1] = (addr & 0xFF);
			buf[2] = ((addr & 0xFF00) >> 8);
			buf[3] = (data & 0xFF);
			buf[4] = ((data & 0xFF00) >> 8);

			i2c_write(I2C_ROACH_MONITOR,0,0,buf,5);

			i2c_read(I2C_ROACH_MONITOR,0,0,buf,2);
			data_val_temp = (buf[0] | (buf[1] << 8));

			if(data_val_temp == 0x08){
				printf("Pinging Successful\n");
				return 0;
			}
			else{
				printf("Pinging Unsuccessful\n");
				return -1;
			}
		default:
			return 0;

	}
}



#define DMULT 10000

rm_sample get_rm_val(unsigned char adc_source, unsigned char prescaler,
                        unsigned char channel, unsigned short sample_value)
{
    /* TODO: is it 2.56 -> 0 or 2.56 to -0.2 ? */
    rm_sample ret;
    switch (adc_source){
        case SOURCE_DIRECT:
            ret.val = (25600 * (((unsigned int)(sample_value))))/4095;
            ret.type = RM_SAMPLE_TYPE_VOLTAGE;
            return ret;
        case SOURCE_PRESCALER:
            switch (prescaler){
                case PRESCALER_16V:
                    /*NOTE:Error compensation as specified in Fusion Datasheet page 2-104*/
                    sample_value += (2 * 4);
                    ret.val = (((unsigned int)(sample_value) * 163680) / (4095));
                    break;
                case PRESCALER_8V:
                    sample_value += (5 * 4);
                    ret.val = (((unsigned int)(sample_value) * 81840) / (4095));
                    break;
                case PRESCALER_4V:
                    sample_value += (9 * 4);
                    ret.val = (((unsigned int)(sample_value) * 40920) / (4095));
                    break;
                case PRESCALER_2V:
                    sample_value += (19 * 4);
                    ret.val = (((unsigned int)(sample_value) * 20460) / (4095));
                    break;
                case PRESCALER_1V:
                    sample_value += (7 * 4);
                    ret.val = (((unsigned int)(sample_value) * 10230) / (4095));
                    break;
                case PRESCALER_0V5:
                    sample_value += (12 * 4);
                    ret.val = (((unsigned int)(sample_value) * 5115) / (4095));
                    break;
                case PRESCALER_0V25:
                    sample_value += (14 * 4);
                    ret.val = (((unsigned int)(sample_value) * 2557) / (4095));
                    break;
                /* case PRESCALER_0V125: */
                default : 
                    sample_value += (29 * 4);
                    ret.val = (((unsigned int)(sample_value) * 1278) / (4095));
                    break;
            }
            ret.type = RM_SAMPLE_TYPE_VOLTAGE;
            return ret;
        case SOURCE_CURRENT:
            ret.val = ((unsigned int)(sample_value) * 2560 * sense_conductance_value[channel]) / (4095);
            ret.type = RM_SAMPLE_TYPE_CURRENT;
            return ret;
        case SOURCE_TEMPERATURE:
            ret.val = ((unsigned int)(sample_value) * 2560 * 4) / (4095) - 273;
            ret.type = RM_SAMPLE_TYPE_TEMPERATURE;
            return ret;
        default:
            ret.val = -1;
            ret.type = RM_SAMPLE_TYPE_INVALID;
            return ret;
    }
}

#define SAMPLE_AVERAGING 32
int do_roachmonitor (cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
  unsigned int i, j;
  unsigned int accum;
  unsigned short data_val;
  unsigned short sample_val;
  unsigned char cmd;
  unsigned short addr;
  unsigned short data;
  rm_sample rm_val;

  /*cases
   * argc : 0
   * argc : >1
   * argc <= 3*/
  if (argc > 2 && argc <= 4) {

	  cmd  = rw_np(argv[1]);
	  addr = simple_strtoul(argv[2], NULL, 16);
	  data = simple_strtoul(argv[3], NULL, 16);
#ifdef RMON_DEBUG
	  printf ("echoing the cmd=%02x,addr=%04x,data=%04x\n",cmd,addr,data);
#endif
	  monitor_set_block(cmd,addr,data);

	  return 0;
  } else if(argc == 2){

	  cmd  = rw_np(argv[1]);

	  if(cmd == 0x3){

		  addr = simple_strtoul(argv[2], NULL, 16);
		  data = simple_strtoul(argv[3], NULL, 16);

#ifdef RMON_DEBUG
		  printf ("echoing the cmd=%02x,addr=%04x,data=%04x\n",cmd,addr,data);
#endif
		  monitor_set_block(cmd,addr,data);

      return 0;
	  }
	  else if(cmd == 0x4){

		  addr = simple_strtoul(argv[2], NULL, 16);
		  data = simple_strtoul(argv[3], NULL, 16);

#ifdef RMON_DEBUG
		  printf ("echoing the cmd=%02x,addr=%04x,data=%04x\n",cmd,addr,data);
#endif
		  monitor_set_block(cmd,addr,data);

      return 0;
	  }
	  else if(cmd == 0x8){
#ifdef RMON_DEBUG
		  printf ("echoing the cmd=%02x,addr=%04x,data=%04x\n",cmd,0x00,0x08);
#endif
		  monitor_set_block(ROACH_MONITOR_PING, 0x00, 0x00);
      return 0;
	  }
	  else{
		  printf ("Usage:\n%s\n", cmdtp->usage);
		  return 1;
	  }
  } else if(argc == 1){

	  printf("ROACH MONITORING STARTS NOW\n");

	  if(i2c_probe(I2C_ROACH_MONITOR)){
		  printf("ROACH CHIP NOT FOUND\n");
		  return -1;
	  }

	  data_val = monitor_get_val(RBSP_SYS_ID / 2);
	  printf("ROACH BOARD ID:%x\n",data_val);

	  /*Dump the FLASH ROM Register(Read Only) which contains the default ACM configuration that gets loaded to the ACM */
	  /*40 config values ordered into 10 quads:40->0x28*/
	  printf("\nFLASH ROM Register Dump\n");
	  for(i = 0 ; i < 40; i++){
		  data_val = monitor_get_val(FROM_ACM_CONFIG + 1 + i);          
		  printf("%c%03x", (i % 16) ? ' ' : '\n', data_val);
	  }

	  /*Dump the ACM Register(RW) and configure
	   * For each Analog quad we have 3 output values to be computed
	   * Analog Quad i/ps:AV0,AC0,AG0,AT0 
	   * */

	  //V5 temp
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 0*4 + 0, 0x80); //default config - disconnected
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 0*4 + 1, 0x80); //default config - disconnected
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 0*4 + 2, 0x81); //Temperature mon on
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 0*4 + 3, 0x10); //temp mon -> ADC, prescaler disabled

	  //NC
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 1*4 + 0, 0x80); //default config - disconnected
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 1*4 + 1, 0x80); //default config - disconnected
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 1*4 + 2, 0x80); //default config - disconnected
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 1*4 + 3, 0x80); //default config - disconnected

	  //12v & PPC temp
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 2*4 + 0, 0x81); //prescaler 8V 
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 2*4 + 1, 0x81); //prescaler 8V -> special condition on 12V
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 2*4 + 2, 0x81); //PPC Temp
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 2*4 + 3, 0x10); //PPC Temp

	  //5V
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 3*4 + 0, 0x91); //prescaler 8V, current monitor on
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 3*4 + 1, 0x10); //current mon -> ADC
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 3*4 + 2, 0x80); //default config - disconnected
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 3*4 + 3, 0x80); //default config - disconnected

	  //3V3
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 4*4 + 0, 0x92); //prescaler 4V 
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 4*4 + 1, 0x10); //current mon -> ADC
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 4*4 + 2, 0x80); //default config - disconnected
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 4*4 + 3, 0x80); //default config - disconnected

	  //1V0
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 5*4 + 0, 0x93); //prescaler 2V 
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 5*4 + 1, 0x10); //current mon -> ADC
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 5*4 + 2, 0x80); //default config - disconnected
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 5*4 + 3, 0x80); //default config - disconnected

	  //NC
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 6*4 + 0, 0x80); //default config - disconnected
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 6*4 + 1, 0x80); //default config - disconnected 
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 6*4 + 2, 0x80); //default config - disconnected
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 6*4 + 3, 0x80); //default config - disconnected

	  //1V5
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 7*4 + 0, 0x93); //prescaler 2V 
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 7*4 + 1, 0x10); //current mon -> ADC
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 7*4 + 2, 0x80); //default config - disconnected
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 7*4 + 3, 0x80); //default config - disconnected

	  //1V8
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 8*4 + 0, 0x93); //prescaler 2V 
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 8*4 + 1, 0x10); //current mon -> ADC
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 8*4 + 2, 0x80); //default config - disconnected
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 8*4 + 3, 0x80); //default config - disconnected

	  //2V5
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 8*4 + 0, 0x92); //prescaler 4V 
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 8*4 + 1, 0x10); //current mon -> ADC
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 8*4 + 2, 0x80); //default config - disconnected
	  data_val = monitor_set_val(ACM_MEM_OFFSET + 1 + 8*4 + 3, 0x80); //default config - disconnected

	  printf("\nACM Register Dump\n");
	  for(i = 0 ; i < 40; i++){
		  data_val = monitor_get_val(ACM_MEM_OFFSET + 1 + i);          
		  printf("%c%03x", (i % 16) ? ' ' : '\n', data_val);
	  }

	  printf("\nSamples Dump\n");
	  for(i = 0 ; i < 32 ; i++){
		  data_val = monitor_get_val(VALUE_STORAGE_OFFSET + i);
		  printf("%c%04x", (i % 8) ? ' ' : '\n', data_val);
	  }
	  printf("\n");      
	  analog_quad AQs[10];

	  /*Dump the Value Storage Register(Read Only):sample values,32->0x20*/
	  /*Store the values in analog_quads*/
	  for(i = 0 ; i < 10 ; i++){
		  AQs[i].AV = (unsigned char)monitor_get_val(ACM_MEM_OFFSET + 1 + i*4 + 0);
		  AQs[i].AC = (unsigned char)monitor_get_val(ACM_MEM_OFFSET + 1 + i*4 + 1);
		  AQs[i].AG = (unsigned char)monitor_get_val(ACM_MEM_OFFSET + 1 + i*4 + 2);
		  AQs[i].AT = (unsigned char)monitor_get_val(ACM_MEM_OFFSET + 1 + i*4 + 3);
	  }

	  for(i = 0 ; i < 32 ; i++){
		  accum = 0;
		  for (j=0; j < SAMPLE_AVERAGING; j++){
			  accum += monitor_get_val(VALUE_STORAGE_OFFSET + i);
		  }
		  sample_val = accum / 32;
		  switch(i) {
			  case 0:
				  rm_val = get_rm_val(SOURCE_DIRECT, 0, 0, sample_val);
				  break;
			  case 31:
				  rm_val = get_rm_val(SOURCE_TEMPERATURE, 0, 0, sample_val);
				  break;
			  default:
				  switch((i - 1) % 3) {
					  case 0: //voltage case
						  /*adc_source,prescaler,channel,sample_value*/
						  rm_val = get_rm_val(RM_VSOURCE(AQs[((i-1)/3)].AV), RM_PRESCALE(AQs[(i-1)/3].AV),
								  0, sample_val);

						  break;
					  case 1: //current case
						  rm_val = get_rm_val(RM_CSOURCE(AQs[((i-1)/3)].AC), RM_PRESCALE(AQs[(i-1)/3].AC),
								  (i-1)/3, sample_val);
						  /* Check ACM[v0].current_mon_enable if the adc source == SOURCE_CURRENT*/
						  if (RM_CSOURCE(AQs[((i-1)/3)].AC) == SOURCE_CURRENT &&
								  !(RM_CURRENT_MON_EN(AQs[(i-1)/3].AV))){
							  printf("WARNING: current monitoring incorrectly disabled on AV%d\n", (i-1)/3);
							  /* Uncomment below to mark the sample as invalid */
							  //rm_val.type = RM_SAMPLE_TYPE_INVALID;
						  }
						  if (RM_CSOURCE(AQs[((i-1)/3)].AC) != SOURCE_CURRENT &&
								  (RM_CURRENT_MON_EN(AQs[(i-1)/3].AV))){
							  printf("WARNING: current monitoring incorrectly enabled on AV%d\n", (i-1)/3);
							  /* Uncomment below to mark the sample as invalid */
							  //rm_val.type = RM_SAMPLE_TYPE_INVALID;
						  }
						  break;
					  /* case 2: */ //temp case
            default :
						  rm_val = get_rm_val(RM_TSOURCE(AQs[((i-1)/3)].AT), RM_PRESCALE(AQs[(i-1)/3].AT),
								  0, sample_val);

						  /* Check ACM[G0].temp_mon_enable*/
						  if (RM_TSOURCE(AQs[((i-1)/3)].AT) == SOURCE_TEMPERATURE &&
								  !(RM_TEMPERATURE_MON_EN(AQs[(i-1)/3].AG))){
							  printf("WARNING: temperature monitoring incorrectly disabled on AG%d\n", (i-1)/3);
							  /* Uncomment below to mark the sample as invalid */
							  //rm_val.type = RM_SAMPLE_TYPE_INVALID;
						  }
						  if (RM_TSOURCE(AQs[((i-1)/3)].AT) != SOURCE_TEMPERATURE &&
								  (RM_TEMPERATURE_MON_EN(AQs[(i-1)/3].AG))){
							  printf("WARNING: temperature monitoring incorrectly disabled on AG%d\n", (i-1)/3);
							  /* Uncomment below to mark the sample as invalid */
							  //rm_val.type = RM_SAMPLE_TYPE_INVALID;
						  }

						  /* Check for valid prescale*/
						  if (RM_TSOURCE(AQs[((i-1)/3)].AT) == SOURCE_PRESCALER &&
								  !(RM_PRESCALE(AQs[(i-1)/3].AT) == PRESCALER_4V || RM_PRESCALE(AQs[(i-1)/3].AT) == PRESCALER_16V)){
							  printf("WARNING: invalid prescaler on AT%d\n", (i-1)/3);
							  /* Uncomment below to mark the sample as invalid */
							  //rm_val.type = RM_SAMPLE_TYPE_INVALID;
						  }
						  break;
				  }
		  }

		  switch (rm_val.type){
			  case RM_SAMPLE_TYPE_VOLTAGE:
				  printf("channel %d:V[%d]: %d.%dV foo???%d\n",i,(i-1)/3, (rm_val.val / DMULT) , (rm_val.val % DMULT), rm_val.val);
				  break;
			  case RM_SAMPLE_TYPE_CURRENT:
				  printf("channel %d:C[%d]: %d.%dA\n",i,(i-1)/3,(rm_val.val / DMULT) , (rm_val.val % DMULT));
				  break;
			  case RM_SAMPLE_TYPE_TEMPERATURE:
				  printf("channel %d:T[%d]: %d.%dC\n",i,(i-1)/3,(rm_val.val / DMULT) , (rm_val.val % DMULT));
				  break;
			  case RM_SAMPLE_TYPE_INVALID:
				  printf("channel %d: %d.%d*%d\n",i,(i-1)/3,(rm_val.val / DMULT) , (rm_val.val % DMULT));
				  break;
			  default:
				  printf("channel %d: invalid sample type\n",i);
				  break;
		  }
	  }

	  return 0;
  } else{
	  printf ("Usage:\n%s\n", cmdtp->usage);
	  return 1;
  }
} 

/**************************************************************************/

U_BOOT_CMD(
		rmon, 4, 1, do_roachmonitor,
		"rmon cmd [addr data] - roach monitoring\n",
		"   				  - Read V,I and T values to the ACM\n"
		"rmon r/R addr        - Read from specified I2C addr\n"
		"rmon w/W addr data   - Write to  specified I2C addr with data\n"
		"rmon p/P			  - Ping ROACH\n"
		"rmon t/T             - Test suite for ROACH MONITORING\n"
		"rmon h/H		      - ROACH HEALTH STATUS\n"
		);

