#include "protocol.h"
#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8081

void handle_client(int client_fd, struct sockaddr_in cliaddr) {
  char client_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(cliaddr.sin_addr), client_ip, INET_ADDRSTRLEN);
  int client_port = ntohs(cliaddr.sin_port);

  std::cout << "client connected from " << client_ip << ":" << client_port
            << std::endl;

  Message msg;

  if (recv(client_fd, &msg, sizeof(Message), 0) <= 0)
    return;
  if (msg.type == MSG_HELLO) {
    std::cout << "[" << client_ip << ":" << client_port << "]: Hello ("
              << msg.payload << ")" << std::endl;

    Message welcome;
    welcome.type = MSG_WELCOME;
    std::string welcome_text =
        "welcome " + std::string(client_ip) + ":" + std::to_string(client_port);
    strncpy(welcome.payload, welcome_text.c_str(), MAX_PAYLOAD);
    welcome.length = 1 + welcome_text.length();
    send(client_fd, &welcome, sizeof(Message), 0);
  }

  while (true) {
    memset(&msg, 0, sizeof(Message));
    int n = recv(client_fd, &msg, sizeof(Message), 0);
    if (n <= 0) {
      std::cout << "client disconnected" << std::endl;
      break;
    }

    if (msg.type == MSG_TEXT) {
      std::cout << "[" << client_ip << ":" << client_port
                << "]: " << msg.payload << std::endl;
    } else if (msg.type == MSG_PING) {
      std::cout << "[" << client_ip << ":" << client_port << "]: PING"
                << std::endl;
      Message pong;
      pong.type = MSG_PONG;
      pong.length = 1;
      send(client_fd, &pong, sizeof(Message), 0);
    } else if (msg.type == MSG_BYE) {
      std::cout << "client sent BYE" << std::endl;
      break;
    }
  }
  close(client_fd);
}

int main() {
  int listenfd, connfd;
  struct sockaddr_in servaddr, cliaddr;

  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return 1;
  }

  int opt = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(PORT);

  if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    perror("bind");
    return 1;
  }

  if (listen(listenfd, 1) < 0) {
    perror("listen");
    return 1;
  }

  std::cout << "server listening on port " << PORT << "..." << std::endl;

  while (true) {
    socklen_t len = sizeof(cliaddr);
    connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &len);
    if (connfd < 0) {
      perror("accept");
      continue;
    }
    handle_client(connfd, cliaddr);
  }

  close(listenfd);
  return 0;
}
