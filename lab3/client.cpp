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
#define PORT 8082

int sockfd;
bool connected = false;

void *receiver(void *arg) {
  while (true) {
    if (!connected) {
      usleep(100000);
      continue;
    }
    Message msg;
    int n = recv(sockfd, &msg, sizeof(Message), 0);
    if (n <= 0) {
      connected = false;
      std::cout << "Disconnected from server." << std::endl;
      continue;
    }

    if (msg.type == MSG_WELCOME) {
      std::cout << "Server: " << msg.payload << std::endl;
    } else if (msg.type == MSG_TEXT) {
      std::cout << "Message: " << msg.payload << std::endl;
    } else if (msg.type == MSG_PONG) {
      std::cout << "PONG" << std::endl;
    } else if (msg.type == MSG_BYE) {
      connected = false;
      std::cout << "Server closed connection." << std::endl;
    }
  }
  return NULL;
}

int main() {
  pthread_t recv_thread;
  pthread_create(&recv_thread, NULL, receiver, NULL);

  while (true) {
    if (!connected) {
      sockfd = socket(AF_INET, SOCK_STREAM, 0);
      struct sockaddr_in servaddr;
      memset(&servaddr, 0, sizeof(servaddr));
      servaddr.sin_family = AF_INET;
      servaddr.sin_port = htons(PORT);
      servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);

      if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        std::cout << "Connection failed. Retrying in 2 seconds..." << std::endl;
        sleep(2);
        continue;
      }

      std::cout << "Connected to server." << std::endl;
      connected = true;

      Message hello;
      hello.type = MSG_HELLO;
      std::string nick = "student";
      strncpy(hello.payload, nick.c_str(), MAX_PAYLOAD);
      hello.length = 1 + nick.length();
      send(sockfd, &hello, sizeof(Message), 0);
    }

    std::string input;
    if (!std::getline(std::cin, input))
      break;

    if (!connected) continue;

    Message msg;
    memset(&msg, 0, sizeof(Message));

    if (input == "/quit") {
      msg.type = MSG_BYE;
      msg.length = 1;
      send(sockfd, &msg, sizeof(Message), 0);
      connected = false;
      close(sockfd);
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

  return 0;
}
