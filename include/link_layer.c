#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include "link_layer.h"

// Variáveis globais 
#define BAUDRATE B38400
#define _POSIX_SOURCE 1
#define BUF_SIZE 5000
#define FLAG 0x7E
#define ADD_S 0x03
#define ADD_R 0x01
#define SET 0x03
#define UA 0x07
#define RR0 0x05
#define RR1 0x85
#define REJ0 0x01
#define REJ1 0x81
#define DISC 0x0B
#define I0 0x00
#define I1 0x40

int RR[] = {RR0, RR1};
int REJ[] = {REJ0, REJ1};
volatile int RETRANSMIT = TRUE;
volatile int STOP = FALSE;
int fd;


void alarmHandler(int signal) {
    RETRANSMIT = TRUE;
    printf("ALARM: retransmitindo...\n");
}


int llopen(LinkLayer connectionParameters) {
    struct termios oldtio, newtio;
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(connectionParameters.serialPort);
        return -1;
    }

    if (tcgetattr(fd, &oldtio) == -1) {
        perror("tcgetattr");
        return -1;
    }

    memset(&newtio, 0, sizeof(newtio));
    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);
    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        return -1;
    }

    unsigned char SET_frame[5] = {FLAG, ADD_S, SET, ADD_S ^ SET, FLAG};
    unsigned char buf[BUF_SIZE] = {0};
    char state = ' ';
    int attempts = 0;

    while (STOP == FALSE && attempts < connectionParameters.nRetransmissions) {
        if (RETRANSMIT == TRUE) {
            write(fd, SET_frame, 5);
            alarm(connectionParameters.timeout);
            printf("SET enviado, à espera do UA...\n");
            RETRANSMIT = FALSE;
            attempts++;
        }
        if (read(fd, buf, 1) > 0) {
            printf("var = 0x%02X\n", buf[0]);
            switch (state) {
                case 'S':
                    if (buf[0] == ADD_S) state = 'A';
                    else if (buf[0] == FLAG) state = 'S';
                    else state = 'E';
                    break;
                case 'A':
                    if (buf[0] == UA) state = 'B';
                    else if (buf[0] == FLAG) state = 'S';
                    else state = 'E';
                    break;
                case 'B':
                    if (buf[0] == (ADD_S ^ UA)) state = 'C';
                    else if (buf[0] == FLAG) state = 'S';
                    else state = 'E';
                    break;
                case 'C':
                    if (buf[0] == FLAG) {
                        STOP = TRUE;
                        alarm(0);
                        printf("UA recebido\n");
                    }
                    else state = 'E';
                    break;
                default:
                    if (buf[0] == FLAG) state = 'S';
                    break;
            }
        }
    }

    if (!STOP) {
        printf("UA não recebido! \n");
        close(fd);
        return -1;
    }

    return fd;
}


