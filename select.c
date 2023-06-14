#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

#define STDIN 0

fd_set master, read_fds, write_fds;
struct sockaddr_in myaddr, remoteaddr;
int fdmax, sd, newfd, i, j, nbytes, yes = 1;
socklen_t addrlen;
char buf[256];

void envia_msg() {
    for (j = 0; j <= fdmax; j++) {
        if (FD_ISSET(j, &master)) {
            if ((j != i) && (j != sd)) {
                send(j, buf, nbytes, 0);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Digite IP e Porta para este servidor\n");
        exit(1);
    }

    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    sd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = inet_addr(argv[1]);
    myaddr.sin_port = htons(atoi(argv[2]));

    memset(&(myaddr.sin_zero), '\0', 8);
    addrlen = sizeof(remoteaddr);
    bind(sd, (struct sockaddr *)&myaddr, sizeof(myaddr));

    listen(sd, 10);
    FD_SET(sd, &master);
    FD_SET(STDIN, &master);
    fdmax = sd;
    for (;;) {
        read_fds = master;
        select(fdmax + 1, &read_fds, NULL, NULL, NULL);
        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == sd) {
                    newfd = accept(sd, (struct sockaddr *)&remoteaddr, &addrlen);
                    char *client_ip = inet_ntoa(remoteaddr.sin_addr);
                    printf("Novo cliente conectado: %s\n", client_ip);
                    FD_SET(newfd, &master);
                    if (newfd > fdmax)
                        printf("newfd > fdmax\n");
                    fdmax = newfd;
                } else {
                    memset(&buf, 0, sizeof(buf));
                    nbytes = recv(i, buf, sizeof(buf), 0);
                    char *client_ip = inet_ntoa(remoteaddr.sin_addr);
                    strcat(buf, " - ");
                    strcat(buf, client_ip);
                    strcat(buf, "\n");
                    nbytes = strlen(buf) + 1;
                    envia_msg();
                }
            }
        }
    }
    return 0;
}
