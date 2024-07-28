#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sstream>
#include <vector>

std::vector<std::string> split(const std::string &str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  // You can use print statements as follows for debugging, they'll be visible when running tests.
  std::cout << "Logs from your program will appear here!\n";

  // Uncomment this block to pass the first stage
  
  int server_fd = socket(AF_INET/*IPv4*/, SOCK_STREAM/*TCP*/, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  
  std::cout << "Waiting for a client to connect...\n";
  
  int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
  std::cout << "Client connected\n";
  
  
  // Receive data from the client
  char buffer[4096] = {0};
  int bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
  if (bytes_read < 0) {
      perror("recv");
      close(client_fd);
      close(server_fd);
      exit(EXIT_FAILURE);
  }

  std::string request(buffer);
  std::cout << "Received request:\n" << request << std::endl;
  
  std::istringstream requestStream(request);
  std::string requestLine;
  std::getline(requestStream, requestLine);

  std::vector<std::string> requestParts = split(requestLine, ' ');
  if (requestParts.size() >= 3) {
      std::string method = requestParts[0];
      std::string uri = requestParts[1];
      std::string version = requestParts[2];

      // std::cout << "Method: " << method << std::endl;
      // std::cout << "URI: " << uri << std::endl;
      // std::cout << "Version: " << version << std::endl;

      if(method == "GET"){
        std::string response_header = "HTTP/1.1 200 OK\r\n";
        if(uri == "/")
        {
          response_header += "\r\n";
          send(client_fd, response_header.c_str(), response_header.length(), 0);
        }
        std::vector<std::string> path = split(uri,'/');
        if(path[1] == "echo"){
          std::string content = path[2];
          std::string content_type = "Content-Type: text/plain\r\n";
          std::string content_length = "Content-Length: "+ std::to_string(content.size()) +"\r\n\r\n";
          std::string response = response_header +
                                  content_type +
                                  content_length +
                                  content;
          std::cout << "\nRESPONSE\n" << response << std::endl;
          send(client_fd, response.c_str(), response.length(), 0);
        }
        else{
          response_header = "HTTP/1.1 404 Not Found\r\n\r\n";
          send(client_fd, response_header.c_str(), response_header.length(), 0);
        }
    } else {
        std::cerr << "Invalid request line" << std::endl;
    }
    
  }
close(client_fd);
close(server_fd);
return 0;
}
