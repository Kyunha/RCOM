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

//Declarar frames
unsigned char UA_frame[5] = {FLAG, ADD_S, UA, (ADD_S ^ UA), FLAG};
unsigned char RR0_frame[5] = {FLAG, ADD_S, RR0, (ADD_S ^ RR0), FLAG};
unsigned char RR1_frame[5] = {FLAG, ADD_S, RR1, (ADD_S ^ RR1), FLAG};
unsigned char REJ0_frame[5] = {FLAG, ADD_S, REJ0, (ADD_S ^ REJ0), FLAG};
unsigned char REJ1_frame[5] = {FLAG, ADD_S, REJ1, (ADD_S ^ REJ1), FLAG};
unsigned char REJ1_frame[5] = {FLAG, ADD_S, DISC, (ADD_S ^ DISC), FLAG};
unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
unsigned char frame[5];

int I[2]={0x00,0x40};

volatile int STOP = FALSE;

void infoFrameRead(int fd) {
    enum { START, ADDRESS, CONTROL, BCC1, DATA, ACK, DISCONNECT } state = START; //decalrar estados
    int data_count = 0;
    unsigned char data[BUF_SIZE] = {0};
    unsigned char buf[1];
    unsigned char infoField = 0;
    int stop = 0;
    
    while (!stop) {
        // Bloqueia
        while(read(fd, buf, 1) <= 0){}//esperar até ter dados no buffer
        printf("buf[0]: 0x%02X, State: %d\n", buf[0], state);
        switch (state) {
            case START:
                if (buf[0] == FLAG) {
                    // receba
                    state = ADDRESS;
                }
            
                break;
                
            case ADDRESS:
                if (buf[0] == FLAG) {
                    // FLAG repetiu
                    state = ADDRESS;
                } else if (buf[0] == ADD_S) {
                    state = CONTROL;
                } else {
                    // Address n reconhecido
                    state = START;
                }
                break;
                
            case CONTROL:
                if (buf[0] == FLAG) {
                    // flag, repetir address
                    state = ADDRESS;
                } else if (buf[0] == I0 || buf[0] == I1) {
                    infoField = buf[0];  // guardar que I foi recebida
                    state = BCC1;
                } else if (buf[0] == DISC) {
                    state= DISCONNECT;
                }
                 else {
                    // I errado
                    state = START;
                }
                break;
                
            case BCC1: {
                if (buf[0] == ADD_S ^ infoField) {
                    // Recebeu BCC1 correto
                    state = DATA;
                    data_count = 0;  // data_count a zero
                } else if (buf[0] == FLAG) {
                    // flag lol
                    state = ADDRESS;
                } else {
                    // Error no BCC1 mandar negative acknoledgement
                    write(fd, (infoField == I0 ? RR0_frame : RR1_frame), 5);
                    state = START;
                }
                break;
            }
                
            case DATA:
                if (buf[0] == FLAG) { // caso receber FLAG final
                    if (data_count <= 1) {
                        // Não tem data recomeça
                        state = START;
                    } else {
                        unsigned char BCC2 = 0;
                        BCC2 = data[0];
                        for (int j = 1; j < data_count - 1; j++) { // Calcular BCC2 enviado
                            BCC2 ^= data[j];
                        }
                        
                        if (BCC2 == data[data_count]) { //Verificar BCC2 
                            state = ACK;
                        } else {
                            // BCC2 erro -> negative acknowledgement.
                            write(fd, (infoField == I0 ? REJ0_frame : REJ1_frame), 5);
                            state = START;
                        }
                    }
                } 
                else { //Nao tem flag -> ler data
                    if (data_count < BUF_SIZE) { //apanhar possivel overflow
                        data[data_count] = buf[0];
                        data_count++;
                    } else {
                        // Overflow
                        printf("overflow\n");
                        state = START;
                    }
                }
                break;
                
                case ACK:
                printf("Dados recebidos.\n Bytes de dados: %d\n", data_count - 1);
                // positive ack
                write(fd, (infoField == I0 ? RR1_frame : RR0_frame), 5);
                
                // Reset da maquina de estados
                state = START;
                data_count = 0;
                break;

                case DISCONNECT:
                if (buf[0] == ADD_S ^ DISC) {
                    // Recebeu BCC1 correto
                    stop=TRUE;
                } else if (buf[0] == FLAG) {
                    // flag lol
                    state = ADDRESS;
                } else {
                    // Error no BCC1 
                    //write(fd, (infoField == I0 ? RR0_frame : RR1_frame), 5);
                    state = START;
                }
                break;
                
            default:
                // Nao deve entrar neste estado
                state = START;
                break;
        } 
    } 
}


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
                write (fd, UA_frame,5); //mandar UA frame
            }else if (buf[0] == FLAG){ state ='S';
            }else state = 'E';
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
