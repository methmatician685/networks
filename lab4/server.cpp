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

#define PORT 8083

struct Client {
  int sock;
  char nickname[32];
  bool authenticated;
};

std::vector<Client*> clients;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_layer(int layer, const char* msg) {
  std::cout << "[Layer " << layer << "] " << msg << std::endl;
}

void send_msg(int sock, Message* msg) {
  log_layer(7, "prepare response");
  log_layer(6, "serialize Message");
  log_layer(4, "send()");
  send(sock, msg, sizeof(Message), 0);
}

void broadcast(Message *msg) {
  pthread_mutex_lock(&clients_mutex);
  for (auto c : clients) {
    if (c->authenticated) {
      send(c->sock, msg, sizeof(Message), 0);
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

void* handle_client(void* arg) {
  int client_fd = *(int*)arg;
  free(arg);

  Client* self = new Client;
  self->sock = client_fd;
  self->authenticated = false;
  memset(self->nickname, 0, 32);

  while (true) {
    Message msg;
    memset(&msg, 0, sizeof(Message));
    int n = recv(client_fd, &msg, sizeof(Message), 0);

    if (n <= 0) {
      log_layer(4, "recv() - disconnection");
      break;
    }

    log_layer(4, "recv()");
    log_layer(6, "deserialize Message");

    if (!self->authenticated) {
      if (msg.type == MSG_AUTH) {
        log_layer(5, "processing MSG_AUTH");
        bool unique = true;
        pthread_mutex_lock(&clients_mutex);
        for (auto c : clients) {
          if (c->authenticated && strcmp(c->nickname, msg.payload) == 0) {
            unique = false;
            break;
          }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (unique && strlen(msg.payload) > 0) {
          self->authenticated = true;
          strncpy(self->nickname, msg.payload, 31);
          log_layer(5, "authentication success");
          std::cout << "User [" << self->nickname << "] connected" << std::endl;
          
          Message welcome;
          welcome.type = MSG_WELCOME;
          std::string text = "Welcome, " + std::string(self->nickname);
          strncpy(welcome.payload, text.c_str(), MAX_PAYLOAD);
          welcome.length = 1 + text.length();
          send_msg(client_fd, &welcome);

          pthread_mutex_lock(&clients_mutex);
          clients.push_back(self);
          pthread_mutex_unlock(&clients_mutex);

          Message info;
          info.type = MSG_SERVER_INFO;
          std::string info_text = "User [" + std::string(self->nickname) + "] joined the chat";
          strncpy(info.payload, info_text.c_str(), MAX_PAYLOAD);
          info.length = 1 + info_text.length();
          broadcast(&info);
        } else {
          log_layer(5, "authentication failed");
          Message err;
          err.type = MSG_ERROR;
          std::string err_text = "Login failed: nickname taken or empty";
          strncpy(err.payload, err_text.c_str(), MAX_PAYLOAD);
          err.length = 1 + err_text.length();
          send_msg(client_fd, &err);
          break;
        }
      } else {
        log_layer(5, "unauthenticated request ignored");
      }
      continue;
    }

    log_layer(5, "session active");
    if (msg.type == MSG_TEXT) {
      log_layer(7, "handle MSG_TEXT");
      Message chat;
      chat.type = MSG_TEXT;
      std::string text = "[" + std::string(self->nickname) + "]: " + std::string(msg.payload);
      strncpy(chat.payload, text.c_str(), MAX_PAYLOAD);
      chat.length = 1 + text.length();
      broadcast(&chat);
    } else if (msg.type == MSG_PRIVATE) {
      log_layer(7, "handle MSG_PRIVATE");
      std::string payload = msg.payload;
      size_t sep = payload.find(':');
      if (sep != std::string::npos) {
        std::string target_nick = payload.substr(0, sep);
        std::string text = payload.substr(sep + 1);
        
        bool found = false;
        pthread_mutex_lock(&clients_mutex);
        for (auto c : clients) {
          if (c->authenticated && target_nick == c->nickname) {
            Message priv;
            priv.type = MSG_PRIVATE;
            std::string full = "[PRIVATE][" + std::string(self->nickname) + "]: " + text;
            strncpy(priv.payload, full.c_str(), MAX_PAYLOAD);
            priv.length = 1 + full.length();
            send_msg(c->sock, &priv);
            found = true;
            break;
          }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (!found) {
          Message err;
          err.type = MSG_ERROR;
          std::string err_text = "User " + target_nick + " not found";
          strncpy(err.payload, err_text.c_str(), MAX_PAYLOAD);
          err.length = 1 + err_text.length();
          send_msg(client_fd, &err);
        }
      }
    } else if (msg.type == MSG_PING) {
      log_layer(7, "handle MSG_PING");
      Message pong;
      pong.type = MSG_PONG;
      pong.length = 1;
      send_msg(client_fd, &pong);
    } else if (msg.type == MSG_BYE) {
      log_layer(7, "handle MSG_BYE");
      break;
    }
  }

  if (self->authenticated) {
    std::cout << "User [" << self->nickname << "] disconnected" << std::endl;
    pthread_mutex_lock(&clients_mutex);
    for (size_t i = 0; i < clients.size(); ++i) {
      if (clients[i] == self) {
        clients.erase(clients.begin() + i);
        break;
      }
    }
    pthread_mutex_unlock(&clients_mutex);
    
    Message info;
    info.type = MSG_SERVER_INFO;
    std::string info_text = "User [" + std::string(self->nickname) + "] left the chat";
    strncpy(info.payload, info_text.c_str(), MAX_PAYLOAD);
    info.length = 1 + info_text.length();
    broadcast(&info);
  }

  close(client_fd);
  delete self;
  return NULL;
}

int main() {
  int listenfd;
  struct sockaddr_in servaddr;

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(PORT);

  bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
  listen(listenfd, 10);

  std::cout << "Server (OSI Visualization) listening on port " << PORT << "..." << std::endl;

  while (true) {
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);
    int* connfd = (int*)malloc(sizeof(int));
    *connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &len);
    
    pthread_t tid;
    pthread_create(&tid, NULL, handle_client, connfd);
    pthread_detach(tid);
  }

  return 0;
}
