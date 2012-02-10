#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFFER 1024
#define MAX_MSGS 20

char *device;
int baud_rate;

int read_char_commands(int argc, char *argv[], char msgs[MAX_MSGS][BUFFER]){
  int i, cmd, arg;
  char *temp;
  arg = 0;
  cmd = 0;
  for(i=1; argv[i] != NULL; i+=arg){
    if(argv[i][0] == '-'){
      arg = 1;
      switch(argv[i][1]){
        case 'd':
          temp = strtok(argv[i+1], ":");
          //strcpy(msgs[0], strtok(argv[i+1], ":"));
          if(temp == NULL){
            printf("error: strtok(argv[],\":\") failed to read device name\n");
            return -1;
          }
          device = temp;
          temp = strtok(NULL, ":");
          if(temp == NULL){
            printf("error: strtok(NULL, \":\") failed to read baud rate\n");
            return -1;
          }
          baud_rate = atoi(temp);
          
          printf("setting device name to %s and baud rate to %d\n",
              device, baud_rate);
          break;
        case 'r':
          strcpy(msgs[cmd++], "*RST?");
          break;
        case 'c':strcpy(msgs[cmd], "*CLS?");
                 break;
        case 'p':
                 if(argv[i][2] == '?' || !argv[i+1] || argv[i+1][0] == '-'){
                   strcpy(msgs[cmd++], "POWER?");
                 }else{
                   sprintf(msgs[cmd++], "POWER %s", argv[i+1]);
                   arg = 2;
                 }
                 break;
        case 'f':
                 if(argv[i][2] == '?' || !argv[i+1] || argv[i+1][0] == '-'){
                   strcpy(msgs[cmd++], "FREQUENCY?");
                 }else {
                   arg = 2;
                   sprintf(msgs[cmd++], "FREQUENCY %s", argv[i+1]);
                 }
                 break;
        case 'o': if(argv[i][2] == '?' || !argv[i+1] || argv[i+1][0] == '-'){
                    strcpy(msgs[cmd++], "OUTPUT?");
                  }else {
                    sprintf(msgs[cmd++], "OUTPUT %s", argv[i+1]);
                    arg = 2;
                  }
                  break;
        case 'e':
                  strcpy(msgs[cmd++], "*ESR?");
                  strcpy(msgs[cmd++], "SYST:ERR?");
                  break;
        case 's': 
                  strcpy(msgs[cmd++], "FREQUENCY?");
                  strcpy(msgs[cmd++], "POWER?");
                  strcpy(msgs[cmd++], "OUTPUT?");
                  strcpy(msgs[cmd++], "*ESR?");
                  strcpy(msgs[cmd++], "SYST:ERR?");
                  break;
        default:
                  strcpy(msgs[cmd++], "*IDN?");
                  break;
      }

    }
  }
  return cmd;
}

int read_full_commands(int argc, char *argv[], char msgs[MAX_MSGS][BUFFER]){
  int i, arg = 0, cmd = 0;
  char *temp;

  for(i=1; argv[i] != NULL && cmd<MAX_MSGS; i+=arg){
    arg = 1;
    if(argv[i][0] == '*'){//common command - doesn't have arguments
      strcpy(msgs[cmd++], argv[i]);
    }else if(strpbrk(argv[i], "?") != NULL){
      strcpy(msgs[cmd++], argv[i]);//query message - doesn't have arguments
    }else if(strpbrk(argv[i], " ") != NULL){
      strcpy(msgs[cmd++], argv[i]);//command and arguments are packed as a single entity
    }else if(strcmp(argv[i], "DEVICE:BAUD_RATE") == 0){
      if(argv[i+1] == NULL){
        printf("DEVICE:BAUD_RATE command should be followed by arguments\n");
        return -1;
      }

      temp = strtok(argv[i+1], ":");
      //strcpy(msgs[0], strtok(argv[i+1], ":"));
      if(temp == NULL){
        printf("error: strtok(argv[],\":\") failed to read device name\n");
        return -1;
      }
      device = temp;
      // strcpy(device, temp);
      // strcpy(msgs[1], strtok(NULL, ":"));
      temp = strtok(NULL, ":");
      if(temp == NULL){
        printf("error: strtok(NULL, \":\") failed to read baud rate\n");
        return -1;
      }
      baud_rate = atoi(temp);
      printf("setting device name to %s and baud rate to %d\n",
          device, baud_rate);
      cmd--;
      arg = 2;
   
   }else if(argv[i+1] != NULL){
      arg = 2;
      sprintf(msgs[cmd++], "%s %s", argv[i], argv[i+1]);
    }else{
     printf("Failed to load the command, enter -h, for help\n");
     return -1;
    }
  }

  return cmd;
}

