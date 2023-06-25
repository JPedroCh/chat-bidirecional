#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_CLIENTS 10
#define MAX_ROOMS 5
#define MAX_ROOM_NAME_LENGTH 50
#define MAX_MESSAGE_SIZE 500

struct Client {
  int fd;
  char name[50];
};

struct Room {
  int roomNumber;
  struct Client clients[MAX_CLIENTS];
  int numClients;
  char name[MAX_ROOM_NAME_LENGTH + 1];
};

char *listRooms(struct Room rooms[]) {
  char tmp[MAX_ROOM_NAME_LENGTH * 25];
  char *roomMessage = malloc(MAX_ROOMS * MAX_ROOM_NAME_LENGTH * 25 * sizeof(char));

  sprintf(roomMessage, "\nSalas:\n");
  for (int i = 0; i < MAX_ROOMS; i++) {
    sprintf(tmp, "  Room %d: %s (%d/%d clients)\n", rooms[i].roomNumber, rooms[i].name, rooms[i].numClients, MAX_CLIENTS);
    strcat(roomMessage, tmp);
  }
  strcat(roomMessage, "\n");

  return roomMessage;
};

void handleNewClient(struct Room rooms[], int newfd, int *fdmax) {
  int roomIndex = -1;
  char message[MAX_MESSAGE_SIZE];


  if (rooms[0].numClients < MAX_CLIENTS) {
    roomIndex = 0;
  }

  if (roomIndex == -1) {
    printf("Sala principal esta cheia.\n");
    close(newfd);
    return;
  }

  int clientIndex = rooms[roomIndex].numClients;
  rooms[roomIndex].clients[clientIndex].fd = newfd;
  rooms[roomIndex].numClients++;
  if (newfd > *fdmax) {
    *fdmax = newfd;
  }

  snprintf(message, sizeof(message), "\033[2J\033[HBem vindo a sala principal.\nPrimeiro defina seu nome utilizando o comando \\name <NAME>. Depois disso verifique as salas disponiveis utilizando \\list e entao \\join <ROOM_ID> para entrar em uma sala.\n\n");
  send(newfd, message, strlen(message), 0);

  printf("Novo cliente conectou-se a sala principal\n");
};

void sendToRoomChatExcept(struct Room rooms[], int indexRoom, int excludedUser, char *message) {
  for (int j = 0; j < rooms[indexRoom].numClients; j++) {
    int client = rooms[indexRoom].clients[j].fd;
    if (client != excludedUser) {
      send(client, message, strlen(message), 0);
    }
  }
}

void sendToRoomChat(struct Room rooms[], int indexRoom, char *message) {
  for (int j = 0; j < rooms[indexRoom].numClients; j++) {
    int client = rooms[indexRoom].clients[j].fd;
    send(client, message, strlen(message), 0);
  }
}

