#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <math.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "options.h"
#include "collect.h"
#include "gnuplot_i.h"

#define PACKET_LEN 9728

struct collect_state{
	int c_verbose;
	int c_nfd;
	int c_ffd;
	FILE *c_meta;
	FILE *c_text;

	unsigned int c_have;

	struct timeval c_start;
	struct timeval c_stop;

	/*EXTRAS*/
	unsigned int c_valbits;
	unsigned int c_itemvals;
	unsigned int c_byteorder;
	unsigned int c_datatype;

	unsigned int c_size;
	unsigned int c_new;
	unsigned int c_extra;
	unsigned int c_direction;
	unsigned int c_mask;

	unsigned long c_rxp;
	unsigned long c_rxopts;
	unsigned long c_rxdata;
	unsigned long c_rxtotal;
	unsigned long c_lostpackets;
	unsigned long c_tracker;
	unsigned int  lost_flag;

	unsigned long c_errors;

	unsigned char c_buffer[PACKET_LEN];

	/*Plot Extras*/
	unsigned int c_baseline;
	unsigned int c_cnt;
	unsigned int c_counter;
	unsigned int c_offset;
};

int collect_file(char *name)
{
	int flags;

	flags = O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR;
#ifdef O_LARGEFILE
	flags |= O_LARGEFILE;
#endif

	return open(name, flags, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
}

int collect_socket(int port)
{
	int fd, size, result;
	struct sockaddr_in *addr, data;
	unsigned int len;

	addr = &data;

	fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0){
		fprintf(stderr, "collect: unable to create socket: %s\n", strerror(errno));
		return -1;
	}

#define RESERVE_SOCKET (1024 * 1500)

	len = sizeof(int);
	result = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, &len);
	if((result < 0) || (size < RESERVE_SOCKET)){
		size = RESERVE_SOCKET;
		result = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
		if(result < 0){
#ifdef SO_RCVBUFFORCE
			result = setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE, &size, sizeof(size));
			if(result < 0){
#endif
				fprintf(stderr, "collect: unable to set receive buffer size to %d: %s\n", size, strerror(errno));
#ifdef SO_RCVBUFFORCE
			}
#endif
		}
	}

	len = sizeof(struct sockaddr_in);

	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = htonl(INADDR_ANY);
	addr->sin_port = htons(port);

	if(bind(fd, (struct sockaddr *) addr, len) < 0){
		fprintf(stderr, "collect: bind %s:%d failed: %s\n", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port), strerror(errno));
		close(fd);
		return -1;
	}

	return fd;
}

void destroy_collect(struct collect_state *cs)
{
	if(cs == NULL){
		return;
	}

	if(cs->c_nfd){
		close(cs->c_nfd);
		cs->c_nfd = (-1);
	}
	if(cs->c_ffd){
		close(cs->c_ffd);
		cs->c_ffd = (-1);
	}

	free(cs);
}

struct collect_state *create_collect(char *binary, char *text, int port, int verbose, int baseline)
{
	struct collect_state *cs;

	cs = malloc(sizeof(struct collect_state));
	if(cs == NULL){
		return NULL;
	}

	cs->c_meta = stderr;
	cs->c_text = NULL;

	cs->c_nfd = (-1);
	cs->c_ffd = (-1);

	cs->c_rxp = 0;
	cs->c_rxopts = 0;
	cs->c_rxdata = 0;
	cs->c_rxtotal = 0;
	cs->c_errors = 0;
	cs->c_tracker = 0;
	cs->c_lostpackets = 0;

	cs->c_new = 1;
	cs->c_size = 4;
	cs->c_extra = 0;
	cs->c_direction = 1;
	cs->c_mask = 0xFFFFFFFF;

	cs->c_cnt = 0;
	cs->c_counter = 0;
	cs->c_baseline = baseline;


	/* at this point, cs structure valid, can safely be destroyed */

	cs->c_verbose = verbose;
	if(text){
		if(strcmp(text, "-")){
			cs->c_text = fopen(text, "w");
			if(cs->c_text == NULL){
				fprintf(stderr, "create: unable to open %s for writing\n", text);
				destroy_collect(cs);
				return NULL;
			} else {
#ifdef DEBUG
				fprintf(stderr, "create: opened %s\n", text);
#endif
			}
		} else {
			cs->c_text = stdout;
		}
	} else {
		cs->c_text = NULL;
	}