struct speed_item{
  int x_speed;
  int x_code;
};

static struct speed_item speed_table[] = {
  { 2400,    B2400 },
  { 9600,    B9600 },  
  { 19200,   B19200 }, 
  { 19200,   B19200 }, 
  { 38400,   B38400 }, 
  { 57600,   B57600 },  
  { 115200,  B115200 }, 
  { 0,       B0 } 
};

static int start_serial(char *device, int speed)
{
  struct termios tio;
  int i, code;
  int fd;

  fd = open(device, O_RDWR | O_NONBLOCK | O_NOCTTY);
  if(fd < 0){
    fprintf(stderr, "error: unable to open serial %s: %s\n", device, strerror(errno));
    return -1;
  }

  if(!isatty(fd)){
    fprintf(stderr, "error: open device (%s) is not a serial device, isatty()\n", device);
    return -1;
  }

  if(tcgetattr(fd, &tio)){
    fprintf(stderr, "error: unable to get attributes for serial %s: %s\n", device, strerror(errno));
    close(fd);
    return -1;
  }

#if 0
  cfmakeraw(&tio);
#endif

  tio.c_cc[VMIN] = 1;
  tio.c_cc[VTIME] = 0;

  tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IXOFF | IXON);
  tio.c_iflag |= (IGNCR | ICRNL);

#if 0
  tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR);
  tio.c_iflag |= (IGNCR | ICRNL | IXOFF | IXON);
#endif

  tio.c_oflag &= ~OPOST;

  tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

#ifdef CRTSCTS
  tio.c_cflag &= ~(CRTSCTS);
#endif
  tio.c_cflag &= ~(CSIZE | PARENB);
  tio.c_cflag |=  (CS8 | CLOCAL);

  if(speed){
    for(i = 0; (speed_table[i].x_speed > 0) && (speed_table[i].x_speed != speed); i++);

    if(speed_table[i].x_speed != speed){
      fprintf(stderr, "error: invalid serial speed %d\n", speed);
      close(fd);
      return -1;
    }

    code = speed_table[i].x_code;
#ifdef DEBUG
    fprintf(stderr, "serial: setting serial speed to %d\n", speed);
#endif
    cfsetispeed(&tio, code);
    cfsetospeed(&tio, code);
  }

  if(tcsetattr(fd, TCSANOW, &tio)){
    fprintf(stderr, "error: unable to set serial attributes for %s: %s\n", device, strerror(errno));
    close(fd);
    return -1;
  }

  return fd;
}