int llwrite(const unsigned char *buf, int bufSize) {
    unsigned char data_frame
    int seqNum = 0; // Você pode querer tornar isso global para controle de sequência
    size_t bytes_sent = 0;

    while (bytes_sent < bufSize) {
        size_t chunk_size = (bufSize - bytes_sent) < BUF_SIZE ? (bufSize - bytes_sent) : BUF_SIZE;
        unsigned char stuffed_data[2 * BUF_SIZE];
        int j = 0;

        // **1️⃣ Calcular BCC2 antes do Byte Stuffing**
        unsigned char BCC2 = data[bytes_sent];
        for (size_t i = 1; i < chunk_size; i++) {
            BCC2 ^= data[bytes_sent + i];  // XOR de todos os bytes do bloco
        }

        // **2️⃣ Aplicar Byte Stuffing aos dados**
        for (size_t i = 0; i < chunk_size; i++) {
            if (data[bytes_sent + i] == FLAG) {
                stuffed_data[j++] = 0x7D;
                stuffed_data[j++] = 0x5E;  // FLAG transformada
            } else if (data[bytes_sent + i] == 0x7D) {
                stuffed_data[j++] = 0x7D;
                stuffed_data[j++] = 0x5D;  // Escape transformado
            } else {
                stuffed_data[j++] = data[bytes_sent + i];
            }
        }

        // **3️⃣ Aplicar Byte Stuffing ao BCC2, se necessário**
        if (BCC2 == FLAG) {
            stuffed_data[j++] = 0x7D;
            stuffed_data[j++] = 0x5E;
        } else if (BCC2 == 0x7D) {
            stuffed_data[j++] = 0x7D;
            stuffed_data[j++] = 0x5D;
        } else {
            stuffed_data[j++] = BCC2;
        }

        int stuffed_length = j;  // Comprimento real dos dados com stuffing

        // **4️⃣ Criar quadro de dados**
        unsigned char data_frame[stuffed_length + 6];
        data_frame[0] = FLAG;
        data_frame[1] = ADD_S;
        data_frame[2] = (seqNum == 0) ? I0 : I1;
        data_frame[3] = data_frame[1] ^ data_frame[2];  // BCC1
        memcpy(&data_frame[4], stuffed_data, stuffed_length);
        data_frame[4 + stuffed_length] = FLAG;

        // **5️⃣ Enviar quadro e esperar resposta**
       
        unsigned char buf[BUF_SIZE] = {0};
        char state = 'S';
		
        while (TRUE) {
            
		write(fd, data_frame, stuffed_length + 5);
        printf("Pacote %d enviado (%d bytes, incluindo stuffing): ", seqNum, stuffed_length + 	5);
        for (int k = 0; k < stuffed_length + 5; k++)
            printf("0x%02X ", data_frame[k]);
        printf("\n");

        sleep(1);  // Pequeno atraso para evitar leituras incompletas

        if (read(fd, buf, 1) > 0) {
            printf("Recebido: 0x%02X\n", buf[0]);

            switch (state) {
                case 'S':
                    if (buf[0] == ADD_S) state = 'Z';
                    break;
                case 'Z':
                    if (buf[0] == RR[seqNum]) {
                        printf("Recebido RR: pacote %d transmitido com sucesso!\n", seqNum);
                        bytes_sent += chunk_size;  // Avançar para o próximo bloco
                        seqNum = !seqNum;  // Alternar entre I0 e I1
                        goto next_packet;  // Seguir para o próximo pacote
                    }
                    else if (buf[0] == REJ[seqNum]) {
                        printf("Recebido REJ: retransmitindo pacote %d...\n", seqNum);
                        state = 'S';  // Voltar ao início e retransmitir
                    }
                    break;
            }
        }
            
        }

        printf("Erro: número máximo de retransmissões atingido.\n");
        return -1;

    next_packet:
        continue;
    }

    printf("Transmissão completa!\n");
    return 0;


        
    

    return bytes_sent; // Ou -1 em caso de erro
}