	cs->c_nfd = collect_socket(port);
	if(cs->c_nfd < 0){
		destroy_collect(cs);
		return NULL;
	}

	cs->c_ffd = collect_file(binary);
	if(cs->c_ffd < 0){
		destroy_collect(cs);
		return NULL;
	}

	return cs;
}

int option_process(struct collect_state *cs, struct option_poco *op)
{
	uint16_t label, msw;
	uint32_t lsw;
	unsigned long long big;
	int result;
	time_t t, now;

	label = ntohs(op->o_label);
	msw = ntohs(op->o_msw);
	lsw = ntohl(op->o_lsw);

	big = (msw * 0x1000000ULL) + lsw;

	result = 0;

	if(cs->c_meta){
		fprintf(cs->c_meta, "option 0x%04x: 0x%04x%08x ", label, msw, lsw);
	}

	switch(label){
		case INSTRUMENT_TYPE_OPTION_POCO :
			if(cs->c_meta){
				cs->c_valbits  = GET_VALBITS_INSTRUMENT_TYPE_POCO(lsw);
				cs->c_itemvals = GET_VALITMS_INSTRUMENT_TYPE_POCO(lsw);

				if(lsw & BIGENDIAN_INSTRUMENT_TYPE_POCO){
					cs->c_byteorder = 1;
				}
				else{
					cs->c_byteorder = 0;
				}
				if(lsw & UINT_TYPE_INSTRUMENT_TYPE_POCO){
					cs->c_datatype = 0;
				}
				else if(lsw & SINT_TYPE_INSTRUMENT_TYPE_POCO){
					cs->c_datatype = 1;
				}
				else{
					cs->c_datatype = 2;
				}
			}
			fprintf(cs->c_meta, "instrument %d", msw);
			fprintf(cs->c_meta, " has %d", cs->c_itemvals);
			fprintf(cs->c_meta, " %s %s-endian values",
					(lsw & COMPLEX_INSTRUMENT_TYPE_POCO) ? "complex" : "real",
					cs->c_byteorder ? "big" : "little");
			fprintf(cs->c_meta, " each %d bits", cs->c_valbits);

			break;
		case TIMESTAMP_OPTION_POCO :
			if(cs->c_meta) fprintf(cs->c_meta, "timestamp %llu", big);
			break;
		case PAYLOAD_LENGTH_OPTION_POCO :
			if(cs->c_meta) fprintf(cs->c_meta, "payload length %llu", big);
			break;
		case PAYLOAD_OFFSET_OPTION_POCO :
			if(cs->c_meta){
				fprintf(cs->c_meta, "payload offset %llu", big);
				cs->c_offset = big;
			}
			break;
		case ADC_SAMPLE_RATE_OPTION_POCO :
			if(cs->c_meta) fprintf(cs->c_meta, "adc rate %lluHz", big);
			break;
		case FREQUENCY_CHANNELS_OPTION_POCO :
			if(cs->c_meta) fprintf(cs->c_meta, "%u frequency channels", lsw);
			break;
		case ANTENNAS_OPTION_POCO :
			if(cs->c_meta) fprintf(cs->c_meta, "%u antennas", lsw);
			break;
		case BASELINES_OPTION_POCO :
			if(cs->c_meta) fprintf(cs->c_meta, "%u baselines", lsw);
			break;
		case STREAM_CONTROL_OPTION_POCO :
			if(cs->c_meta) fprintf(cs->c_meta, "stream control: ");
			switch(lsw){
				case START_STREAM_CONTROL_POCO  :
					if(cs->c_meta) fprintf(cs->c_meta, "start");
					break;
				case SYNC_STREAM_CONTROL_POCO   :
					if(cs->c_meta) fprintf(cs->c_meta, "sync");
					break;
				case CHANGE_STREAM_CONTROL_POCO :
					if(cs->c_meta) fprintf(cs->c_meta, "change");
					break;
				case STOP_STREAM_CONTROL_POCO   :
					result = 1;
					if(cs->c_meta) fprintf(cs->c_meta, "stop");
					break;
				default :
					if(cs->c_meta) fprintf(cs->c_meta, "UNKNOWN");
					break;
			}
			break;
		case META_COUNTER_OPTION_POCO :
			if(cs->c_meta) fprintf(cs->c_meta, "meta packet %u", lsw);
			break;
		case SYNC_TIME_OPTION_POCO :
			t = big;
			time(&now);
			if(cs->c_meta){
				fprintf(cs->c_meta, "last sync at %lu (%lu seconds ago)", t, (now - t));
			}
			break;
		case BANDWIDTH_HZ_OPTION_POCO :
			if(cs->c_meta){
				fprintf(cs->c_meta, "%lluHz bandwidth", big);
			}
			break;
		case ACCUMULATIONS_OPTION_POCO :
			if(cs->c_meta) fprintf(cs->c_meta, "%llu accumulations", big);
			break;
		case TIMESTAMP_SCALE_OPTION_POCO :
			if(cs->c_meta) fprintf(cs->c_meta, "%llu timestamp scale", big);
			break;

	}

	if(cs->c_meta){
		fprintf(cs->c_meta, "\n");
	}

	return result;
}

