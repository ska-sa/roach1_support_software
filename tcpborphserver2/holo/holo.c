#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>

#include <sys/time.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <katcp.h>
#include <katpriv.h> /* for timeval arith */

#include "core.h"
#include "holo.h"
#include "misc.h"
#include "modes.h"
#include "input.h"
#include "holo-registers.h"
#include "holo-config.h"
#include "holo-options.h"
#include "katadc.h"

int enter_mode_holo(struct katcp_dispatch *d, char *flags, unsigned int from)
{
	struct state_holo *sh;

	sh = get_mode_katcp(d, POCO_HOLO_MODE);
	if(sh == NULL){
		return from;
	}
	/*holo bof file gets programmed*/
	if(program_core_poco(d, sh->h_image) < 0){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to program holo bof file");
		return from;
	}

        /*katadc configuration*/
        if( katadc_init(d, 0 ) < 0){
          fprintf(stderr,"main: error initialising adc 0\n");
        }else{
          log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "ADC in slot 0 initialised");
        }
#if 0
	unsigned int rst_value;
	/* reset the whole system before capture starts */
	rst_value = 0x1;
	if(write_name_pce(d, CONTROL_HOLO_REGISTER, &(rst_value), 0, 4) != 4){
		return -1;
	}

        /*Calculating timestamp and setting it for the metaoption*/
        if(read_name_pce(d, ACC_TIMESTAMP_HOLO_REGISTER, &value, 0, 4) != 4){
          return -1;
        }
        sh->h_time_stamp = value * 8192 * 16;
#endif
        return POCO_HOLO_MODE;
}

void leave_mode_holo(struct katcp_dispatch *d, unsigned int to)
{
	struct state_holo *sh;

	sh = get_mode_katcp(d, POCO_HOLO_MODE);
	if(sh == NULL){
		return;
	}

	/* unclear how much should be stopped here: for certain cases one wants a snapshot crashed system, on the other hand things still have to be consistent */
}


void destroy_holo_poco(struct katcp_dispatch *d)
{
	struct state_holo *sh;
	int i;

	sh = get_mode_katcp(d, POCO_HOLO_MODE);
	if(sh == NULL){
		return;
	}

	if(sh->h_image){
		free(sh->h_image);
		sh->h_image = NULL;
	}

	for(i = 0; i < sh->h_size; i++){
		destroy_capture_holo(d, sh->h_captures[i]);
	}
	sh->h_size = 0;

	if(sh->h_captures){
		free(sh->h_captures);
		sh->h_captures = NULL;
	}
}

struct capture_holo *find_capture_holo(struct state_holo *sh, char *name)
{
	int i;

	if(name == NULL){
		return NULL;
	}

	for(i = 0; i < sh->h_size; i++){
		if(!strcmp(sh->h_captures[i]->c_name, name)){
			return sh->h_captures[i];
		}
	}

#ifdef DEBUG
	fprintf(stderr, "find: unable to locate capture %s\n", name);
#endif

	return NULL;
}

/*********************************************************************************************/
int capture_hdestination_cmd(struct katcp_dispatch *d, int argc)
{
	struct state_holo *sh;
	struct capture_holo *ch;
	char *name, *host;
	unsigned long port;
	struct in_addr ina;
	struct hostent *he;

	sh = need_current_mode_katcp(d, POCO_HOLO_MODE);
	if(sh == NULL){
		return KATCP_RESULT_FAIL;
	}

	if(argc <= 3){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need parameters");
		extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
		return KATCP_RESULT_OWN;
	}

	name = arg_string_katcp(d, 1);

	ch = find_capture_holo(sh, name);
	if(ch == NULL){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "no match for name %s", name);
		extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
		return KATCP_RESULT_OWN;
	}

	host = arg_string_katcp(d, 2);
	if(inet_aton(host, &ina) == 0){
		he = gethostbyname(host);
		if((he == NULL) || (he->h_addrtype != AF_INET)){
			log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to convert %s to ipv4 address", host);
			extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
			return KATCP_RESULT_OWN;
		}

		ina = *(struct in_addr *) he->h_addr;
	}

	ch->c_ip = ina.s_addr;

	port = arg_unsigned_long_katcp(d, 3);
	if((port <= 0) || (port > 0xffff)){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "port %lu not in range", port);
		extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
		return KATCP_RESULT_OWN;
	}

	ch->c_port = htons(port);

	log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "destination is %08x:%04x", ch->c_ip, ch->c_port);

	if(init_udp_holo(d, ch)){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to initiate udp connection");
		return KATCP_RESULT_FAIL;
	}

	return KATCP_RESULT_OK;
}


