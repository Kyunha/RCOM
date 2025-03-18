// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]
#include <signal.h>
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

#define BUF_SIZE 5

#define FLAG 0x7E
#define ADD_S 0x03 // frames sent by the Sender or answers from the Receiver
#define ADD_R 0x01 // frames sent by the Receiver or answers from the Sender
#define SET 0x03
#define UA 0x07
#define RR0 0x05
#define RR1 0x85
#define RRJ0 0x01
#define RRJ1 0x81
#define DISC 0x0B
#define I0 0x00
#define I1 0x40

int i=0;

int RR[]={0x05,0x85};

int RETRANSMIT = TRUE;

void alarmHandler(int signal)          // User-defined function to handle alarms (handler function)
    {                                       // This function will be called when the alarm is triggered
    RETRANSMIT=TRUE;
    printf("ALARM \n");
    sleep(1);
    }

volatile int STOP = FALSE;

int main(int argc, char *argv[])
{
    (void) signal(SIGALRM, alarmHandler);// Install the function signal to be automatically
     // invoked when the timer expires, invoking in its turn
       // the user function alarmHandler
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
   

    // Open serial port device for reading and writing, and not as controlling tty
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
    newtio.c_cc[VTIME] = 0;// Inter-character timer unused
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

    // Create string to send
    unsigned char buf[BUF_SIZE] = {0};
    unsigned char SET_frame[5] = {FLAG, ADD_S, SET, ADD_S^SET, FLAG};
    unsigned char test[5] = {FLAG, ADD_S, I0, ADD_S^I0, FLAG};
    
    
    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.
    //buf[5] = '\n'
    
    //Maquina de estados
    char state=' ';
    int i=0;
    //alarm(2); // Enable alarm in t seconds
    while (STOP == FALSE)
    {
        // Retransmitir
      
        if (RETRANSMIT==TRUE){
            int bytes = write(fd, SET_frame, 5);
            alarm(3);
            printf("%d Á espera\n", bytes);
            sleep(1);
            RETRANSMIT = FALSE;

        }
        //read(fd,buf,1);
        if(read(fd,buf,1) == 0){continue;}
        printf("var = 0x%02X\n",buf[0]); 
        switch (state)
        {
        case  'S':
            printf("%c \n", state);
            if (buf[0] == ADD_S){
                state = 'A';
            }else if (buf[0] == FLAG){ state ='S';
            }else state = 'E';
            break;

        case  'A':
        printf("%c \n", state);
            if (buf[0] == UA){
                state = 'B';
            }else if (buf[0] == FLAG){ state ='S';
            }else state = 'E';
            break;

        case  'B':
        printf("%c \n", state);
            if (buf[0] == (ADD_S ^ UA)){
                state = 'C';
            }else if (buf[0] == FLAG){ state ='S';
            }else state = 'E';
            break;

        case  'C':
        printf("%c \n", state);
            if (buf[0] == FLAG){
                state = ' ';
                STOP = TRUE;
                alarm(0); //disable o alarme
                write(fd,test,5);
            }else state = 'E';
            break;
        
        default:
        printf("start \n");
            if (buf[0] == FLAG)
                state = 'S';
            
            break;
        }
    }
    
    
    state=' ';
    i=0;
    i=!i;
    STOP=FALSE;
    //alarm(2); // Enable alarm in t seconds
    while (STOP == FALSE)
    {
        
        if(read(fd,buf,1) == 0){continue;}
        printf("var = 0x%02X\n",buf[0]); 
        switch (state)
        {
        case  'S':
            printf("%c \n", state);
            if (buf[0] == ADD_S){
                state = 'A';
                printf("rroz");
            }else if (buf[0] == FLAG){ state ='S';
            }else state = 'E';
            break;

        case  'A':
        printf("%c \n", state);
            if (buf[0]== RR[i]){
                state = 'B';
            }else if (buf[0]==RR[!i]){
                state= 'E';
            }else if (buf[0] == FLAG){ state ='S';
            }else state = 'E';
            break;

        case  'B':
        printf("%c \n", state);
            if (buf[0] == (ADD_S ^ RR[i])){
                state = 'C';
            }else if (buf[0]==RR[!i]){
                state= 'E';
            }else if (buf[0] == FLAG){ state ='S';
            }else state = 'E';
            break;

        case  'C':
        printf("%c \n", state);
            if (buf[0] == FLAG){
                printf("oooo");
                state = ' ';
                write(fd,test,5);
                STOP = TRUE;
                i=!i;
          
            }else state = 'E';
            break;
        
        default:
        printf("start \n");
            if (buf[0] == FLAG)
                state = 'S';
            
            break;
        }
    }
    


    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