int collect_precalculate(struct collect_state *cs)
{
	int k;
	/*Determine size*/
	switch(cs->c_valbits){
		case 8:
			cs->c_size = 1;
			break;
		case 16:
			cs->c_size = 2;
			break;
		case 32:
			cs->c_size= 4;
			break;
		default :
			/* TODO */
			break;
	}
	/*Check for Endianness*/
	if(cs->c_byteorder){
		cs->c_extra = 0;
		cs->c_direction = 1;
	}
	else{
		cs->c_extra = cs->c_size - 1;
		cs->c_direction = -1;
	}

	for(k = 0; k < cs->c_size; k++){
		cs->c_mask = cs->c_mask << 8;
	}

	return 0;

}

int init_baseline_display(gnuplot_ctrl *h[4])
{
	int count;

	for(count = 0; count < 4; count++){
		h[count] = gnuplot_init();
	}
	return 0;
}

int clean(gnuplot_ctrl *bsln_disp[4])
{
	int count;

	for( count = 0; count < 4; count++) {
		if(bsln_disp[count] != NULL){
			gnuplot_close(bsln_disp[count]);
		}
	}
	return 0;
}

int get_product(int baseline, int *raw_real, int *raw_imag, double *power, double *phase)
{
	int real[3], imag[3], count;

	if( raw_real == NULL || raw_imag == NULL || real == NULL || imag == NULL ){
		fprintf(stderr,"get_product: dodgy arguments\n");
		return -1;
	}
	switch(baseline){
		case BASELINE01: //01
			real[0] = raw_real[CROSS00];
			real[1] = raw_real[CROSS11];
			real[2] = raw_real[CROSS01];
			imag[0] = raw_imag[CROSS00];
			imag[1] = raw_imag[CROSS11];
			imag[2] = raw_imag[CROSS01];
			break;
		case BASELINE02: //02
			real[0] = raw_real[CROSS00];
			real[1] = raw_real[CROSS22];
			real[2] = raw_real[CROSS02];
			imag[0] = raw_imag[CROSS00];
			imag[1] = raw_imag[CROSS22];
			imag[2] = raw_imag[CROSS02];
			break;
		case BASELINE03: //03
			real[0] = raw_real[CROSS00];
			real[1] = raw_real[CROSS33];
			real[2] = raw_real[CROSS03];
			imag[0] = raw_imag[CROSS00];
			imag[1] = raw_imag[CROSS33];
			imag[2] = raw_imag[CROSS03];
			break;
		case BASELINE12: //12
			real[0] = raw_real[CROSS11];
			real[1] = raw_real[CROSS22];
			real[2] = raw_real[CROSS12];
			imag[0] = raw_imag[CROSS11];
			imag[1] = raw_imag[CROSS22];
			imag[2] = raw_imag[CROSS12];
			break;
		case BASELINE13: //13
			real[0] = raw_real[CROSS11];
			real[1] = raw_real[CROSS33];
			real[2] = raw_real[CROSS13];
			imag[0] = raw_imag[CROSS11];
			imag[1] = raw_imag[CROSS33];
			imag[2] = raw_imag[CROSS13];
			break;
		case BASELINE23: //23
			real[0] = raw_real[CROSS22];
			real[1] = raw_real[CROSS33];
			real[2] = raw_real[CROSS23];
			imag[0] = raw_imag[CROSS22];
			imag[1] = raw_imag[CROSS33];
			imag[2] = raw_imag[CROSS23];
			break;

		default:
			fprintf( stderr,"get_product: unknown baseline %d\n", baseline);
	}

	for(count = 0; count < 3; count++){
		power[count] = 10*log( sqrt( (double)real[count]*(double)real[count] + (double)imag[count]*(double)imag[count] ));
		phase[count] = atan2( (double)imag[count], (double)real[count] ) * (360/(2*M_PI));
	}

	return 0;
}

