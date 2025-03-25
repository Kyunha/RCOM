#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

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

int i = 0;
int RR[] = {0x05, 0x85};
int RETRANSMIT = TRUE;
int fd;

void alarmHandler(int signal)
{
    RETRANSMIT = TRUE;
    printf("ALARM \n");
}

volatile int STOP = FALSE;

int main(int argc, char *argv[])
{
    (void)signal(SIGALRM, alarmHandler);
    
    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }
    
    const char *serialPortName = argv[1];
    fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio, newtio;
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }



    unsigned char buf[BUF_SIZE] = {0};
    unsigned char SET_frame[5] = {FLAG, ADD_S, SET, ADD_S ^ SET, FLAG};
    unsigned char DISC_frame[5] = {FLAG, ADD_S, DISC, ADD_S ^ DISC, FLAG};
    unsigned char UA_frame[5];
    
    char state = ' ';
    int attempts = 0;
    
    while (STOP == FALSE && attempts < 3)
    {
        if (RETRANSMIT == TRUE)
        {
            write(fd, SET_frame, 5);
            alarm(3);
            printf("SET enviado, á espera do UA...\n");
            RETRANSMIT = FALSE;
            attempts++;
        }
        if (read(fd, buf, 1) > 0)
        {
            printf("var = 0x%02X\n", buf[0]);
            switch (state)
            {
            case 'S':
                printf("%c \n", state);
                if (buf[0] == ADD_S)
                    state = 'A';
                else if (buf[0] == FLAG)
                    state = 'S';
                else
                    state = 'E';
                break;
            case 'A':
                printf("%c \n", state);
                if (buf[0] == UA)
                    state = 'B';
                else if (buf[0] == FLAG)
                    state = 'S';
                else
                    state = 'E';
                break;
            case 'B':
                printf("%c \n", state);
                if (buf[0] == (ADD_S ^ UA))
                    state = 'C';
                else if (buf[0] == FLAG)
                    state = 'S';
                else
                    state = 'E';
                break;
            case 'C':
                printf("%c \n", state);
                if (buf[0] == FLAG)
                {
                    STOP = TRUE;
                    alarm(0);
                    printf("UA recebido\n");
                }
                else
                    state = 'E';
                break;
            default:
                printf("start \n");
                if (buf[0] == FLAG)
                    state = 'S';
                break;
            }
        }
    }
    
    if (!STOP)
    {
        printf("UA não recebido! \n");
        close(fd);
        return -1;
    }
    
    printf("A enviar o DISC...\n");
    write(fd, DISC_frame, 5);
    
    STOP = FALSE;
    while (STOP == FALSE)
    {
        if (read(fd, buf, 1) > 0)
        {
            printf("var = 0x%02X\n", buf[0]);
            switch (state)
            {
            case 'S':
                printf("%c \n", state);
                if (buf[0] == ADD_S)
                    state = 'A';
                else if (buf[0] == FLAG)
                    state = 'S';
                else
                    state = 'E';
                break;
            case 'A':
                printf("%c \n", state);
                if (buf[0] == UA)
                    state = 'B';
                else if (buf[0] == FLAG)
                    state = 'S';
                else
                    state = 'E';
                break;
            case 'B':
                printf("%c \n", state);
                if (buf[0] == (ADD_S ^ UA))
                    state = 'C';
                else if (buf[0] == FLAG)
                    state = 'S';
                else
                    state = 'E';
                break;
            case 'C':
                printf("%c \n", state);
                if (buf[0] == FLAG)
                {
                    STOP = TRUE;
                    printf("UA recebido!\n");
                }
                else
                    state = 'E';
                break;
            default:
                printf("start \n");
                if (buf[0] == FLAG)
                    state = 'S';
                break;
            }
        }
    }
    
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
    return 0;
}
