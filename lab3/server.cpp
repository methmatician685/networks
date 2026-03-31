#include "protocol.h"
#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <pthread.h>
#include <queue>
#include <string.h>
#include <sys/socket.h>
#include <vector>
#include <unistd.h>
#include <algorithm>

#define PORT 8082
#define THREAD_POOL_SIZE 10

std::queue<int> client_queue;
std::vector<int> clients;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

void broadcast(Message *msg, int sender_fd) {
  pthread_mutex_lock(&clients_mutex);
  for (int fd : clients) {
    send(fd, msg, sizeof(Message), 0);
  }
  pthread_mutex_unlock(&clients_mutex);
}

void *worker(void *arg) {
  while (true) {
    int client_fd;
    pthread_mutex_lock(&queue_mutex);
    while (client_queue.empty()) {
      pthread_cond_wait(&queue_cond, &queue_mutex);
    }
    client_fd = client_queue.front();
    client_queue.pop();
    pthread_mutex_unlock(&queue_mutex);

    Message msg;
    if (recv(client_fd, &msg, sizeof(Message), 0) <= 0) {
      close(client_fd);
      continue;
    }

    if (msg.type == MSG_HELLO) {
      std::string nick = msg.payload;
      std::cout << "User connected: " << nick << std::endl;

      Message welcome;
      welcome.type = MSG_WELCOME;
      std::string text = "Welcome, " + nick;
      strncpy(welcome.payload, text.c_str(), MAX_PAYLOAD);
      welcome.length = 1 + text.length();
      send(client_fd, &welcome, sizeof(Message), 0);

      pthread_mutex_lock(&clients_mutex);
      clients.push_back(client_fd);
      pthread_mutex_unlock(&clients_mutex);

      while (true) {
        memset(&msg, 0, sizeof(Message));
        int n = recv(client_fd, &msg, sizeof(Message), 0);
        if (n <= 0) {
          std::cout << "User disconnected" << std::endl;
          break;
        }

        if (msg.type == MSG_TEXT) {
          broadcast(&msg, client_fd);
        } else if (msg.type == MSG_PING) {
          Message pong;
          pong.type = MSG_PONG;
          pong.length = 1;
          send(client_fd, &pong, sizeof(Message), 0);
        } else if (msg.type == MSG_BYE) {
          break;
        }
      }

      pthread_mutex_lock(&clients_mutex);
      for (size_t i = 0; i < clients.size(); ++i) {
        if (clients[i] == client_fd) {
          clients.erase(clients.begin() + i);
          break;
        }
      }
      pthread_mutex_unlock(&clients_mutex);
    }
    close(client_fd);
  }
  return NULL;
}

int main() {
  int listenfd, connfd;
  struct sockaddr_in servaddr, cliaddr;

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(PORT);

  bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
  listen(listenfd, 10);

  pthread_t pool[THREAD_POOL_SIZE];
  for (int i = 0; i < THREAD_POOL_SIZE; i++) {
    pthread_create(&pool[i], NULL, worker, NULL);
  }

  std::cout << "Server (Thread Pool) listening on port " << PORT << "..." << std::endl;

  while (true) {
    socklen_t len = sizeof(cliaddr);
    connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &len);
    pthread_mutex_lock(&queue_mutex);
    client_queue.push(connfd);
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);
  }

  return 0;
}