int plot_doubles(gnuplot_ctrl *h, double *xdata, double *ydata, int len, char *plot_name)
{
	if( h == NULL || ydata == NULL || xdata == NULL|| len <= 0  || plot_name == NULL){
		fprintf(stderr, "plot_data: dodgy arguments\n");
		return -1;
	}

	gnuplot_setstyle(h, "lines");
	gnuplot_resetplot(h);
	gnuplot_plot_xy(h, xdata, ydata, len, plot_name);

	return 0;
}

int plot_baseline(gnuplot_ctrl *h[4], double *power, double *phase, int len)
{
	int count;
	double *all;
	double *x_data, *self_power, *other_self_power, *cross_power, *cross_phase;

	if( power == NULL || phase == NULL || h[0] == NULL || h[1] == NULL || h[2] == NULL){
		fprintf(stderr,"plot_baseline: dodgy arguments\n");
		return -1;
	}

	all = malloc(sizeof(double) * len * 5);
	if( all == NULL){
		fprintf(stderr,"Error allocating for all\n");
		return -1;
	}
	x_data = all;
	self_power = all+len;
	other_self_power = all+len*2;
	cross_power = all+len*3;
	cross_phase = all+len*4;

	for(count = 0; count < len; count++){
		x_data[count] = count;
		self_power[count] = power[count * 3];
		other_self_power[count] = power[count*3+1];
		cross_power[count] = power[count*3+2];
		cross_phase[count] = phase[count*3+2];
	}
#if DEBUG
	for(count = 0; count < 60; count++){
		fprintf(stderr, "x_data[%d]=%f,self_power=%f,other_self_power=%f,cross_power=%f,cross_phase=%f\n",count,x_data[count],self_power[count],other_self_power[count],cross_power[count],cross_phase[count]);
	}
#endif

	if( plot_doubles(h[0], x_data, self_power, len, "Self Power") < 0){
		fprintf(stderr,"Error plotting power in self\n");
		free(all);
		return -1;
	}
	if( plot_doubles(h[1], x_data, other_self_power, len, "Other Power") < 0){
		fprintf(stderr,"Error plotting power in other_self\n");
		free(all);
		return -1;
	}

	if( plot_doubles(h[2], x_data, cross_power, len, "Cross Power") < 0){
		fprintf(stderr,"Error plotting power in other_self\n");
		free(all);
		return -1;
	}

	if( plot_doubles(h[3], x_data, cross_phase, len, "Phase") < 0){
		fprintf(stderr,"Error plotting phase in cross\n");
		free(all);
		return -1;
	}

	free(all);
	return 0;
}

int capture_baseline_data(int baseline, int *real_raw, int *imag_raw, double *power, double *phase, int num_chan)
{
	int count;

	if( power == NULL || phase == NULL || real_raw == NULL || imag_raw == NULL){
		fprintf(stderr,"capture_baseline_data: dodgy parameters\n");
		return -1;
	}

	/*loop through channels getting all products for specified baseline*/
	for( count = 0; count < num_chan; count++){
		if( get_product(baseline, real_raw + (count*NUM_BASELINE_PRODUCTS), imag_raw + (count*NUM_BASELINE_PRODUCTS), power+(count*3), phase+(count*3)) < 0 ){
			fprintf(stderr,"capture_baseline_data: error getting products for baseline %d\n", baseline);
			free(real_raw);
			free(imag_raw);
		}
	}

	return 0;
}

