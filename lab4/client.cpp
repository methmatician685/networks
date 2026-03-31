#include "protocol.h"
#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8083

int sockfd;

void *receiver(void *arg) {
  while (true) {
    Message msg;
    int n = recv(sockfd, &msg, sizeof(Message), 0);
    if (n <= 0) {
      std::cout << "Disconnected from server." << std::endl;
      exit(0);
    }

    if (msg.type == MSG_WELCOME) {
      std::cout << "Server: " << msg.payload << std::endl;
    } else if (msg.type == MSG_TEXT) {
      std::cout << msg.payload << std::endl;
    } else if (msg.type == MSG_PRIVATE) {
      std::cout << msg.payload << std::endl;
    } else if (msg.type == MSG_PONG) {
      std::cout << "PONG" << std::endl;
    } else if (msg.type == MSG_ERROR) {
      std::cout << "Error: " << msg.payload << std::endl;
    } else if (msg.type == MSG_SERVER_INFO) {
      std::cout << "[SERVER INFO]: " << msg.payload << std::endl;
    } else if (msg.type == MSG_BYE) {
      std::cout << "Server closed connection." << std::endl;
      exit(0);
    }
  }
  return NULL;
}

int main() {
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(PORT);
  servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);

  if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    perror("connect");
    return 1;
  }

  std::cout << "Connected to server." << std::endl;
  std::cout << "Enter nickname: ";
  std::string nick;
  std::getline(std::cin, nick);

  Message auth;
  auth.type = MSG_AUTH;
  strncpy(auth.payload, nick.c_str(), MAX_PAYLOAD);
  auth.length = 1 + nick.length();
  send(sockfd, &auth, sizeof(Message), 0);

  pthread_t recv_thread;
  pthread_create(&recv_thread, NULL, receiver, NULL);

  while (true) {
    std::string input;
    if (!std::getline(std::cin, input)) break;

    Message msg;
    memset(&msg, 0, sizeof(Message));

    if (input.substr(0, 3) == "/w ") {
      msg.type = MSG_PRIVATE;
      std::string body = input.substr(3);
      size_t space = body.find(' ');
      if (space != std::string::npos) {
        std::string target = body.substr(0, space);
        std::string text = body.substr(space + 1);
        std::string payload = target + ":" + text;
        strncpy(msg.payload, payload.c_str(), MAX_PAYLOAD);
        msg.length = 1 + payload.length();
        send(sockfd, &msg, sizeof(Message), 0);
      } else {
        std::cout << "Usage: /w <nick> <message>" << std::endl;
      }
    } else if (input == "/quit") {
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

  close(sockfd);
  return 0;
}
