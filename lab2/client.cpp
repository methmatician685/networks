#include "protocol.h"
#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8081

int main() {
  int sockfd;
  struct sockaddr_in servaddr;

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return 1;
  }

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(PORT);
  servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);

  if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    perror("connect");
    return 1;
  }

  std::cout << "connected to server." << std::endl;

  Message hello;
  hello.type = MSG_HELLO;
  std::string nickname = "student";
  strncpy(hello.payload, nickname.c_str(), MAX_PAYLOAD);
  hello.length = 1 + nickname.length();
  send(sockfd, &hello, sizeof(Message), 0);

  fd_set readfds;
  while (true) {
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(sockfd, &readfds);

    int max_fd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;

    if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
      perror("select");
      break;
    }

    if (FD_ISSET(sockfd, &readfds)) {
      Message msg;
      int n = recv(sockfd, &msg, sizeof(Message), 0);
      if (n <= 0) {
        std::cout << "disconnected from server." << std::endl;
        break;
      }

      if (msg.type == MSG_WELCOME) {
        std::cout << "Server: " << msg.payload << std::endl;
      } else if (msg.type == MSG_TEXT) {
        std::cout << "Message: " << msg.payload << std::endl;
      } else if (msg.type == MSG_PONG) {
        std::cout << "PONG" << std::endl;
      } else if (msg.type == MSG_BYE) {
        std::cout << "server closed connection (BYE)." << std::endl;
        break;
      }
    }

    if (FD_ISSET(STDIN_FILENO, &readfds)) {
      std::string input;
      if (!std::getline(std::cin, input))
        break;

      Message msg;
      memset(&msg, 0, sizeof(Message));

      if (input == "/quit") {
        msg.type = MSG_BYE;
        msg.length = 1;
        send(sockfd, &msg, sizeof(Message), 0);
        break;
      } else if (input == "/ping") {
        msg.type = MSG_PING;
        msg.length = 1;
        send(sockfd, &msg, sizeof(Message), 0);
      } else {
        msg.type = MSG_TEXT;
        strncpy(msg.payload, input.c_str(), MAX_PAYLOAD);
        msg.length = 1 + input.length();
        send(sockfd, &msg, sizeof(Message), 0);
      }
    }
  }

  close(sockfd);
  return 0;
}