//#define MAX_MSGS 20
//#define BUFFER 512
int error_exist(int status_register);
void print_help(char *app);
int main(int argc, char *argv[]){

  char *temp;// = "/dev/ttyMI1";
  //int baud_rate;// = 9600;

  FILE *cfPtr;
  int i, max_cmd, error_query = 0;
  struct timeval timeout = {3, 0};//3 seconds
  int retval;
  fd_set rd_serial_data_fds;

  char buffer[BUFFER], file_name[50], msgs[MAX_MSGS][BUFFER];
  int len = 0, fd = -1;

  device = getenv("DEVICE");
  if(device == NULL){
    device = "/dev/ttyS2";//"/dev/ttyMI1";
  }
  temp = getenv("BAUD_RATE");
  if(temp != NULL){
    baud_rate = atoi(temp);
  } else {        
    baud_rate = 9600;
  }

  for(i=0; baud_rate != speed_table[i].x_speed && speed_table[i].x_speed > 0; i++){}
  if(speed_table[i].x_speed == 0){
    printf("Read baud rate = %d using getenv() is invalid, default is used\n", baud_rate);
    baud_rate = 9600;
  }

  if((fd = start_serial(device, baud_rate)) < 0){
    printf("Application failed while opening or setting the attributes of a serial device: device = %s, baud_rate = %d\n", device, baud_rate);
    close(fd);
    return 0;
  }

  memset(buffer, 0x00, BUFFER); max_cmd = 0;
  if(argc == 1){
    printf("\nTo get started enter -h as an argument to display help commands: i.e. %s -h (then press enter)\n", argv[0]);
    strcpy(msgs[0], "*IDN?");//if no arguments then requet device id
    max_cmd = 1;
  }else if(!strcmp(argv[1], "help") || !strcmp(argv[1], "-h")){
    print_help(argv[0]);
    return 0;

  } else if (!strcmp(argv[1], "config")){
    if(argv[2] != NULL){
      strcpy(file_name, argv[2]);
    } else {
      printf("The configuration file name is not specified\n");
      return 0;
    }
    if((cfPtr = fopen(file_name, "r")) == NULL){
      fprintf(stderr, "error: config file fopen(%s, ): %s\n", file_name, strerror(errno));
      return -1;
    }
    printf("Reading commands from the configuration file defined as %s\n", file_name);
    while(fgets(buffer, BUFFER, cfPtr) != NULL){
      //printf("String read: %s\n",buffer);
      if(buffer[0] != '#'){//not a comment
        if(max_cmd == MAX_MSGS){
          printf("The number of commands read are greater than the defined possible No. = %d\n", MAX_MSGS);
          break;
        }
        strcpy(msgs[max_cmd], buffer);//, strlen(buffer)-1);
        msgs[max_cmd][strlen(buffer)-1] = '\0';//remove '\n'
        max_cmd++;
      }
      memset(buffer, 0x00, strlen(buffer));
    }

    fclose(cfPtr);//finished reading from a file
  }else if(argv[1][0] == '-'){
    //printf("reading char commands ...\n");
    max_cmd = read_char_commands(argc, argv, msgs);
    //printf("Finished reading, max_cmd = %d\n", max_cmd);
  }else {
    //printf("reading full commands ...\n");
    max_cmd = read_full_commands(argc, argv, msgs);
    //printf("Finished reading, max_cmd = %d\n", max_cmd);
  }

  for(i=0; baud_rate!=speed_table[i].x_speed && speed_table[i].x_speed > 0; i++){}

  if(speed_table[i].x_speed == 0){
    printf("baud rate = %d is invalid, the default is used\n", baud_rate);
    baud_rate = 9600;
  }
#if 0
  printf("\ndevice is %s, and the baud_rate is %d\n\n", device, baud_rate);
  for(i=0; i < max_cmd; i++){printf("command: msgs[%d] = %s\n", i, msgs[i]);}
  return 0;
#endif

  if((fd = start_serial(device, baud_rate)) < 0){
    printf("Failed during opening and setting the attributes of a serial device: \
        device = %s, baud_rate = %d\n", device, baud_rate);
    close(fd);
    //free_msgs_buffer(msgs);
    return -1;
  }

  printf("\ndevice is %s, and the baud_rate is %d\n\n", device, baud_rate);
  memset(buffer, 0x00, BUFFER);
  for(i=0; i < max_cmd; i++){
    len = snprintf(buffer, BUFFER, "%s\r\n", msgs[i]);
    if((len <= 0) || (len >= BUFFER)){
      return -1;
    }
    if(write(fd, buffer, len) < 0){
      fprintf(stderr, "error: failed to send a command to %s: %s\n", device, strerror(errno));
      return -1;
    }
    printf("The command sent: \t%s ", buffer);
    memset(buffer, 0x00, len);
    sleep(1);//introduce a delay for the sg device to execute and then respond (only if its a query)

    len = 0;
    if(strchr(msgs[i], '?') != NULL){
      FD_ZERO(&rd_serial_data_fds);
      FD_SET(fd, &rd_serial_data_fds);

      timeout.tv_sec = 1;//1 seconds
      if((retval = select(fd+1, &rd_serial_data_fds, NULL, NULL, &timeout)) <0){
        fprintf(stderr, "error: select()\n");
        return -1;
      }else if(retval == 0){
        printf("No response is available after 1 seconds, and request msgs was: %s\n", msgs[i]);
      }else{
        if((len = read(fd, buffer, BUFFER)) <0){
          fprintf(stderr, "error: failed to read data from serial device\n");
          return -1;
        }

        printf("and the response: \t%s", buffer);
        if(strcmp(msgs[i], "*ESR?") == 0){
          error_query = atoi(buffer);
          if((!error_query || !error_exist(error_query)) && !strcmp(msgs[i+1], "SYST:ERR?")){
            i++;//this means there is no error, then don't read error definition, skip the next command
          }
        }
        memset(buffer, 0x00, len);
      } 
    }
    printf("\n");
  }
  close(fd);

  return 0;
}