void handleReceivedData(struct Room rooms[], int i, char *buf) {
  int indexRoom = -1;
  int indexClient = -1;
  char message[MAX_MESSAGE_SIZE];

  for (int j = 0; j < MAX_ROOMS && indexRoom < 0; j++) {
    for (int k = 0; k < MAX_CLIENTS; k++) {
      if (rooms[j].clients[k].fd == i) {
        indexRoom = j;
        indexClient = k;
        break;
      }
    }
  }

  int client = rooms[indexRoom].clients[indexClient].fd;

  if (strncmp(buf, "/name ", 6) == 0) {
    strncpy(rooms[indexRoom].clients[indexClient].name, buf + 6, strlen(buf) - 2);
    rooms[indexRoom].clients[indexClient].name[strlen(rooms[indexRoom].clients[indexClient].name) - 2] = '\0';
    
    snprintf(message, sizeof(message), "[SUCCESS] Nome atualizado com sucesso: %s.\n", rooms[indexRoom].clients[indexClient].name);
    send(client, message, strlen(message), 0);
  } else {
    if (rooms[indexRoom].clients[indexClient].name[0] == '\0') {
      snprintf(message, sizeof(message), "[ERROR] Defina seu nome de usuario primeiro.\n");
      send(client, message, strlen(message), 0);
      return;
    }

    if (buf[0] == '/') {
      if (strncmp(buf, "/list", 5) == 0) {
        printf("Cliente %d na sala %d executou o comando /list\n", i, rooms[indexRoom].roomNumber);
        char *message = listRooms(rooms);
        send(client, message, strlen(message), 0);
      } else if (strncmp(buf, "/join ", 6) == 0) {
        printf("Cliente %d na sala %d executou o comando /join\n", i, rooms[indexRoom].roomNumber);
        int requestedRoom = atoi(buf + 6);
        if (requestedRoom > 0 && requestedRoom <= MAX_ROOMS) {
          rooms[indexRoom].clients[indexClient].fd = -1;
          rooms[indexRoom].numClients--;

          int clientIndex = rooms[requestedRoom - 1].numClients;
          rooms[requestedRoom - 1].clients[clientIndex].fd = i;
          strcpy(rooms[requestedRoom - 1].clients[clientIndex].name, rooms[indexRoom].clients[indexClient].name);

          rooms[requestedRoom - 1].numClients++;

          snprintf(message, sizeof(message), "\033[2J\033[H[SUCCESS] Conectado a sala %d.\n\nBem vindo a sala %d.\nComandos disponiveis:\n  \\join: Troca de sala\n  \\list: Lista todas as salas disponiveis\n  \\name: Trocar de nome de usuario\n  \\leave: Sair para sala principal\n\n", requestedRoom, requestedRoom);
          send(client, message, strlen(message), 0);
          printf("Client %d switched to Room %d\n", i, requestedRoom - 1);
        } else {
          snprintf(message, sizeof(message), "[ERROR] Sala invalida.\n");
          send(client, message, strlen(message), 0);
        }
      } else if (strncmp(buf, "/leave", 5) == 0) {
        printf("Cliente %d na sala %d executou o comando /leave\n", i, rooms[indexRoom].roomNumber);
        if (indexRoom == 0) {
          snprintf(message, sizeof(message), "[ERROR] Você já está na sala principal.\n");          
          send(client, message, strlen(message), 0);
          return;
        }

        rooms[indexRoom].numClients--;
        rooms[indexRoom].clients[indexClient].fd = -1;

        rooms[0].numClients++;
        rooms[0].clients[rooms[0].numClients].fd = i;
        strcpy(rooms[0].clients[rooms[0].numClients].name, rooms[indexRoom].clients[indexClient].name);

        snprintf(message, sizeof(message), "[SUCCESS] Você saiu da sala. Você está de volta a sala principal\n");
        send(client, message, strlen(message), 0);
        printf("Cliente %d saiu da sala %d\n", i, indexRoom);
      } else {
        snprintf(message, sizeof(message), "[ERROR] Comando invalido.\n");
        send(client, message, strlen(message), 0);
      }
    } else {
      if (indexRoom == 0) {
        snprintf(message, sizeof(message), "[ERROR] Conecte-se a uma sala primeiro para enviar uma mensagem.\n");
        send(client, message, strlen(message), 0);
        return;
      }

      snprintf(message, sizeof(message), "%s: %s", rooms[indexRoom].clients[indexClient].name, buf);
      sendToRoomChatExcept(rooms, indexRoom, i, message);
    }
  }
}

int main(int argc, char *argv[]) {
  int listener, newfd, fdmax, i, nbytes;
  struct sockaddr_in myaddr, remoteaddr;
  socklen_t addrlen;
  char buf[256];
  fd_set master, read_fds;
  int yes = 1;

  if (argc < 3) {
    printf("Digite IP e Porta para este servidor\n");
    exit(1);
  }

  // Initialize room data
  struct Room rooms[MAX_ROOMS];
  for (int i = 0; i < MAX_ROOMS; i++) {
    rooms[i].roomNumber = i + 1;
    rooms[i].numClients = 0;
    snprintf(rooms[i].name, MAX_ROOM_NAME_LENGTH + 1, "Room %d", i + 1);
  }

  FD_ZERO(&master);
  FD_ZERO(&read_fds);

  listener = socket(AF_INET, SOCK_STREAM, 0);
  if (listener < 0) {
    perror("socket");
    exit(1);
  }

  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = inet_addr(argv[1]);
  myaddr.sin_port = htons(atoi(argv[2]));
  memset(&(myaddr.sin_zero), '\0', 8);

  if (bind(listener, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
    perror("bind");
    exit(1);
  }

  if (listen(listener, 10) < 0) {
    perror("listen");
    exit(1);
  }

  FD_SET(listener, &master);
  fdmax = listener;

  printf("Server is running. Waiting for connections...\n");

  for (;;) {
    read_fds = master;
    if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
      perror("select");
      exit(1);
    }

    for (i = 0; i <= fdmax; i++) {
      if (FD_ISSET(i, &read_fds)) {
        if (i == listener) {
          addrlen = sizeof(remoteaddr);
          newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);
          if (newfd == -1) {
            perror("accept");
          } else {
            FD_SET(newfd, &master);
            if (newfd > fdmax) {
              fdmax = newfd;
            }

            handleNewClient(rooms, newfd, &fdmax);
          }
        } else {
          memset(buf, 0, sizeof(buf));
          nbytes = recv(i, buf, sizeof(buf), 0);
          if (nbytes <= 0) {
            if (nbytes == 0) {
              printf("Client %d disconnected\n", i);
            } else {
              perror("recv");
            }
            close(i);
            FD_CLR(i, &master);
          } else {
            handleReceivedData(rooms, i, buf);
          }
        }
      }
    }
  }

  return 0;
}
