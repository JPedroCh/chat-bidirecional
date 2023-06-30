#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

// #define MAX_CLIENTS 10
#define MAX_ROOMS 5
#define MAX_ROOM_NAME_LENGTH 50
#define MAX_MESSAGE_SIZE 500
#define MAX_COMMAND_LENGTH 50

int currentMaxRooms = 0;
int closedServer = 0;


// Estrutura de dados para armazenar os clientes
struct Client {
  int fd;
  char name[50];
};

// Estrutura de dados para armazenar as salas
struct Room {
  int roomNumber;
  struct Client* clients;
  int maxClients;
  int numClients;
  char name[MAX_ROOM_NAME_LENGTH + 1];
};


// Estrutura de dados para armazenar os dados do servidor
struct ServerData {
    struct Room rooms[MAX_ROOMS];
    pthread_t commandThread;
    int listener;
    int fdmax;
    fd_set master;
};

// Função responsável pelo comando /list para o cliente listar as salas
char *listRooms(struct Room rooms[]) {
  char tmp[MAX_ROOM_NAME_LENGTH * 25];
  char *roomMessage = malloc(currentMaxRooms * MAX_ROOM_NAME_LENGTH * 25 * sizeof(char));

  sprintf(roomMessage, "\nSalas:\n");
  for (int i = 0; i < currentMaxRooms; i++) {
    sprintf(tmp, "  Room %d: %s (%d/%d clients)\n", rooms[i].roomNumber, rooms[i].name, rooms[i].numClients, rooms[i].maxClients);
    strcat(roomMessage, tmp);
  }
  strcat(roomMessage, "\n");

  return roomMessage;
};

// Função responsável pelo comando /list para o servidor listar as salas
void listRoomsServer(struct Room rooms[]) {
  char tmp[MAX_ROOM_NAME_LENGTH * 25];
  char *roomMessage = malloc(currentMaxRooms * MAX_ROOM_NAME_LENGTH * 25 * sizeof(char));

  sprintf(roomMessage, "\n[INFO] Salas:\n");
  for (int i = 0; i < currentMaxRooms; i++) {
    sprintf(tmp, "Room ID %d: %s (%d/%d clients)\n", rooms[i].roomNumber, rooms[i].name, rooms[i].numClients, rooms[i].maxClients);
    strcat(roomMessage, tmp);
  }
  strcat(roomMessage, "\n");

  printf("%s", roomMessage);
};

void clearBuffer() {
    int c;
    for(int c; (c = getchar()) != '\n' && c != EOF; );
}


