#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
  int sockfd;
  char buffer[BUFFER_SIZE];
  struct sockaddr_in servaddr, cliaddr;

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    std::cerr << "socket creation failed" << std::endl;
    return 1;
  }

  memset(&servaddr, 0, sizeof(servaddr));
  memset(&cliaddr, 0, sizeof(cliaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(PORT);

  if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    std::cerr << "bind failed" << std::endl;
    return 1;
  }

  std::cout << "server is running on port " << PORT << "..." << std::endl;

  while (true) {
    socklen_t len = sizeof(cliaddr);

    int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0,
                     (struct sockaddr *)&cliaddr, &len);
    buffer[n] = '\0';

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(cliaddr.sin_addr), client_ip, INET_ADDRSTRLEN);
    std::cout << "received from " << client_ip << ":" << ntohs(cliaddr.sin_port)
              << " - " << buffer << std::endl;

    sendto(sockfd, (const char *)buffer, strlen(buffer), 0,
           (const struct sockaddr *)&cliaddr, len);
  }

  close(sockfd);
  return 0;
}
