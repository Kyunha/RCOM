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
#define _POSIX_SOURCE 1

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5000

#define FLAG 0x7E
#define ADD_S 0x03
#define ADD_R 0x01
#define SET 0x03
#define UA 0x07
#define RR0 0x05
#define RR1 0x85
#define RRJ0 0x01
#define RRJ1 0x81
#define REJ0 0x01
#define REJ1 0x81
#define DISC 0x0B
#define I0 0x00
#define I1 0x40

int RR[] = {RR0, RR1};
int RRJ[] = {RRJ0, RRJ1};
int REJ[] = {REJ0, REJ1};
volatile int RETRANSMIT = TRUE;
volatile int STOP = FALSE;
int fd;

void alarmHandler(int signal) {
    RETRANSMIT = TRUE;
    printf("ALARM: retransmitindo...\n");
}

// **Função llopen() - Mantendo tua máquina de estados original**
int llopen(const char *port) {
    struct termios oldtio, newtio;
    fd = open(port, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(port);
        return -1;
    }

    if (tcgetattr(fd, &oldtio) == -1) {
        perror("tcgetattr");
        return -1;
    }

    memset(&newtio, 0, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
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

    while (STOP == FALSE && attempts < 3) {
        if (RETRANSMIT == TRUE) {
            write(fd, SET_frame, 5);
            alarm(3);
            printf("SET enviado, à espera do UA...\n");
            RETRANSMIT = FALSE;
            attempts++;
        }
        if (read(fd, buf, 1) > 0) {
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

    if (!STOP) {
        printf("UA não recebido! \n");
        close(fd);
        return -1;
    }

    return fd;
}

// **Função llwrite() - Mantendo tua máquina de estados**
int llwrite(unsigned char *data, size_t length, int seqNum) {
     // Número de sequência inicial (I0 ou I1)
     size_t bytes_sent = 0;

    while (bytes_sent < length) {
        // Determinar o tamanho do próximo bloco
        size_t chunk_size = (length - bytes_sent) < BUF_SIZE ? (length - bytes_sent) : BUF_SIZE;
        
        // Criar buffer de stuffing (o dobro do tamanho para segurança)
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

}

// **Função llclose() - Mantendo tua máquina de estados**
int llclose() {
    unsigned char DISC_frame[5] = {FLAG, ADD_S, DISC, ADD_S ^ DISC, FLAG};
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
            }else if (buf[0] == FLAG){ state =ADDRESS;
            }else state = START;
            break;

        case  BCC1:
        printf("%c \n", state);
            if (buf[0] == (ADD_S ^ DISC)){
                state = END;
            }else if (buf[0] == FLAG){ state =ADDRESS;
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
}

// **Camada de Aplicação - Envio do ficheiro**
int main(int argc, char *argv[]) {
    (void)signal(SIGALRM, alarmHandler);

    if (argc < 3) {
        printf("Uso: %s <PortaSerial> <Ficheiro>\n", argv[0]);
        exit(1);
    }

    const char *port = argv[1];
    const char *fileName = argv[2];

    if (llopen(port) == -1) return -1;

    FILE *file = fopen(fileName, "rb");
    if (!file) {
        perror("Erro ao abrir o ficheiro");
        return -1;
    }

    int seqNum = 0;
    size_t bytesRead;
    unsigned char buf[BUF_SIZE];

    while ((bytesRead = fread(buf, 1, BUF_SIZE - 6, file)) > 0) {
        if (llwrite(buf, bytesRead, seqNum) == 1) break;
        seqNum = !seqNum;
    }

    fclose(file);
    llclose();
    return 0;
}