// Função de tratamento da conexão de novo cliente
void handleNewClient(struct Room rooms[], int newfd, int *fdmax) {
  int roomIndex = -1;
  char message[MAX_MESSAGE_SIZE];


  if (rooms[0].numClients < rooms[0].maxClients) {
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

  snprintf(message, sizeof(message), "\033[2J\033[HBem vindo a sala principal.\nPrimeiro defina seu nome utilizando o comando /name <NAME>. Depois disso verifique as salas disponiveis utilizando /list e entao /join <ROOM_ID> para entrar em uma sala.\n\n");
  send(newfd, message, strlen(message), 0);

  printf("[INFO] Novo cliente conectou-se a sala principal\n");
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

// Função de tratamento de dados recebidos
void handleReceivedData(struct Room rooms[], int i, char *buf, int *fdmax, fd_set *master) {
  int indexRoom = -1;
  int indexClient = -1;
  char message[MAX_MESSAGE_SIZE];

  // Identifica a sala e o cliente que enviou a mensagem
  for (int j = 0; j < currentMaxRooms && indexRoom < 0; j++) {
    for (int k = 0; k < rooms[j].numClients; k++) {
      if (rooms[j].clients[k].fd == i) {
        indexRoom = j;
        indexClient = k;
        break;
      }
    }
  }

  // Armazena o file descriptor do cliente
  int client = rooms[indexRoom].clients[indexClient].fd;

  // Tratamento para comando /name, o qual define nome do cliente
  if (strncmp(buf, "/name ", 6) == 0) {
    strncpy(rooms[indexRoom].clients[indexClient].name, buf + 6, strlen(buf) - 2);
    rooms[indexRoom].clients[indexClient].name[strlen(rooms[indexRoom].clients[indexClient].name) - 2] = '\0';
    
    snprintf(message, sizeof(message), "[SUCCESS] Nome atualizado com sucesso: %s.\n Utilize o comando /help para visualizar os comandos disponíveis\n", rooms[indexRoom].clients[indexClient].name);
    send(client, message, strlen(message), 0);
  } else {
    if (rooms[indexRoom].clients[indexClient].name[0] == '\0') {
      snprintf(message, sizeof(message), "[ERROR] Defina seu nome de usuario primeiro.\n");
      send(client, message, strlen(message), 0);
      return;
    }

    if (buf[0] == '/') {
      // Tratamento para comando /list, o qual lista as salas disponiveis e numero de clientes em cada uma
      if (strncmp(buf, "/list", 5) == 0) {
        printf("[INFO] Cliente %d na sala %d executou o comando /list\n", i, rooms[indexRoom].roomNumber);
        char *message = listRooms(rooms);
        send(client, message, strlen(message), 0);

        // Tratamento para comando /join, o qual troca o cliente de sala
      } else if (strncmp(buf, "/join ", 6) == 0) {
        printf("[INFO] Cliente %d na sala %d executou o comando /join\n", i, rooms[indexRoom].roomNumber);
        int requestedRoom = atoi(buf + 6);

        if (requestedRoom < 1 || requestedRoom > currentMaxRooms) {
          // Sala invalida
          snprintf(message, sizeof(message), "[ERROR] Sala invalida.\n");
          send(client, message, strlen(message), 0);
        } else if (rooms[requestedRoom - 1].numClients >= rooms[requestedRoom - 1].maxClients) { 
          // Sala Lotada
          snprintf(message, sizeof(message), "[ERROR] Sala lotada.\n");
          send(client, message, strlen(message), 0);
        } else {
          // Adiciona o cliente na sala desejada
          int clientIndex = rooms[requestedRoom - 1].numClients;
          rooms[requestedRoom - 1].clients[clientIndex].fd = i;
          strcpy(rooms[requestedRoom - 1].clients[clientIndex].name, rooms[indexRoom].clients[indexClient].name);
          rooms[requestedRoom - 1].numClients++;

          // Remove o cliente da sala antiga
          for(int i = indexClient; i < rooms[indexRoom].numClients; i++) {
            rooms[indexRoom].clients[i] = rooms[indexRoom].clients[i + 1];
          }
          rooms[indexRoom].clients[rooms[indexRoom].numClients].fd = -1;
          rooms[indexRoom].clients[rooms[indexRoom].numClients].name[0] = '\0';
          rooms[indexRoom].numClients--;

          snprintf(message, sizeof(message), "\033[2J\033[H[SUCCESS] Conectado a sala %d.\n\nBem vindo a sala %d.\nComandos disponiveis:\n  /join: Troca de sala\n  /list: Lista todas as salas disponiveis\n  /name: Trocar de nome de usuario\n  /leave: Sair para sala principal\n  /help: Visualizar comandos disponíveis\n\n", requestedRoom, requestedRoom);
          send(client, message, strlen(message), 0);
          printf("[INFO] Client %d switched to Room %d\n", i, requestedRoom - 1);
        }
        
        // Tratamento para comando /leave, o qual retorna o cliente para a sala principal (lounge)
      } else if (strncmp(buf, "/leave", 5) == 0) {
        printf("[INFO] Cliente %d na sala %d executou o comando /leave\n", i, rooms[indexRoom].roomNumber);
        if (indexRoom == 0) {
          snprintf(message, sizeof(message), "[ERROR] Você já está na sala principal.\n");          
          send(client, message, strlen(message), 0);
          return;
        }

        // Adiciona o cliente na sala principal
        rooms[0].clients[rooms[0].numClients].fd = i;
        strcpy(rooms[0].clients[rooms[0].numClients].name, rooms[indexRoom].clients[indexClient].name);
        rooms[0].numClients++;

        // Remove o cliente da sala antiga
        for(int i = indexClient; i < rooms[indexRoom].numClients; i++) {
          rooms[indexRoom].clients[i] = rooms[indexRoom].clients[i + 1];
        }

        rooms[indexRoom].numClients--;
        rooms[indexRoom].clients[rooms[indexRoom].numClients].fd = -1;

        snprintf(message, sizeof(message), "[SUCCESS] Você saiu da sala. Você está de volta a sala principal\n");
        send(client, message, strlen(message), 0);
        printf("[INFO] Cliente %d saiu da sala %s (id %d)\n", i, rooms[indexRoom].name, indexRoom);

        // Tratamento para comando /exit, o qual desconecta o cliente do servidor
      } else if (strncmp(buf, "/exit", 4) == 0) {
        printf("[INFO] Cliente %d na sala %d executou o comando /exit\n", i, rooms[indexRoom].roomNumber);

        // Move todos os clientes uma casa para a esquerda para preencher o espaço vazio deixado pelo cliente desconectado
        for(int i = indexClient; i < rooms[indexRoom].numClients; i++) {
          rooms[indexRoom].clients[i] = rooms[indexRoom].clients[i + 1];
        }
        rooms[indexRoom].clients[rooms[indexRoom].numClients].fd = -1;
        rooms[indexRoom].clients[rooms[indexRoom].numClients].name[0] = '\0';

        rooms[indexRoom].numClients--;

        snprintf(message, sizeof(message), "[SUCCESS] Você foi desconectado.\n");
        send(client, message, strlen(message), 0);
        printf("[INFO] Cliente com ID %d desconectou.\n", i);

        // Fecha o socket do cliente e remove da lista de sockets 
        fdmax = fdmax - 1; 
        close(i);
        FD_CLR(i, master);

      // Tratamento para comando /help, o qual lista todos os comandos disponiveis
      } else if (strncmp(buf, "/help", 4) == 0) {
        printf("[INFO] Cliente %d na sala %d executou o comando /help\n", i, rooms[indexRoom].roomNumber);
        snprintf(message, sizeof(message), "[INFO] Comandos disponiveis:\n  /join: Troca de sala\n  /list: Lista todas as salas disponiveis\n  /name: Trocar de nome de usuario\n  /leave: Sair para sala principal\n  /help: Visualizar comandos disponíveis\n\n");
        send(client, message, strlen(message), 0);
      } else {
        snprintf(message, sizeof(message), "[ERROR] Comando invalido.\n");
        send(client, message, strlen(message), 0);
      }

    } else {
      // Caso o cliente tente enviar uma mensagem estando na sala principal (lounge), retorna erro
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


// Função que remove um cliente de uma sala
void removeClientFromRoom(struct Room rooms[], int fd){
  for (int i = 0; i < currentMaxRooms; i++) {
    for (int j = 0; j < rooms[i].numClients; j++) {
      if (rooms[i].clients[j].fd == fd) {
        rooms[i].clients[j].fd = -1;
        rooms[i].numClients--;
        return;
      }
    }
  }
}

// Função que cria uma nova sala
void createRoom(struct Room rooms[]) {
  if (currentMaxRooms == MAX_ROOMS) {
    printf("Numero maximo de salas atingido.\n");
    return;
  }

  char name[MAX_ROOM_NAME_LENGTH];
  int maxRoomClients;
  printf("Digite o nome da sala: ");
  scanf("%s", name);
  clearBuffer();
  printf("Digite o número máximo de usuários da sala: ");
  scanf("%d", &maxRoomClients);
  clearBuffer();


  // Inicializa a sala com os dados informados
  int roomNumber = currentMaxRooms + 1;
  rooms[currentMaxRooms].roomNumber = roomNumber;
  rooms[currentMaxRooms].numClients = 0;
  rooms[currentMaxRooms].maxClients = maxRoomClients;
  rooms[currentMaxRooms].clients = malloc(sizeof(struct Client) * maxRoomClients);
  snprintf(rooms[currentMaxRooms].name, MAX_ROOM_NAME_LENGTH + 1,  "%s", name);

  currentMaxRooms++;

  printf("[SUCCESS] Sala com ID \"%d\" criada com sucesso.\n", roomNumber);
}


// Função que deleta uma sala
void deleteRoom(struct Room rooms[]) {
  int roomNumber;
  printf("Digite o numero da sala: ");
  scanf("%d", &roomNumber);

  if (roomNumber > currentMaxRooms || roomNumber <= 0) {
    printf("Sala invalida.\n");
    return;
  }

  if (roomNumber == 1) {
    printf("Não é possivel deletar a sala principal.\n");
    return;
  }

  // Move todos os clientes da sala a ser deletada para a sala principal
  for (int i = rooms[roomNumber - 1].numClients - 1; i >= 0; i--) {
    rooms[0].numClients++;
    rooms[0].clients[rooms[0].numClients].fd = rooms[roomNumber - 1].clients[i].fd;
    strcpy(rooms[0].clients[rooms[0].numClients].name, rooms[roomNumber - 1].clients[i].name);
    rooms[roomNumber - 1].clients[i].fd = -1;
    snprintf(rooms[roomNumber - 1].name, MAX_ROOM_NAME_LENGTH + 1,  "");
  }
  rooms[roomNumber - 1].numClients = 0;

  for (int i = roomNumber - 1; i < currentMaxRooms - 1; i++) {
    rooms[i] = rooms[i + 1];
  }

  currentMaxRooms--;

  printf("[SUCCESS] Sala com ID \"%d\" deletada com sucesso.\n", roomNumber);
}


// Função que lista os clientes de uma sala
void listUsersFromRoom(struct Room room) {
  char message[MAX_MESSAGE_SIZE];
  if (room.numClients == 0) {
    printf("[INFO] Nao ha ninguem na sala");
    return;
  }

  printf("[INFO] Lista de usuário da sala %s (id %d)\n", room.name, room.roomNumber);
  for (int i = 0; i < room.numClients; i++) {
    printf("%d) %s (user id: %d)\n", i+1, room.clients[i].name, room.clients[i].fd);
  }
  printf("\n");
}

void printCommands() {
  printf("\n[INFO] Comandos disponíveis:\n");
  printf("/list - Lista as salas disponíveis\n");
  printf("/users <id da sala> - Lista os usuários de uma sala\n");
  printf("/create - Cria uma nova sala\n");
  printf("/delete - Deleta uma sala\n");
  printf("/help - Lista os comandos disponíveis\n");
  printf("/exit - Encerra o servidor\n\n\n");
};

void* commandInput(void* arg) {
    struct ServerData *serverData = (struct ServerData*)arg;
     int c;
    char command[MAX_COMMAND_LENGTH];

    printCommands();
    
    while (1) {
        scanf("%[^\n]", command);
        clearBuffer();

        if (strcmp(command, "/list") == 0) {
            listRoomsServer(serverData->rooms);
        } else if (strncmp(command, "/users ", 7) == 0) {
            int requestedRoom = atoi(command + 7);
            listUsersFromRoom(serverData->rooms[requestedRoom - 1]);
        } else if (strcmp(command, "/create") == 0) {
            createRoom(serverData->rooms);
        } else if (strcmp(command, "/delete") == 0) {
            deleteRoom(serverData->rooms);
        } else if (strcmp(command, "/exit") == 0) {
            exit(0);
        } else if (strcmp(command, "/help") == 0) {
          printCommands();
        } else {
            printf("Invalid command.\n");
        }
    }
    
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
  int newfd, i, nbytes;
  struct sockaddr_in myaddr, remoteaddr;
  socklen_t addrlen;
  char buf[256];
  fd_set read_fds;
  int yes = 1;
  struct ServerData serverData;

  if (argc < 3) {
    printf("Digite IP e Porta para este servidor\n");
    exit(1);
  } 

  // Inicializa a sala principal
  serverData.rooms[0].roomNumber = 1;
  serverData.rooms[0].numClients = 0;
  snprintf(serverData.rooms[0].name, MAX_ROOM_NAME_LENGTH + 1, "Lounge");
  serverData.rooms[0].maxClients = 50;
  serverData.rooms[0].clients = malloc(sizeof(struct Client) * 50);
  currentMaxRooms = 1;
  

  FD_ZERO(&(serverData.master));
  FD_ZERO(&read_fds);

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  serverData.listener = socket(AF_INET, SOCK_STREAM, 0);
  if (serverData.listener < 0) {
    perror("socket");
    exit(1);
  }

  setsockopt(serverData.listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = inet_addr(argv[1]);
  myaddr.sin_port = htons(atoi(argv[2]));
  memset(&(myaddr.sin_zero), '\0', 8);

  if (bind(serverData.listener, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
    perror("[ERROR - bind] ");
    exit(1);
  }

  if (listen(serverData.listener, 10) < 0) {
    perror("[ERROR - listen] ");
    exit(1);
  }

  FD_SET(serverData.listener, &(serverData.master));
  serverData.fdmax = serverData.listener;

  printf("[INFO] Servidor rodando. Aguardando conexões...\n");

  // Cria a thread para receber comandos do servidor
  if (pthread_create(&(serverData.commandThread), NULL, commandInput, (void*)&serverData) != 0) {
      perror("[ERROR - pthread_create] ");
      exit(1);
  }

  for (;;) {
    read_fds = (serverData.master);
    if (select(serverData.fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
      perror("[ERROR - Select] ");
      exit(1);
    }

    for (i = 0; i <= serverData.fdmax; i++) {
      if (FD_ISSET(i, &read_fds)) {
        if (i == serverData.listener) {
          addrlen = sizeof(remoteaddr);
          newfd = accept(serverData.listener, (struct sockaddr *)&remoteaddr, &addrlen);
          if (newfd == -1) {
            perror("accept");
          } else {
            FD_SET(newfd, &(serverData.master));
            if (newfd > serverData.fdmax) {
              serverData.fdmax = newfd;
            }
            // Adiciona o cliente na sala principal
            handleNewClient(serverData.rooms, newfd, &serverData.fdmax);
          }
        } else {
          memset(buf, 0, sizeof(buf));
          nbytes = recv(i, buf, sizeof(buf), 0);
          if (nbytes <= 0) {
            if (nbytes == 0) {
              printf("Client %d disconnected\n", i);
              // Remove o cliente da sala por ter se desconectado
              removeClientFromRoom(serverData.rooms, i);
              
            } else {
              perror("recv");
            }
            close(i);
            FD_CLR(i, &(serverData.master));
          } else {
            // Trata os dados recebidos
            handleReceivedData(serverData.rooms, i, buf, &serverData.fdmax, &serverData.master);
          }
        }
      }
    }
  }

  return 0;
}
