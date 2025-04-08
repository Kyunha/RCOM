#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

typedef enum { LlTx, LlRx } LinkLayerRole;

typedef struct {
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

#define MAX_PAYLOAD_SIZE 1000
#define FALSE 0
#define TRUE 1

int llopen(LinkLayer connectionParameters);
int llwrite(const unsigned char *buf, int bufSize);
int llwrite(int fd, FILE *fptr);
int llclose(int showStatistics);

#endif // _LINK_LAYER_H_