int display_baseline(int baseline, gnuplot_ctrl *h[4], int *real_raw, int *imag_raw){

	double power[FFT_SIZE*3];
	double phase[FFT_SIZE*3];

	/*Populates the power and phase array*/
	if( capture_baseline_data(baseline, real_raw, imag_raw, power, phase, FFT_SIZE) < 0 ){
		fprintf(stderr,"display_baseline: error capturing data for baseline %d\n",baseline);
		return -1;
	}

	/*Plots power and phase*/
	if( plot_baseline( h, power, phase, FFT_SIZE) < 0){
		fprintf(stderr,"display_baseline: error displaying baseline %d\n",baseline);
		return -1;
	}
	return 0;
}

int collect_process(struct collect_state *cs, gnuplot_ctrl *bsln_disp[4], int *real_raw, int *imag_raw)
{
	struct header_poco *hp;
	struct option_poco *op;
	unsigned int options, base, payload, i;
	int wr, or, result;
	int j, k;
	unsigned int value, compare, flag;

	result = 0;
	compare = 0;

	if(cs->c_have < 8){
		fprintf(stderr, "process: short packet (len=%u)\n", cs->c_have);
		return -1;
	}

	hp = (struct header_poco *) cs->c_buffer;

	if(hp->h_magic != htons(MAGIC_HEADER_POCO)){
		fprintf(stderr, "process: bad header magic 0x%04x\n", hp->h_magic);
		cs->c_errors++;
		return -1;
	}


	if(hp->h_version != htons(VERSION_HEADER_POCO)){
		fprintf(stderr, "process: odd version %d\n", ntohs(hp->h_version));
		cs->c_errors++;
		return -1;
	}

	options = ntohs(hp->h_options);
	base = (options + 1) * 8;

	if(base > cs->c_have){
		fprintf(stderr, "process: options larger than packet itself\n");
		cs->c_errors++;
		return -1;
	}

	payload = cs->c_have - base;

	if(cs->c_meta){
		fprintf(cs->c_meta, "frame %lu: options %u, length %u, payload %u\n", cs->c_rxp, options, cs->c_have, payload);
	}

	for(i = 0; i < options; i++){
		op = (struct option_poco *)(cs->c_buffer + ((i + 1) * 8));
		or = option_process(cs, op);
		if(or){
			result = or;
		}
	}

	cs->c_rxopts += options;

	/* at this point we have the number of data bytes in payload, and the start of data at at cp->c_bufer + base */

	if(payload){
		wr = write(cs->c_ffd, cs->c_buffer + base, payload);
		if(wr < payload){
			fprintf(stderr, "process: unable to write payload of %d\n", payload);
			return -1;
		}

		cs->c_rxdata += payload;

		/*This is lame but i think it works*/
		if(!cs->c_offset){
			cs->c_tracker = 0;
			cs->c_tracker += payload;
		}
		else{
			cs->c_tracker += payload;
		}

		/*Check for lost packets*/
		if((cs->c_tracker-payload) != cs->c_offset){
			cs->c_lostpackets++;
			cs->lost_flag = 1;
		}
		fprintf(stderr, "%lu %lu %d->%d\n", cs->c_rxdata, (cs->c_tracker-payload) , cs->c_offset, cs->lost_flag);


		if(cs->c_text != NULL){
			/* TODO: maybe write data out in human readable form to cs->c_text ? */
			/* fprintf(cs->c_text, "..."); */

			if(cs->c_new){
				collect_precalculate(cs);
				cs->c_new = 0;
			}

#ifdef DEBUG
			fprintf(stderr, "base=%d, payload=%d, size=%d\n", base, payload, cs->c_size);
#endif

			for(j = base; j < (base + payload); j += cs->c_size){
				value = 0;
				compare = cs->c_buffer[base + cs->c_extra];
				if(compare & 0x80){
					flag = 1;
				}
				for(k = 0; k < cs->c_size; k++){
					value *= 256;
					value += cs->c_buffer[j + cs->c_extra + (cs->c_direction * k)];
				}
				/*Check for data type:unsigned,signed or float*/
				if(!cs->c_datatype){
					fprintf(cs->c_text,"%u%c", value, (cs->c_cnt == 0 || cs->c_cnt % 23 != 0 ) ? ',' : '\n');
				}
				else if(cs->c_datatype == 1){
					if(flag){
						value = (value | cs->c_mask);
					}
					if((cs->c_cnt % 2) == 0){
						real_raw[(cs->c_counter * 12) + (cs->c_cnt / 2)] = value;
					}
					else{
						imag_raw[(cs->c_counter * 12) + (cs->c_cnt / 2)] = value;
					}
					fprintf(cs->c_text,"%d%c", value, (cs->c_cnt == 0 || cs->c_cnt % 23 != 0) ? ',' : '\n');
				}
				else{
					/*TODO:Floating point case*/
				}
				flag = 0;

				cs->c_cnt++;

				if(cs->c_cnt == 24){
					cs->c_cnt = 0;
					cs->c_counter++;
				}

				if(cs->c_counter == 512 ){
					if(!cs->lost_flag){
						/*Display real time power and phase plots*/
						if(display_baseline(cs->c_baseline, bsln_disp, real_raw, imag_raw) < 0){
							fprintf(stderr,"main: error getting and displaying baseline %d\n", cs->c_baseline);
							return -1;
						}
					}
					cs->c_counter = 0;
					cs->lost_flag = 0;

				}
			}
		}
	}

	return result;
}

