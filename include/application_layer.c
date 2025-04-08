#include "application_layer.h"
#include "link_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate, 
                      int nTries, int timeout, const char *filename) {
    // Configura a DLL
    LinkLayer config;
    strcpy(config.serialPort, serialPort);
    config.role = (strcmp(role, "tx") == 0) ? LlTx : LlRx;
    config.baudRate = baudRate;
    config.nRetransmissions = nTries;
    config.timeout = timeout;

    // Abre conexão
    int fd = llopen(config);
    if (fd < 0) {
        fprintf(stderr, "Falha ao iniciar conexão\n");
        return;
    }

    // Se for transmissor (Tx)
    if (config.role == LlTx) {
        FILE *file = fopen(filename, "rb");
        if (!file) {
            perror("Erro ao abrir arquivo");
            llclose(TRUE);
            return;
        }

        // Lê o arquivo e envia via DLL
        unsigned char buffer[MAX_PAYLOAD_SIZE];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, MAX_PAYLOAD_SIZE, file))) {
            if (llwrite(buffer, bytesRead) < 0) {
                fprintf(stderr, "Falha ao enviar dados\n");
                break;
            }
        }
        fclose(file);
    }

    // Fecha conexão
    llclose(TRUE);
}