int capture_hlist_cmd(struct katcp_dispatch *d, int argc)
{
	struct state_holo *sh;
	struct capture_holo *ch;
	char *name, *ip;
	unsigned int count, i;
	struct in_addr in;

	sh = need_current_mode_katcp(d, POCO_HOLO_MODE);
	if(sh == NULL){
		return KATCP_RESULT_FAIL;
	}

	if(argc <= 1){
		name = NULL;
	} else {
		name = arg_string_katcp(d, 1);
	}

	count = 0;
	for(i = 0; i < sh->h_size; i++){
		if((name == NULL) || (!strcmp(sh->h_captures[i]->c_name, name))){
			ch = sh->h_captures[i];

			prepend_inform_katcp(d);
			append_string_katcp(d, KATCP_FLAG_STRING, ch->c_name);

			in.s_addr = ch->c_ip;
			ip = inet_ntoa(in);

			if(ip){
				append_string_katcp(d, KATCP_FLAG_STRING, ip);
			} else {
				append_hex_long_katcp(d, KATCP_FLAG_XLONG, (unsigned long) ch->c_ip);
			}

			append_unsigned_long_katcp(d, KATCP_FLAG_ULONG, (unsigned long) ch->c_port);

			if(ch->c_start.tv_sec){
				append_args_katcp(d, 0, "%lu%03lu", ch->c_start.tv_sec, ch->c_start.tv_usec / 1000);
			} else {
				append_unsigned_long_katcp(d, KATCP_FLAG_ULONG, 0UL);
			}

			if(ch->c_stop.tv_sec){
				append_args_katcp(d, KATCP_FLAG_LAST, "%lu%03lu", ch->c_stop.tv_sec, ch->c_stop.tv_usec / 1000);
			} else {
				append_unsigned_long_katcp(d, KATCP_FLAG_LAST | KATCP_FLAG_ULONG, 0UL);
			}

			log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "start=%lu.%06lus stop=%lu.%06lus state=%d", ch->c_start.tv_sec, ch->c_start.tv_usec, ch->c_stop.tv_sec, ch->c_stop.tv_usec, ch->c_state);

			count++;
		}
	}

	if(count > 0){
		send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, "!capture-list",
				KATCP_FLAG_STRING,                    KATCP_OK,
				KATCP_FLAG_LAST  | KATCP_FLAG_ULONG, (unsigned long) count);
	} else {
		if(name){
			log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "no match for capture with name %s", name);
		} /* else rather odd error */
		extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
	}

	return KATCP_RESULT_OWN;
}