int collect_loop(struct collect_state *cs)
{
	int rr, result, i;
	int *real_raw, *imag_raw;

	gnuplot_ctrl *bsln_disp[4] = {NULL, NULL, NULL, NULL};

	real_raw = malloc(sizeof(int) * FFT_SIZE * NUM_BASELINE_PRODUCTS);
	if( real_raw == NULL){
		fprintf(stderr, "capture_baseline_data: error mallocing raw_real\n");
		return -1;
	}

	imag_raw = malloc(sizeof(int) * FFT_SIZE * NUM_BASELINE_PRODUCTS);
	if( imag_raw == NULL){
		fprintf(stderr, "capture_baseline_data: error mallocing imag_real\n");
		free(real_raw);
		return -1;
	}

	if( init_baseline_display(bsln_disp) < 0){
		fprintf(stderr,"main: error initialising baseline displays\n");
		return -1;
	}
	for(;;){
		rr = recv(cs->c_nfd, cs->c_buffer, PACKET_LEN, 0);
#ifdef DEBUG
		fprintf(stderr, "loop: received with code %d\n", rr);
#endif
		if(rr < 0){
			switch(errno){
				case EAGAIN :
				case EINTR :
					continue; /* WARNING */
				default :
					fprintf(stderr, "collect: receive failed %s\n", strerror(errno));
					gettimeofday(&(cs->c_stop), NULL);
					return -1;
			}
		}

		if(cs->c_rxp == 0){
			gettimeofday(&(cs->c_start), NULL);
		}

		cs->c_have = rr;

		cs->c_rxp++;
		cs->c_rxtotal += rr;

		if(cs->c_verbose > 1){
			fprintf(stderr, "read: got packet of %d\n", rr);
			if(cs->c_verbose > 2){
				fprintf(stderr, "read: data");
				for(i = 0; i < cs->c_have; i++){
					fprintf(stderr, " %02x", cs->c_buffer[i]);
				}
				fprintf(stderr, "\n");
			}
		}

		result = collect_process(cs, bsln_disp, real_raw, imag_raw);

		if(result){
#ifdef DEBUG
			fprintf(stderr, "CLEANUP......................CLEANUP\n");
#endif
			clean(bsln_disp);
			free(real_raw);
			free(imag_raw);
			gettimeofday(&(cs->c_stop), NULL);
			return (result > 0) ? 0 : -1;
		}
	}
}

