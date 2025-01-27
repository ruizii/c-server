#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFSIZE 4096

int main() {
    struct sockaddr_in client;
    struct sockaddr_in server_addr;
    socklen_t size_of_client;
    int sockfd;
    int client_socket;
    char req[BUFSIZE] = {0};
    char *res = "HTTP/1.1 200 OK\r\nServer: C\r\nCache-Control: no-store\r\nContent-Type: text/html\r\nContent-Length: %ld\r\n\r\n";
    char *res404 = "HTTP/1.1 404 Not Found\r\nServer: C\r\nContent-Length: 0\r\n\r\n";
    char *res400 = "HTTP/1.1 400 Bad request\r\nServer: C\r\nContent-Length: 0\r\n\r\n";
    char buffer[BUFSIZE] = {0};

    signal(SIGCHLD, SIG_IGN);

    server_addr.sin_port = htons(8000);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Error: socket\n");
        return -1;
    }

    if ((bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr))) != 0) {
        fprintf(stderr, "Error: bind\n");
        return -1;
    }

    if ((listen(sockfd, 5)) != 0) {
        fprintf(stderr, "Error: listen\n");
        return -1;
    }

    while (1) {
        size_of_client = sizeof(client);
        client_socket = accept(sockfd, (struct sockaddr *)&client, &size_of_client);

        if (client_socket < 0) {
            fprintf(stderr, "Error: accept\n");
            continue;
        }

        pid_t pid = fork();
        if (pid == -1) {
            fprintf(stderr, "Error: fork\n");
            close(client_socket);
            continue;
        }

        if (pid != 0) { // Si el proceso es el proceso padre, regresar al inicio del loop
            close(client_socket);
            continue;
        }

        // Proceso hijo

        close(sockfd); // El proceso hijo no necesita el socket que escucha

        ssize_t bytes_received = read(client_socket, req, BUFSIZE - 1);
        if (bytes_received <= 0) {
            fprintf(stderr, "Error: read\n");
            close(client_socket);
            exit(1);
        }
        req[bytes_received] = '\0';

        printf("%s", req);

        char *method_end = strchr(req, ' ');
        if (!method_end) {
            write(client_socket, res400, strlen(res400));
            close(client_socket);
            exit(1);
        }
        char *path_start = method_end + 1;
        char *path_end = strchr(path_start, ' ');
        if (!path_end) {
            write(client_socket, res400, strlen(res400));
            close(client_socket);
            exit(1);
        }
        *path_end = '\0';
        char *filename = (*path_start == '/' && *(path_start + 1) == '\0') ? "index.html" : path_start + 1;

        if (strstr(filename, "..") != NULL || *filename == '/') {
            write(client_socket, res400, strlen(res400));
            close(client_socket);
            exit(1);
        }

        FILE *fp = fopen(filename, "rb");
        if (!fp) {
            fprintf(stderr, "El archivo \"%s\" no existe\n", filename);
            write(client_socket, res404, strlen(res404));
            close(client_socket);
            exit(2);
        }

        fseek(fp, 0, SEEK_END);
        long filesize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        char headers[BUFSIZE];
        snprintf(headers, sizeof(headers), res, filesize); // Reemplaza la longitud del contenido en el format string

        write(client_socket, headers, strlen(headers)); // Envia headers

        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, BUFSIZE, fp)) > 0) {
            write(client_socket, buffer, bytes_read); // Envia contenido del archivo solicitado en un loop
        }
        fclose(fp);
        close(client_socket);
        memset(req, 0, BUFSIZE);
        exit(0);
    }

    close(sockfd);
    return 0;
}