int capture_hstart_cmd(struct katcp_dispatch *d, int argc)
{
	struct state_holo *sh;
	struct capture_holo *ch;
	char *name, *ptr;
	struct timeval when, now, delta, prep, start, soonest;
	int result;

	sh = need_current_mode_katcp(d, POCO_HOLO_MODE);
	if(sh == NULL){
		return KATCP_RESULT_FAIL;
	}

	if(argc <= 1){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need parameters");
		extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
		return KATCP_RESULT_OWN;
	}

	name = arg_string_katcp(d, 1);

	ch = find_capture_holo(sh, name);
	if(ch == NULL){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "no match for name %s", name);
		extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
		return KATCP_RESULT_OWN;
	}

	delta.tv_sec = 0;
	delta.tv_usec = sh->h_lead;

	gettimeofday(&now, NULL);

	/* soonest prep time */
	add_time_katcp(&soonest, &now, &delta);

	if(ch->c_start.tv_sec != 0){
		log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "changing previously scheduled start considered poor form");
	}

	if(argc <= 2){

		add_time_katcp(&start, &soonest, &delta);

		ch->c_prep.tv_sec  = soonest.tv_sec;
		ch->c_prep.tv_usec = soonest.tv_usec;

		ch->c_start.tv_sec  = start.tv_sec;
		ch->c_start.tv_usec = start.tv_usec;

	} else {
		ptr = arg_string_katcp(d, 2);
		if(ptr == NULL){
			return KATCP_RESULT_FAIL;
		}
		if(time_from_string(&when, NULL, ptr) < 0){
			log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "%s not a well-formed time", ptr);
			extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
			return KATCP_RESULT_OWN;
		}

		if(sub_time_katcp(&prep, &when, &delta) < 0){
			log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "%s is unreasonable", ptr);
			extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
			return KATCP_RESULT_OWN;
		}

		if(cmp_time_katcp(&soonest, &prep) > 0){
			log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "start time %s already passed or too soon", ptr);
			extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
			return KATCP_RESULT_OWN;
		}

		ch->c_prep.tv_sec  = prep.tv_sec;
		ch->c_prep.tv_usec = prep.tv_usec;

		ch->c_start.tv_sec  = when.tv_sec;
		ch->c_start.tv_usec = when.tv_usec;
	}

	log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "preparing at %lu.%06lus and starting at %lu.%06lus", ch->c_prep.tv_sec, ch->c_prep.tv_usec, ch->c_start.tv_sec, ch->c_start.tv_usec);
#if 0
        sh->h_sync_time.tv_sec = ch->c_start.tv_sec;
        sh->h_sync_time.tv_usec = ch->c_start.tv_usec;
#endif

	result = (*(ch->c_schedule))(d, ch, CAPTURE_POKE_START);
	if(result < 0){
		return KATCP_RESULT_FAIL;
	}

	return KATCP_RESULT_OK;
}

int capture_hsync_cmd(struct katcp_dispatch *d, int argc)
{
	struct state_holo *sh;
	struct capture_holo *ch;
	char *name;

	sh = need_current_mode_katcp(d, POCO_HOLO_MODE);
	if(sh == NULL){
		return KATCP_RESULT_FAIL;
	}

	if(argc <= 1){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need parameters");
		extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
		return KATCP_RESULT_OWN;
	}

	name = arg_string_katcp(d, 1);
	if(name == NULL){
		return KATCP_RESULT_FAIL;
	}

	ch = find_capture_holo(sh, name);
	if(ch == NULL){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "no match for name %s", name);
		extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
		return KATCP_RESULT_OWN;
	}

#if 1
	/* somehow seems grotty */
	meta_udp_holo(d, ch, SYNC_STREAM_CONTROL_HOLO);
	tx_udp_holo(d, ch);
#endif

	return KATCP_RESULT_OK;
}

int capture_hstop_cmd(struct katcp_dispatch *d, int argc)
{
	struct state_holo *sh;
	struct capture_holo *ch;
	char *name, *ptr;
	struct timeval when;
	int result;

	sh = need_current_mode_katcp(d, POCO_HOLO_MODE);
	if(sh == NULL){
		return KATCP_RESULT_FAIL;
	}

	if(argc <= 1){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need parameters");
		extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
		return KATCP_RESULT_OWN;
	}

	name = arg_string_katcp(d, 1);

	ch = find_capture_holo(sh, name);
	if(ch == NULL){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "no match for name %s", name);
		extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
		return KATCP_RESULT_OWN;
	}

	if(ch->c_stop.tv_sec != 0){
		log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "changing stop time while busy considered bad form");
	}

	if(argc <= 2){
		gettimeofday(&when, NULL);
	} else {
		ptr = arg_string_katcp(d, 2);
		if(ptr == NULL){
			return KATCP_RESULT_FAIL;
		}
		if(time_from_string(&when, NULL, ptr) < 0){
			log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "%s not a well-formed time", ptr);
			extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
			return KATCP_RESULT_OWN;
		}
	}