int collect_stats(struct collect_state *cs)
{
	double from, to, delta, rate;

	if(cs->c_meta){
		fprintf(cs->c_meta, "frames received:     %lu\n", cs->c_rxp);
		fprintf(cs->c_meta, "malformed frames:    %lu\n", cs->c_errors);
		fprintf(cs->c_meta, "Lost packet count:    %lu\n", cs->c_lostpackets);
		fprintf(cs->c_meta, "total option count:  %lu\n", cs->c_rxopts);
		fprintf(cs->c_meta, "total data bytes:    %lu\n", cs->c_rxdata);
		fprintf(cs->c_meta, "overall bytes seen:  %lu\n", cs->c_rxtotal);

		fprintf(cs->c_meta, "start:    %lu.%06lus\n", cs->c_start.tv_sec, cs->c_start.tv_usec);
		fprintf(cs->c_meta, "stop:     %lu.%06lus\n", cs->c_stop.tv_sec, cs->c_stop.tv_usec);

		fflush(cs->c_meta);

		from = cs->c_start.tv_sec * 1000000 + cs->c_start.tv_usec;
		to   = cs->c_stop.tv_sec * 1000000 + cs->c_stop.tv_usec;

		delta = (to - from) / 1000.0;
		rate = (cs->c_rxtotal) / delta;
		fprintf(cs->c_meta, "rate: %fkb/s\n", rate);
	}

	return 0;
}

int main(int argc, char **argv)
{
	int i, j, c;
	struct collect_state *cs;
	int verbose;
	int baseline;

	int port = 0;
	char *binary = NULL;
	char *text = NULL;
	verbose = 1;
	baseline = BASELINE01;

	i = 1;
	j = 1;
	while (i < argc) {
		if (argv[i][0] == '-') {
			c = argv[i][j];
			switch (c) {
				case '\0':
					j = 1;
					i++;
					break;
				case '-':
					j++;
					break;
				case 'b':
					i++;
					if( strncmp(argv[i],"01",2) == 0 ) {
						baseline = BASELINE01;
					}
					else if( strncmp(argv[i],"02",2) == 0 ) {
						baseline = BASELINE02;
					}
					else if( strncmp(argv[i],"03",2) == 0) {
						baseline = BASELINE03;
					}
					else if( strncmp(argv[i],"12",2) == 0) {
						baseline = BASELINE12;
					}
					else if( strncmp(argv[i],"13",2) == 0) {
						baseline = BASELINE13;
					}
					else if( strncmp(argv[i],"23",2) == 0) {
						baseline = BASELINE23;
					} else {
						fprintf(stderr,"parse_args: unrecognised baseline %s\n",argv[i]);
					}
					i++;
					j = 1;
					break;
				case 'q':
					verbose = 0;
					i++;
					break;
				case 'v':
					verbose++;
					i++;
					break;
				case 'h' :
					fprintf(stderr, "usage: %s [-b nm] [-o binary-output] [-d decoded-output] [-p receive-port]\n", argv[0]);
					return 0;
					break;
				case 'd' :
					j++;
					if (argv[i][j] == '\0') {
						j = 0;
						i++;
					}
					if(i >= argc){
						text = NULL;
					} else {
						text = argv[i] + j;
					}
					i++;
					j = 1;
					break;
				case 'o' :
				case 'p' :
					j++;
					if (argv[i][j] == '\0') {
						j = 0;
						i++;
					}
					if (i >= argc) {
						fprintf(stderr, "%s: option -%c requires a parameter\n", argv[0], c);
						return EX_USAGE;
					}
					switch(c){
						case 'o' :
							binary = argv[i] + j;
							break;
						case 'p' :
							port = atoi(argv[i] + j);
							break;
					}
					i++;
					j = 1;
					break;
				default:
					fprintf(stderr, "%s: unknown option -%c\n", argv[0], c);
					return 1;
					break;
			}
		} else {
			fprintf(stderr, "%s: extra argument %s\n", argv[0], argv[i]);
			return EX_USAGE;
		}
	}

	if((port <= 0) || (port > 0xffff)){
		fprintf(stderr, "%s: invalid port %d\n", argv[0], port);
		return EX_USAGE;
	}

	if(binary == NULL){
		fprintf(stderr, "%s: need an output filename\n", argv[0]);
		return EX_USAGE;
	}

	cs = create_collect(binary, text, port, verbose, baseline);
	if(cs == NULL){
		fprintf(stderr, "%s: unable to set up\n", argv[0]);
		return EX_USAGE;
	}

	collect_loop(cs);

	collect_stats(cs);

	destroy_collect(cs);

	return 0;
}
