#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
  int sockfd;
  char buffer[BUFFER_SIZE];
  struct sockaddr_in servaddr;

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    std::cerr << "socket creation failed" << std::endl;
    return 1;
  }

  memset(&servaddr, 0, sizeof(servaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(PORT);
  servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

  std::string message;
  std::cout << "enter message to send: ";
  std::getline(std::cin, message);

  sendto(sockfd, message.c_str(), message.length(), 0,
         (const struct sockaddr *)&servaddr, sizeof(servaddr));
  std::cout << "message sent." << std::endl;

  socklen_t len = sizeof(servaddr);
  int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0,
                   (struct sockaddr *)&servaddr, &len);
  buffer[n] = '\0';

  std::cout << "server echoed: " << buffer << std::endl;

  close(sockfd);
  return 0;
}
