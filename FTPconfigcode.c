#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// Porta padrão para ligação de controlo do protocolo FTP
#define FTP_PORT 21
#define BUFFER_SIZE 1024

/**
 * Função que lê uma resposta do servidor FTP.
 * Lê do socket até BUFFER_SIZE-1 bytes e imprime no ecrã.
 */
int read_response(int sockfd, char *buffer) {
    int bytes = read(sockfd, buffer, BUFFER_SIZE - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0'; // termina a string
        printf("%s", buffer); // mostra resposta recebida
    }
    return bytes;
}

/**
 * Função que envia um comando FTP para o servidor e
 * lê/imprime a resposta.
 */
void send_cmd(int sockfd, const char *cmd) {
    char buffer[BUFFER_SIZE];
    write(sockfd, cmd, strlen(cmd));     // envia comando
    write(sockfd, "\r\n", 2);            // termina com CRLF conforme RFC959
    read_response(sockfd, buffer);       // lê resposta do servidor
}

/**
 * Função que extrai IP e porto da resposta PASV do servidor FTP.
 * Formato típico da resposta: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2).
 */
void parse_pasv(char *response, char *ip, int *port) {
    int h1, h2, h3, h4, p1, p2;
    sscanf(strchr(response, '(') + 1, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2);
    sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);       // IP = h1.h2.h3.h4
    *port = p1 * 256 + p2;                            // Porta = p1*256 + p2
}

int main() {
    struct hostent *h;                // struct para resolver hostname -> IP
    struct sockaddr_in server_addr;  // endereço do servidor (controlo)
    struct sockaddr_in data_addr;    // endereço da ligação de dados (modo PASV)
    char buffer[BUFFER_SIZE];        // buffer para leitura de mensagens
    char ip[64];                     // IP extraído do modo PASV
    int control_sock, data_sock;     // descritores de socket
    int port;                        // porta para ligação de dados

    // 1. Obter IP do hostname com DNS (ftp.netlab.fe.up.pt)
    if ((h = gethostbyname("ftp.netlab.fe.up.pt")) == NULL) {
        herror("gethostbyname");
        exit(1);
    }

    // 2. Criar socket TCP para a ligação de controlo
    control_sock = socket(AF_INET, SOCK_STREAM, 0);

    // 3. Preparar endereço do servidor FTP (porta 21)
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(FTP_PORT); // htons: host-to-network short
    bcopy(h->h_addr, &server_addr.sin_addr.s_addr, h->h_length); // copiar IP obtido

    // 4. Estabelecer ligação TCP ao servidor (controlo)
    if (connect(control_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(1);
    }

    // 5. Ler mensagem de boas-vindas do servidor FTP (ex: código 220)
    read_response(control_sock, buffer);

    // 6. Enviar credenciais (login anónimo)
    send_cmd(control_sock, "USER anonymous");
    send_cmd(control_sock, "PASS anonymous@");

    // 7. Solicitar modo binário (TYPE I)
    send_cmd(control_sock, "TYPE I");

    // 8. Entrar em modo passivo (PASV), servidor vai indicar IP e porto para dados
    send_cmd(control_sock, "PASV");
    read_response(control_sock, buffer); // resposta com IP e porto
    parse_pasv(buffer, ip, &port);       // extrair IP/porta da resposta

    // 9. Criar socket para a ligação de dados (IP/porto fornecido pelo PASV)
    data_sock = socket(AF_INET, SOCK_STREAM, 0);
    bzero((char *) &data_addr, sizeof(data_addr));
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(port);         // usar a porta do PASV
    inet_aton(ip, &data_addr.sin_addr);       // converter string IP para binário

    // 10. Ligar à porta de dados
    if (connect(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
        perror("connect data");
        exit(1);
    }

    // 11. Enviar comando RETR para fazer download do ficheiro
    send_cmd(control_sock, "RETR pub/README");

    // 12. Ler dados vindos da ligação de dados e guardar localmente
    FILE *fp = fopen("README", "wb");
    int bytes;
    while ((bytes = read(data_sock, buffer, BUFFER_SIZE)) > 0) {
        fwrite(buffer, 1, bytes, fp); // escrever no ficheiro local
    }
    fclose(fp);
    close(data_sock); // fechar ligação de dados

    // 13. Terminar ligação FTP com comando QUIT
    send_cmd(control_sock, "QUIT");
    close(control_sock); // fechar ligação de controlo

    printf("Download completo.\n");
    return 0;
}
