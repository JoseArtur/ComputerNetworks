/*Non-Canonical Input Processing*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

// Added by student
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
// -----------------

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07

volatile int STOP=FALSE;

unsigned char UA[5];

char* get_cmd(int fd){
	char c;
	char* ret = malloc(sizeof(char)*2);
	int state=0;
	int BCC_OK;
	while (1) {       /* loop for input */
		int res = read(fd, &c, 1);   /* returns after 1 char has been input */
		printf("read %x state:%d\n", c, state);
		char packet_A, packet_C;
		switch (state){
			case 0:
				if (c==FLAG){
					state = 1; // this is the beginning of a packet
				}else
					state=0;
			break;
			case 1:
				packet_A=c; // we received the byte A, here. Storing.
				if(c==0x03)
					state = 2;
				else
					state = 0;
			break;
			case 2:
				packet_C=c; // we received the byte C, here. Storing.
				if(c==C_SET)
					state=3;
				else
					state=0;
			break;
			case 3: 
				if((packet_A ^ packet_C)==c)
					BCC_OK=1;
				else
					BCC_OK=0;

				if(BCC_OK)
					state=4;
				else
					state=0;
			break;
			case 4:
				if(c==FLAG){
					// SUCCESS!!
					ret[0]=packet_A;
					ret[1]=packet_C;
					return ret;
				}else{
					state=0;
				}			
			break;

		}	
    }
	return ret;
}

void prepareUA(){ //prepare message
  UA[0] = FLAG;
  UA[1] = A;
  UA[2] = C_UA;
  UA[3] = UA[1] ^ UA[2];
  UA[4] = FLAG;
}


int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];




  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  
    
    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd <0) {perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */



  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) próximo(s) caracter(es)
  */



    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");

	char * ret=get_cmd(fd);
	printf("get_cmd has returned! Press any key to find out the result (insert drum roll here)\n");
	getc(stdin);

	printf("We got a %x!\n", ret[1]);
	prepareUA();
	int written=write(fd, UA, 5);
	printf("Wrote %d/5 bytes of UA response.\n", written);


    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
	free(ret);
    return 0;
}