#if 0
	/* if a start time is set and it is after the requested stop time, advance the stop time */
	if((ch->c_start.tv_sec != 0) && (cmp_time_katcp(&(ch->c_start), &when) >= 0)){
		/* move to the schedule function - it knows more */
	}
#endif

	ch->c_stop.tv_sec = when.tv_sec;
	ch->c_stop.tv_usec = when.tv_usec;

	log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "shutdown request for %lu.%06lus", ch->c_stop.tv_sec, ch->c_stop.tv_usec);

	result = (*(ch->c_schedule))(d, ch, CAPTURE_POKE_STOP);
	if(result < 0){
		return KATCP_RESULT_FAIL;
	}

	return KATCP_RESULT_OK;
}
/************HOLO DELAY***************/
int holo_delay_cmd(struct katcp_dispatch *d, int argc)
{
	char *label, *ptr;
	struct state_holo *sh;
	int pol, inp, result, wr;
        unsigned long adc_samples;
        uint32_t control, delay_select;
        long delay_time;
        int delay_cycles;
	char *delay_names[] = {DELAY_REGISTER_1, DELAY_REGISTER_2};
	uint32_t delay_control[] = {SET_DELAY_CONTROL_1, SET_DELAY_CONTROL_2};

	sh = need_current_mode_katcp(d, POCO_HOLO_MODE);
	if(sh == NULL){
		return KATCP_RESULT_FAIL;
	}

        adc_samples = sh->h_dsp_clock;

	if(argc < 2){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need channel time as parameter");
		extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
		return KATCP_RESULT_OWN;
	}

	label = arg_string_katcp(d, 1);/* inp pol */

	if(label == NULL){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire parameters");
		extra_response_katcp(d, KATCP_RESULT_FAIL, "internal");
		return KATCP_RESULT_OWN;
	}

	inp = extract_input_poco(label);
	pol = extract_polarisation_poco(label);
	log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "inp=%d,pol=%d",inp, pol);

        ptr = arg_string_katcp(d, 2); /* time */
        if(ptr == NULL){
          return KATCP_RESULT_FAIL;
        }
        result = shift_point_string(&delay_time, ptr, 11);
        /* delay time was given in ms, we multiplied it by 10^11 to remove fractions, now in tens of femto-seconds */
        log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "string %s converted to %ld", ptr, delay_time);
        if(result < 0){
          log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "delay value is too large or otherwise invalid");
          extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
          return KATCP_RESULT_OWN;
        } 
        if(result > 0){
          log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "discarding delay precision finer than 10^-11");
        }
        if(delay_time < 0){
          log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "negative delays not reasonable");
          return KATCP_RESULT_FAIL;
        }

        /* how many adc samples does the delay come to */
        delay_cycles = ((delay_time / 1000) * (adc_samples / 1000000)) / 100000;

	/* Check for inp and pol and determine which control register to write*/
	if(!inp && !pol){
		delay_select = 0;
	}
	else if(!inp && pol){
		delay_select = 1;
	}
	else if(inp && !pol){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "Not supported in holo mode,only 0x and 0y labels");
		return KATCP_RESULT_FAIL;
	}
	else{
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "Not supported in holo mode,only 0x and 0y labels");
		return KATCP_RESULT_FAIL;
	}

        /* Writing value to the determined delay register*/
        wr = write_name_pce(d, delay_names[delay_select], &delay_cycles, 0, 4);
        if(wr != 4){
          log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to zero bram control");
          return -1;
        }
        control = sh->h_source_state;                                                    
        log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "sanity check:Before flags applied ctrl0 = %x", control);

        /* Masking and setting required bits for source */
        control = CLEAR_DELAY_CONTROL(sh->h_source_state);
        log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "sanity check:After flags applied ctrl0 = %x", control);

        sh->h_source_state = control;                                                    

	/* Write positive edge to determined control register to release the value*/
	control = sh->h_source_state;
	wr = write_name_pce(d, CONTROL_HOLO_REGISTER, &control, 0, 4);
	if(wr != 4){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to zero bram control");
		return -1;
	}
	control = (sh->h_source_state | delay_control[delay_select]);
        sh->h_source_state = control;                                                    

	wr = write_name_pce(d, CONTROL_HOLO_REGISTER, &control, 0, 4);
	if(wr != 4){
		log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to reset bram control");
		return -1;
	}

	return KATCP_RESULT_OK;
}

