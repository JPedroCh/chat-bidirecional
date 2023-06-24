#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_CLIENTS 10
#define MAX_ROOMS 5
#define MAX_ROOM_NAME_LENGTH 50

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



void listRooms(struct Room rooms[]) {
    printf("Rooms:\n");
    for (int i = 0; i < MAX_ROOMS; i++) {
        printf("Room %d: %s (%d/%d clients)\n", rooms[i].roomNumber, rooms[i].name, rooms[i].numClients, MAX_CLIENTS);
    }
}

void handleNewClient(struct Room rooms[], int newfd, int *fdmax) {
    int roomIndex = -1;
    
    if (rooms[0].numClients < MAX_CLIENTS) {
        roomIndex = 0;
    }

    if (roomIndex == -1) {
        printf("Lounge is full right now.\n");
        close(newfd);
        return;
    }

    // Add the new client to the room
    int clientIndex = rooms[roomIndex].numClients;
    rooms[roomIndex].clients[clientIndex].fd = newfd;
    rooms[roomIndex].numClients++;
    if (newfd > *fdmax) {
        *fdmax = newfd;
    }
    
    printf("New client connected to Lounge\n");
}

void handleReceivedData(struct Room rooms[], int i, char *buf) {    
    if (strncmp(buf, "/list", 5) == 0) {
        // Handle the "/list" command to list all rooms
        listRooms(rooms);
        return;
    } else {
      int found = 0;
      int indexRoom = 0;
      int indexClient = 0;
      // Find the room where the client is currently in
      for(int j = 0; j < MAX_ROOMS && found == 0; j++) {
          for(int k = 0; k < MAX_CLIENTS; k++) {
              if (rooms[j].clients[k].fd == i) {
                  indexRoom = j;
                  indexClient = k;
                  found = 1;  
                  break;
              }
          }
      }

      if (strncmp(buf, "/join ", 6) == 0) {
          int requestedRoom = atoi(buf + 6);
          if (requestedRoom > 0 && requestedRoom <= MAX_ROOMS) {
              // Remove the client from the current room
              rooms[indexRoom].clients[indexClient].fd = -1;
              rooms[indexRoom].numClients--;

              // Add the client to the requested room
              int clientIndex = rooms[requestedRoom - 1].numClients;
              rooms[requestedRoom - 1].clients[clientIndex].fd = i;
              rooms[requestedRoom - 1].numClients++;
              printf("Client %d switched to Room %d\n", i, requestedRoom - 1);
          } else {
              printf("Invalid room number.\n");
              return;
          }
      }

    printf("Received data from client %d in Room %d: %s\n", i, rooms[indexRoom].roomNumber, buf);
    
    // Send the received data to all clients in the same room
    for (int j = 0; j < rooms[indexRoom].numClients; j++) {
        int client = rooms[indexRoom].clients[j].fd;
        if (client != i) {
            send(client, buf, strlen(buf), 0);
        }
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
