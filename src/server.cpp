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
#include <map>
#include <thread>
#include <mutex>

std::mutex cout_mutex;
std::atomic<int> active_connections(0);

// Utility functions
std::vector<std::string> split(const std::string &str, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

std::string trim(const std::string &str)
{
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");
    return (start == std::string::npos || end == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

std::map<std::string, std::string> parseHeader(std::istringstream &requestStream)
{
    std::map<std::string, std::string> headers;
    std::string line;
    while (std::getline(requestStream, line) && line != "\r" && !line.empty())
    {
        size_t pos = line.find(':');
        if (pos != std::string::npos)
        {
            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));
            headers[key] = value;
        }
    }
    return headers;
}

// Server setup functions
int createServerSocket()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        throw std::runtime_error("Failed to create server socket");
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        throw std::runtime_error("setsockopt failed");
    }

    return server_fd;
}

void bindServerSocket(int server_fd, int port)
{
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        throw std::runtime_error("Failed to bind to port " + std::to_string(port));
    }
}

void listenForConnections(int server_fd, int backlog)
{
    if (listen(server_fd, backlog) != 0)
    {
        throw std::runtime_error("listen failed");
    }
}

// Request handling functions
std::string handleRequest(const std::string &method, const std::string &uri, const std::map<std::string, std::string> &headers)
{
    std::string response_line = "HTTP/1.1 200 OK\r\n";
    std::string content_type = "Content-Type: text/plain\r\n";
    std::string content_length = "Content-Length: ";
    std::string content;

    if (method == "GET")
    {
        if (uri == "/")
        {
            return response_line + "\r\n";
        }
        else if (uri == "/user-agent")
        {
            content = headers.at("User-Agent");
        }
        else if (uri.substr(0, 6) == "/echo/")
        {
            content = uri.substr(6);
        }
        else
        {
            return "HTTP/1.1 404 Not Found\r\n\r\n";
        }
    }
    else
    {
        return "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
    }

    content_length += std::to_string(content.size()) + "\r\n\r\n";
    return response_line + content_type + content_length + content;
}

void handleClient(int client_fd)
{
    active_connections++;
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "New client connected. Active connections: " << active_connections << std::endl;

        char buffer[4096] = {0};
        int bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes_read < 0)
        {
            std::cerr << "Failed to receive data from client\n";
            close(client_fd);
            return;
        }
        std::istringstream requestStream(buffer);
        std::string requestLine;
        std::getline(requestStream, requestLine);
        std::vector<std::string> requestParts = split(requestLine, ' ');
        if (requestParts.size() < 3)
        {
            std::cerr << "Invalid request line\n";
            close(client_fd);
            return;
        }
        std::string method = requestParts[0];
        std::string uri = requestParts[1];
        std::map<std::string, std::string> headers = parseHeader(requestStream);
        std::string response = handleRequest(method, uri, headers);
        send(client_fd, response.c_str(), response.length(), 0);
        close(client_fd);
    }
    active_connections--;

    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Client disconnected. Active connections: " << active_connections << std::endl;
    }
}

// Main function
int main()
{
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    const int PORT = 4221;

    try
    {
        int server_fd = createServerSocket();
        bindServerSocket(server_fd, PORT);
        listenForConnections(server_fd, 5);
        std::cout << "Server is listening on port "<< PORT ;
        while (true)
        {
            struct sockaddr_in client_addr;
            int client_addr_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);

            if (client_fd < 0)
            {
                std::cerr << "Failed to accept client connection\n";
                continue;
            }

            std::cout << "Client connected\n";
            std::thread(handleClient, client_fd).detach();
        }

        close(server_fd);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}