/**************************************HOLO ATTENUATION*************************************************/
int holo_attenuation_cmd(struct katcp_dispatch *d, int argc)
{
  char *label;
  struct state_holo *sh;
  int pol, inp;
  uint8_t atten;
  uint8_t adc, base, in;

  sh = need_current_mode_katcp(d, POCO_HOLO_MODE);
  if(sh == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(argc < 2){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need channel value as parameter");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  label = arg_string_katcp(d, 1);/* inp pol */

  if(label == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire parameters");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "internal");
    return KATCP_RESULT_OWN;
  }

  inp = extract_input_poco(label);
  pol = extract_polarisation_poco(label);
  atten = arg_unsigned_long_katcp(d, 2);

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "inp=%d, pol=%d, att_val=%d",inp, pol, atten);

  /* Check for inp and pol and determine which control register to write*/
  if(!inp && !pol){
    adc = 0;
  }
  else if(!inp && pol){
    adc = 1;
  }
  else if(inp && !pol){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "katadc_set_atten: dodgy parameters");
    return KATCP_RESULT_FAIL;
  }
  else{
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "katadc_set_atten: dodgy parameters");
    return KATCP_RESULT_FAIL;
  }
  if(atten > MAX_ATTENUATION || atten < MIN_ATTENUATION){
    fprintf(stderr,"katadc_set_atten: attenuation value out of range %d-%d\n", MIN_ATTENUATION, MAX_ATTENUATION);
    return -1;
  }

  if(adc == 0) in = GPIOI_A;
  else in = GPIOQ_A;

  base = (uint8_t)(atten / ATTENUATION_STEP) << 1;
  base = ~base; 
  base = (base & 0x3F) | GPIO_LATCH | GPIO_SW_DISABLE;
  //  fprintf(stderr,"setting GPIO_REG_OUT to 0x%2x\n",base); 

 /*slot is 0 for holo system*/
  if (kat_adc_set_iic_reg(d, 0, in, GPIO_REG_OUT, base )){
    fprintf(stderr,"katadc_set_atten: error setting atten val for slot %d, adc %d\n", 0, adc);
    return KATCP_RESULT_FAIL;
  }

  return KATCP_RESULT_OK;
}
/*******************************************************************************************************************/
int setup_holo_poco(struct katcp_dispatch *d, char *image)
{
  struct state_holo *sh;
  struct katcp_acquire *a;
  int result;

  if(image == NULL){
    return -1;
  }

  sh = malloc(sizeof(struct state_holo));

  sh->h_image = NULL;
  sh->h_captures = NULL;
  sh->h_size = 0;
  sh->h_sync_time.tv_sec = 0;
  sh->h_sync_time.tv_usec = 0;
  sh->h_dump_count = 0;
  sh->h_lead = HOLO_DEFAULT_LEAD;/*To determine????*/
  sh->h_manual = HOLO_DEFAULT_SYNC_MODE;/*config.h,IMP:manual mode in holography*/
  sh->h_accumulation_length = HOLO_DEFAULT_ACCUMULATION;
  sh->h_dsp_clock = HOLO_DSP_CLOCK;
  sh->h_scale_factor = HOLO_DOWNSCALE_FACTOR;
  sh->h_fft_window = HOLO_FFT_WINDOW;
  sh->h_center_freq = HOLO_CENTER_FREQUENCY;
  /* clock sync */
  sh->h_ntp_sensor.n_fd = (-1);
  sh->h_time_stamp = 0;/*Time at which data start is zero for holo */

  /*calculate dumps*/
  sh->h_dump_count = (sh->h_dsp_clock >> sh->h_scale_factor);/*half a second dump fixed requirement:524000000/(2 ^ 18)*/

#ifdef DEBUG
		fprintf(stderr, "setup_dump_holo fn:acc_length = %d\n",sh->h_dump_count);
#endif

		sh->h_image = strdup(image);
		if(sh->h_image == NULL){
			destroy_holo_poco(d);
			return -1;
		}

		sh->inp_select = 0; /*default adc bram input stream is 0x*/

		/*TO DO: FILL enter_mode_holo*/
		if(store_full_mode_katcp(d, POCO_HOLO_MODE, POCO_HOLO_NAME, &enter_mode_holo, NULL, sh, &destroy_holo_poco) < 0){
			fprintf(stderr, "setup: unable to register holo mode\n");
			destroy_holo_poco(d);
			return -1;
		} 

                if(mode_version_katcp(d, POCO_HOLO_MODE, NULL, 0, 1)){
                  destroy_holo_poco(d);
                  return -1;
                }

		result = 0;

		result += register_bram_holo(d, "bram", HOLO_ACC_BLOCKS);/*FILL THIS & CHECK PARAMETERS*/
		if(result < 0){
			fprintf(stderr, "setup: unable to register bram capture instances\n");
			return -1;
		}

		if(init_ntp_poco(d, &(sh->h_ntp_sensor)) < 0){
			fprintf(stderr, "setup: unable to initialise ntp sensor\n");
			return -1;
		}
		a = setup_boolean_acquire_katcp(d, &acquire_ntp_poco, &(sh->h_ntp_sensor));
		if(a == NULL){
			fprintf(stderr, "setup: unable to allocate ntp acquisition\n");
			return -1;
		}
		if(register_direct_multi_boolean_sensor_katcp(d, POCO_HOLO_MODE, "holo.timing.sync", "clock good", "none", a) < 0){
			fprintf(stderr, "setup: unable to allocate ntp sensor\n");
			destroy_acquire_katcp(d, a);
			return -1;
		}

		/*capture commands*/
		result += register_mode_katcp(d, "?capture-list", "lists capture instances (?capture-list)", &capture_hlist_cmd, POCO_HOLO_MODE);
		result += register_mode_katcp(d, "?capture-destination", "sets the network destination (?capture-destination name ip port)", &capture_hdestination_cmd, POCO_HOLO_MODE);
		result += register_mode_katcp(d, "?capture-start", "start a capture (?capture-start name [time])", &capture_hstart_cmd, POCO_HOLO_MODE);
		result += register_mode_katcp(d, "?capture-stop", "stop a capture (?capture-stop name [time])", &capture_hstop_cmd, POCO_HOLO_MODE);
		result += register_mode_katcp(d, "?capture-sync", "emit header for a capture stream (?capture-sync name)", &capture_hsync_cmd, POCO_HOLO_MODE);
		result += register_mode_katcp(d, "?holo-delay", "sets the coarse delay (?holo-delay antenna[polarisation] time)", &holo_delay_cmd, POCO_HOLO_MODE);
		/*parameters*/
		result += register_mode_katcp(d, "?holo-snap-shot", "grabs a snap shot(?holo-snap-shot name [input[polarisation]])", &holo_snap_shot_cmd, POCO_HOLO_MODE);
		result += register_mode_katcp(d, "?holo-attenuation", "katadc attenuator adjustments (?holo-attenuation [input[polarisation] value])", &holo_attenuation_cmd, POCO_HOLO_MODE);
#ifdef KATADC
		result += register_mode_katcp(d, "?katadc-config", "katadc initialistation and configuration (?katadc-config)", &katadc_config_cmd, POCO_HOLO_MODE);
#endif
		if(result < 0){
			fprintf(stderr, "setup: unable to register holo mode commands\n");
			return -1;
		}

		return 0;
	}
