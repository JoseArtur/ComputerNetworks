/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1
//Flags for class 2
#define FLAG 0x7E
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07

unsigned char SET[5];
unsigned char UA[5];

volatile int STOP=FALSE;
int attempts = 0;
int fd;//serial file descriptor
int DONE = FALSE;

void prepareSet(){ //prepare message
  SET[0] = FLAG;
  SET[1] = A;
  SET[2] = C_SET;
  SET[3] = SET[1] ^ SET[2];
  SET[4] = FLAG;
}
void prepareUA(){ //prepare message
  UA[0] = FLAG;
  UA[1] = A;
  UA[2] = C_UA;
  UA[3] = UA[1] ^ UA[2];
  UA[4] = FLAG;
}

void sendSET(){
  if(attempts == 3){
    printf("Failed 3 times, exiting...\n");
    exit(-1);
  }
  printf("Attempt %d/3:\n", (attempts + 1));
  int sentBytes = 0;
  while(sentBytes != 5){
    sentBytes = write(fd, SET, 5);
    printf("sentBytes: %d\n", sentBytes);
  }
  attempts++;
  if(!DONE){
    alarm(3);
  }
}

void receiveUA(int fd){
  unsigned char c;//last char received
  int state = 0;
  printf("Receiving UA...\n");
  while(state != 5){
    read(fd, &c, 1);
    printf("State %d - char: 0x%X\n", state, c);
    switch (state) {
      case 0://expecting flag
        if(c == UA[0]){
          state = 1;
        }//else stay in same state
        break;
      case 1://expecting A
        if(c == UA[1]){
          state = 2;
        }else if(c != UA[0]){//if not FLAG instead of A
          state = 0;
        }//else stay in same state
        break;
      case 2://Expecting C_SET
        if(c == UA[2]){
          state = 3;
        }else if(c == UA[0]){//if FLAG received
          state = 1;
        }else {//else go back to beggining
          state = 0;
        }
        break;
      case 3://Expecting BCC
        if (c == UA[3]){
          state = 4;
        }else {
          state = 0;//else go back to beggining
        }
        break;
      case 4://Expecting FLAG
        if (c == UA[4]){
          state = 5;
        }else{
          state = 0;//else go back to beggining
        }
        break;
    }
  }
  DONE = TRUE;
  alarm(0);
  printf("Received UA properly\n");
}


int main(int argc, char** argv){
    //int c, res;
    struct termios oldtio,newtio;
    //char buf[255];



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
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 1 char received */



  /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) pr�ximo(s) caracter(es)
  */



    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");


    prepareSet();
    prepareUA();
    printf("prepared arrays\n");

    (void) signal(SIGALRM, sendSET);  // instala rotina que atende interrupcao
    sendSET();
    receiveUA(fd);


    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}