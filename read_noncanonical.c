// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

#define FLAG    0x7E
#define ADD_S   0x03 // frames sent by the Sender or answers from the Receiver
#define ADD_R   0x01 // frames sent by the Receiver or answers from the Sender
#define SET     0x03
#define UA      0x07
#define RR0     0x05
#define RR1     0x85
#define REJ0    0x01
#define REJ1    0x81
#define DISC    0x0B
#define I0      0x00
#define I1      0x40

int I[2]={0x00,0x40};

volatile int STOP = FALSE;



int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;
    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0.1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");
    // Loop for input
    //Declarar frames
    unsigned char UA_frame[5] = {FLAG, ADD_S, UA, (ADD_S ^ UA), FLAG};
    unsigned char RR0_frame[5] = {FLAG, ADD_S, RR0, (ADD_S ^ RR0), FLAG};
    unsigned char RR1_frame[5] = {FLAG, ADD_S, RR1, (ADD_S ^ RR1), FLAG};
    unsigned char REJ0_frame[5] = {FLAG, ADD_S, REJ0, (ADD_S ^ REJ0), FLAG};
    unsigned char REJ1_frame[5] = {FLAG, ADD_S, REJ1, (ADD_S ^ REJ1), FLAG};
    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
    unsigned char frame[5];


    //Maquina de estados
    char state=' ';
    int i=0;
    while (STOP == FALSE)
    {
        if(read(fd, buf,1)==0){continue;};
        printf("var = 0x%02X\n", buf[0]);
        // Returns after 5 chars have been input
        switch (state)
        {
        default:
        printf("start \n");
            if (buf[0] == FLAG)
                state = 'S';
            
            break;
            
        case  'S':
            printf("%c \n", state);
            if (buf[0] == ADD_S){
                state = 'A';
            }else if (buf[0] == FLAG){ state ='S';
            }else state = 'E';
            break;

        case  'A':
        printf("%c \n", state);
            if (buf[0] == SET){
                state = 'B';
            }else if (buf[0] == FLAG){ state ='S';
            }else state = 'E';
            break;

        case  'B':
        printf("%c \n", state);
            if (buf[0] == (ADD_S ^ SET)){
                state = 'C';
            }else if (buf[0] == FLAG){ state ='S';
            }else state = 'E';
            break;

        case  'C':
        printf("%c \n", state);
            if (buf[0] == FLAG){
                state = ' ';
                STOP = TRUE;
                
            }else if (buf[0] == FLAG){ state ='S';
            }else state = 'E';
            break;
        
        }
    }

    
    int S=0;
    i = 0;
    write (fd, UA_frame,5); //mandar UA frame
    printf("Escrever UA \n");
    unsigned char data[BUF_SIZE]={0};
    STOP=FALSE;
    while (STOP == FALSE)
    {
        printf("entrou \n");
        while(read(fd, buf,1)==0);
        printf("var = 0x%02X\n", buf[0]);
        // Returns after 5 chars have been input
        switch (S)
        {
        default:
        printf("start \n");
            if (buf[0] == FLAG)
                S=1;
            break;
        case -1: //erro, reenviar o ready to receive
            if(i==0){
                write (fd, RR0_frame,5);
            }else{write (fd, RR1_frame,5);}
            S++;
            break;
        case  1:    //Address
            printf("%d \n", S);
            if (buf[0] == ADD_S){
                S++;
            }else if (buf[0] == FLAG){ break;
            }else S=-1;
            break;
        case  2:    //I 0 ou 1
            printf("%d \n", S);
            if (buf[0] == I[i]){
                S++;
            }else if (buf[0] == FLAG){ S=1;
            }else S=-1;
            break;
        case  3:    //BCC1
            printf("%d \n", S);
            if (buf[0] == ADD_S^I[i]){
                S++;
            }else if (buf[0] == FLAG){ S=1;
            }else S=-1;
            break;
        case  4:    //Receber data
            int i = 0;
            while (buf[0]!=FLAG)
            {
                data[i] = buf[0];
                i++;
                while(read(fd, buf,1)==0);
            }
            printf("saiu \n");
            break;
        case  5:
            printf("%d \n", S);
            if (buf[0] == FLAG){
                S++;
            }else if (buf[0] == FLAG){ S=1;
            }else S=-1;
            break;
        case  6:
            printf("Acabou");
            STOP==TRUE;
            i=!i;
            if(i==0){
                write (fd, RR0_frame,5);
            }else{write (fd, RR1_frame,5);}
            break;
        
        }
    }
    
    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide
    sleep(10);
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