void llread(int fd, FILE *fptr) {
    enum { START, ADDRESS, CONTROL, BCC1, DATA, ACK, DISCONNECT } state = START; //declarar estados
    int data_count = 0;
    unsigned char data[BUF_SIZE] = {0};
    unsigned char buf[1];
    unsigned char infoField = 0;
    int stop = 0;
    
    while (!stop) {
        int infoState = 0;
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
                    printf("Sucesso ADDRESS\n");
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
                } else if (buf[0] == I[infoState]) {
                    infoField = buf[0];  // guardar que I foi recebida
                    state = BCC1;
                    printf("Sucesso Control\n");
                } else if (buf[0] == DISC) {
                    state= DISCONNECT;
                }
                 else {
                    // I errado
                    state = START;
                }
                break;
                
            case BCC1: {
                if (buf[0] == (ADD_S ^ I[infoState])) {
                    // Recebeu BCC1 correto
                    printf("Sucesso BCC1\n");
                    state = DATA;
                    data_count = 0;  // data_count a zero
                } else if (buf[0] == FLAG) {
                    // flag lol
                    state = ADDRESS;
                } else {
                    // Error no BCC1 mandar negative acknoledgement
                    printf("BCC1 errado\n");
                    write(fd, (infoField == I0 ? REJ0_frame : REJ1_frame), 5);
                    state = START;
                }
                break;
            }
                
            case DATA:
                if (buf[0] == FLAG) { // caso receber FLAG final
                    printf("Fim de data\n");

                    if (data_count <= 1) {
                        // Não tem data recomeça
                        state = START;
                    } else {
                        unsigned char BCC2 = 0;
                        BCC2 = data[0];
                        printf("Verificar BCC2\n");
                        for (int j = 1; j <= (data_count - 1); j++) { // Calcular BCC2 enviado
                            BCC2 ^= data[j];
                        }
                        
                        if (BCC2 == data[data_count]) { //Verificar BCC2 
                            state = ACK;
                            printf("Deu certo\n");
                            
                        //unstuff
                        unsigned char unstuffed_data[2 * BUF_SIZE];  // Buffer com espaço extra para unstuffing
                        int skip=0;
                        for (size_t i = 0; i < data_count; i++) {
                            if ((data[i+skip] == 0x7D)){
                                if (data[i+1+skip] == 0x5E) {
                                    unstuffed_data[i] = 0x7E;  // muda
                                    skip++;
                                    data_count--;
                                }else if ((data[i+1+skip] == 0x5D)){
                                    unstuffed_data[i] = 0x7D;  // muda
                                    skip++;
                                    data_count--;
                                }
                            }else {
                                unstuffed_data[i] = data[i+skip];
                            }
                        }

                            for (size_t i = 0; i < data_count; i++) {
                                fprintf(fptr, "%c", unstuffed_data[i]);
                                printf("%c",unstuffed_data[i]);
                            }
                            
                        } else {
                            // BCC2 erro -> negative acknowledgement.
                            printf("Deu ruim\n");
                            write(fd, (infoField == I0 ? REJ0_frame : REJ1_frame), 5);
                            printf("%c",infoField);
                            printf("---\n");
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
                write(fd, (I[infoState] == I0 ? RR1_frame : RR0_frame), 5);
                printf("mandar RR \n");
                printf("---%d\n",I[infoState]);
                infoState = !infoState;
                // Reset da maquina de estados
                state = START;
                data_count = 0;
                break;

                case DISCONNECT:
                printf("disconectar\n");
                if (buf[0] == ADD_S ^ DISC) {
                    // Recebeu BCC1 correto
                    stop=TRUE;
                    write(fd, DISC_frame, 5);
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

int llclose(int showStatistics) {

    / unsigned char DISC_frame[5] = {FLAG, ADD_S, DISC, ADD_S ^ DISC, FLAG};
    write(fd, DISC_frame, 5);
    printf("DISC enviado\n");
    enum { START, ADDRESS, CONTROL, BCC1, END} state = START; //declarar estados

    STOP = FALSE;
    unsigned char buf[BUF_SIZE] = {0};
    int i=0;
    while (STOP == FALSE)
    {
        if(read(fd, buf,1)==0){continue;};
        printf("var = 0x%02X\n", buf[0]);
        // Returns after 5 chars have been input
        switch (state)
        {
        default:
        state = START;
        break; //n deve entrar aqui
        case  START:
            printf("%c \n", state);
            if (buf[0] == FLAG){
                state = ADDRESS;  
            }
            break;

        case  ADDRESS:
        printf("%c \n", state);
            if (buf[0] == ADD_S){
                state = CONTROL;
            }else if (buf[0] == FLAG){ state = ADDRESS;
            }else state = START;
            break;

        case  CONTROL:
        printf("%c \n", state);
            if (buf[0] == (DISC)){
                state = BCC1;
            }else if (buf[0] == FLAG){ state = ADDRESS;
            }else state = START;
            break;

        case  BCC1:
        printf("%c \n", state);
            if (buf[0] == (ADD_S ^ DISC)){
                state = END;
            }else if (buf[0] == FLAG){ state = ADDRESS;
            }else state = START;
            break;

        case  END:
        printf("%c \n", state);
            if (buf[0] == FLAG){
                STOP=TRUE;
            }else state = START;
            break;
        
        }
    }
  
    printf("DISC recebido! Conexão encerrada.\n");
    close(fd);
    return 0;
    
    close(fd);
    return 0;
}