int error_exist(int status_register){
  int x = 0;
  if((status_register>>2) & 1){
    printf("Query error detected\n");
    x++;
  }
  if((status_register>>3) & 1){
    printf("Device-dependent error detected\n");
    x++;
  }
  if((status_register>>4) & 1){
    printf("Execution error detected\n");
    x++;
  }
  if((status_register>>5) & 1){
    printf("Command error detected\n");
    x++;
  }
  if((status_register>>6) & 1){
    printf("Device is switched over to manual control: user pressed LOCAL key\n ");
  }

  if((status_register>>7) & 1){
    printf("Device is switched on: user pressed the power button\n");
  }
  return x;
}

void print_help(char *app){
  printf("\nUsage: %s [options] command [args]\n\n", app);
  printf("help or -h              this help\n");
  printf("config file_name        specify the configuration file name - and it should be written in full commands, see config_file.txt\n");
  printf("-e or -e?               check if error exist, and if so then a detailed error status is reported\n");
  printf("-s or -s?               request RF value and output status, power level and state if it was on manual mode or not\n");
  printf("-d device:baud_rate     specify the device:baud rate. DEVICE and BAUD_RATE are environment variables\n");
  printf("-i or -i?               identification query for the device\n");
  printf("\" \"                     if there are no arguments,then its request the device id\n");
  printf("-r                      reset the device\n");
  printf("-c                      clear status register\n");
  printf("-f or -f?               read the current RF frequency, the response units are in Hz.\n");
  printf("-f value[units]         Setting the RF,NO SPACE between value and units, i.e 100MHz\n");
  printf("-p or -p?               request power level of the RF signal\n");
  printf("-p value[units]         set the RF power level\n");
  printf("-o?                     request RF output status, 0 - means switched Off and 1 means On.\n");
  printf("-o ON|OFF               set the RF signal output\n");
  printf("\n");

  printf("Overview of syntax elements of the remote control commands, and for more details refer\n\
  to R&S SML manual(chapter 5 and 6):\n");
  printf(":       the colon separates the key words of a command.\n");
  printf(";       the semicolon separates two commands of a command line.\n");
  printf(",       the comma separates several parameters of a command.\n");
  printf("?       the question mark forms a query.\n");
  printf("*       the asterix marks a common command.\n");
  printf("\"      quotation marks introduce a string and terminate it.\n");
  printf("#       ASCI character # introduces block data. this is considered as a comment\n");
  printf("\" \"   A \"white space\" (ASCII-Code 0 to 9, 11 to 32 decimal, e.g. blank) separates header and parameter.\n");
  printf("\n");
  printf("The samples of few command are shown below: Note that this application is written to accept\n \
  a maximum of two fileds namely: command and argument.\n"); 
  printf("There should be no space between the value and units - 100MHz instead of 100 MHz. \n");
  printf("However the user can use inverted commas to include commands with more than 0ne arguments, \n\
  i.e. \"FREQ 100 MHz\" - the application will interprete this as a single entity and sent it directly to the device.\n");

  printf("Command              Description\n\n");
  printf("*IDN?                Identification query\n");
  printf("*RST                 Reset signal generator\n");
  printf("*CLS                 Clear status register\n");
  printf("*RST;*CLS            Reset and then clear status register - sending more than one command at a time\n");
  printf("FREQUENCY 100MHz     Setting the RF to 100MHz\n");
  printf("FREQUENCY? or FREQ?  Read the current RF frequency\n");
  printf("POWER 10dB           Set the RF Power to 10 dB\n");
  printf("POWER?    or POW?    Request Power level of the RF signal\n");
  printf("OUTPUT ON            Switch On RF output signal\n");
  printf("OUTPUT?              Request RF output signal - 0 means Off and 1 means On.\n");
  printf("\nDEVICE:BAUD_RATE device:baud_rate setting the device and baud rate using then use full commands\n");
  printf("\n");
  
}
