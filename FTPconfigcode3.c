#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define FTP_PORT 21
#define BUFFER_SIZE 1024

typedef struct {
    char user[64];
    char password[64];
    char host[256];
    char path[512];
} ftp_url;

int parse_url(const char *url, ftp_url *result) {
    if (strncmp(url, "ftp://", 6) != 0) return -1;

    const char *start = url + 6;
    const char *at = strchr(start, '@');
    const char *slash = strchr(start, '/');

    if (at && (!slash || at < slash)) {
        // URL com autenticação
        sscanf(start, "%63[^:]:%63[^@]@%255[^/]/%511[^\n]",
               result->user, result->password, result->host, result->path);
    } else {
        // URL anónima
        strcpy(result->user, "anonymous");
        strcpy(result->password, "anonymous@");

        if (slash) {
            sscanf(start, "%255[^/]/%511[^\n]", result->host, result->path);
        } else {
            sscanf(start, "%255[^\n]", result->host);
            result->path[0] = '\0';  // Sem path definido
        }
    }

    return 0;
}

int read_response(int sockfd, char *buffer) {
    int bytes = read(sockfd, buffer, BUFFER_SIZE - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }
    return bytes;
}

void send_cmd(int sockfd, const char *cmd) {
    char buffer[BUFFER_SIZE];
    write(sockfd, cmd, strlen(cmd));
    write(sockfd, "\r\n", 2);
    read_response(sockfd, buffer);
}

int parse_pasv(char *response, char *ip, int *port) {
    int h1, h2, h3, h4, p1, p2;
    char *start = strchr(response, '(');
    if (!start) return -1;

    if (sscanf(start + 1, "%d,%d,%d,%d,%d,%d",
               &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
        return -1;
    }

    sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
    *port = p1 * 256 + p2;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s ftp://[user[:pass]@]host/path\n", argv[0]);
        return 1;
    }

    ftp_url info;
    if (parse_url(argv[1], &info) != 0 || strlen(info.host) == 0 || strlen(info.path) == 0) {
        fprintf(stderr, "URL inválida ou incompleta.\n");
        return 1;
    }

    // DNS
    struct hostent *h;
    if ((h = gethostbyname(info.host)) == NULL) {
        herror("gethostbyname");
        return 1;
    }

    int control_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(FTP_PORT);
    memcpy(&server_addr.sin_addr, h->h_addr, h->h_length);

    if (connect(control_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }

    char buffer[BUFFER_SIZE];
    read_response(control_sock, buffer);  // 220

    // Autenticação
    char cmd[128];
    sprintf(cmd, "USER %s", info.user);
    send_cmd(control_sock, cmd);
    sprintf(cmd, "PASS %s", info.password);
    send_cmd(control_sock, cmd);

    send_cmd(control_sock, "TYPE I");     // Modo binário
    send_cmd(control_sock, "PASV");       // Entrar em modo passivo
    read_response(control_sock, buffer);

    char ip[64]; int port;
    if (parse_pasv(buffer, ip, &port) < 0) {
        fprintf(stderr, "Erro ao interpretar resposta PASV.\n");
        return 1;
    }

    // Ligação de dados
    int data_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in data_addr;
    bzero(&data_addr, sizeof(data_addr));
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(port);
    inet_aton(ip, &data_addr.sin_addr);

    if (connect(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
        perror("connect data");
        return 1;
    }

    sprintf(cmd, "RETR %s", info.path);
    send_cmd(control_sock, cmd);

    const char *filename = strrchr(info.path, '/');
    filename = filename ? filename + 1 : info.path;

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    int bytes;
    while ((bytes = read(data_sock, buffer, BUFFER_SIZE)) > 0) {
        fwrite(buffer, 1, bytes, fp);
    }
    fclose(fp);
    close(data_sock);

    send_cmd(control_sock, "QUIT");
    close(control_sock);

    printf("Download completo: %s\n", filename);
    return 0;
}