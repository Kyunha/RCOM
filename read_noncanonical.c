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
#define BUF_SIZE 256

#define FLAG    0x7E
#define ADD_S   0x03 // frames sent by the Sender or answers from the Receiver
#define ADD_R   0x01 // frames sent by the Receiver or answers from the Sender
#define SET     0x03
#define UA      0x07

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
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 5;  // Blocking read until 5 chars received

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
    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
    unsigned char frame[5];


    //Maquina de estados
    char state=' ';
    int i=0;
    while (STOP == FALSE)
    {
        // Returns after 5 chars have been input
        switch (state)
        {
        case  'S':
            printf("%c \n", state);
            read(fd, buf,1);
            printf("var = 0x%02X\n", buf[0]);
            if (buf[0] == ADD_S){
                state = 'A';
            }else if (buf[0] == FLAG){ state ='S';
            }else state = 'E';
            break;

        case  'A':
        printf("%c \n", state);
            read(fd, buf,1);
            printf("var = 0x%02X\n", buf[0]);
            if (buf[0] == SET){
                state = 'B';
            }else if (buf[0] == FLAG){ state ='S';
            }else state = 'E';
            break;

        case  'B':
        printf("%c \n", state);
            read(fd, buf,1);
            printf("var = 0x%02X\n", buf[0]);
            if (buf[0] == (ADD_S ^ SET)){
                state = 'C';
            }else if (buf[0] == FLAG){ state ='S';
            }else state = 'E';
            break;

        case  'C':
        printf("%c \n", state);
            read(fd, buf,1);
            printf("var = 0x%02X\n", buf[0]);
            if (buf[0] == FLAG){
                state = ' ';
                STOP = TRUE;
                write (fd, UA_frame,5); //mandar UA frame
            }else if (buf[0] == FLAG){ state ='S';
            }else state = 'E';
            break;
        
        default:
        printf("start \n");
            read(fd, buf,1);
            printf("var = 0x%02X\n", buf[0]);
            if (buf[0] == FLAG)
                state = 'S';
            
            break;
        }
    }
    
    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
