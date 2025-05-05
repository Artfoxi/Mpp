#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <vector>
#include <string>

const int BUFFER_SIZE = 4096;
const int LINES_PER_SCREEN = 25;

// Парсинг URL (простая версия: http://host[:port]/path)
bool parse_url(const std::string & url, std::string & host, std::string & port, std::string & path) {
  if (url.substr(0, 7) != "http://") return false;
  std::string rest = url.substr(7);
  size_t slash_pos = rest.find('/');
  size_t colon_pos = rest.find(':');

  if (slash_pos == std::string::npos) {
    path = "/";
    if (colon_pos == std::string::npos) {
      host = rest;
      port = "80";
    } else {
      host = rest.substr(0, colon_pos);
      port = rest.substr(colon_pos + 1);
    }
  } else {
    path = rest.substr(slash_pos);
    if (colon_pos == std::string::npos || colon_pos > slash_pos) {
      host = rest.substr(0, slash_pos);
      port = "80";
    } else {
      host = rest.substr(0, colon_pos);
      port = rest.substr(colon_pos + 1, slash_pos - colon_pos - 1);
    }
  }
  return true;
}

// Подключение к серверу
int connect_to_server(const std::string & host,
  const std::string & port) {
  struct addrinfo hints, * res;
  memset( & hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(host.c_str(), port.c_str(), & hints, & res) != 0) {
    std::cerr << "getaddrinfo failed: " << gai_strerror(errno) << std::endl;
    return -1;
  }

  int sock = socket(res -> ai_family, res -> ai_socktype, res -> ai_protocol);
  if (sock < 0) {
    std::cerr << "socket failed: " << strerror(errno) << std::endl;
    freeaddrinfo(res);
    return -1;
  }

  if (connect(sock, res -> ai_addr, res -> ai_addrlen) < 0) {
    std::cerr << "connect failed: " << strerror(errno) << std::endl;
    close(sock);
    freeaddrinfo(res);
    return -1;
  }

  freeaddrinfo(res);
  return sock;
}

int main(int argc, char * argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <URL>" << std::endl;
    return 1;
  }

  std::string url = argv[1];
  std::string host, port, path;
  if (!parse_url(url, host, port, path)) {
    std::cerr << "Invalid URL" << std::endl;
    return 1;
  }

  // Подключение к серверу
  int sock = connect_to_server(host, port);
  if (sock < 0) return 1;

  // Отправка GET-запроса
  std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
  if (write(sock, request.c_str(), request.size()) < 0) {
    std::cerr << "write failed: " << strerror(errno) << std::endl;
    close(sock);
    return 1;
  }

  char buffer[BUFFER_SIZE];
  bool headers_done = false;
  std::string body;
  int line_count = 0;
  bool waiting_for_space = false;

  fd_set read_fds;
  struct timeval tv;

  while (true) {
    FD_ZERO( & read_fds);
    FD_SET(STDIN_FILENO, & read_fds);
    FD_SET(sock, & read_fds);
    int max_fd = std::max(STDIN_FILENO, sock) + 1;

    tv.tv_sec = 5;
    tv.tv_usec = 0;

    int ret = select(max_fd, & read_fds, nullptr, nullptr, & tv);
    if (ret < 0) {
      std::cerr << "select failed: " << strerror(errno) << std::endl;
      break;
    }

    if (FD_ISSET(sock, & read_fds)) {
      ssize_t n = read(sock, buffer, BUFFER_SIZE - 1);
      if (n <= 0) {
        if (n < 0) std::cerr << "read failed: " << strerror(errno) << std::endl;
        break;
      }
      buffer[n] = '\0';

      if (!headers_done) {
        body += buffer;
        size_t header_end = body.find("\r\n\r\n");
        if (header_end != std::string::npos) {
          body = body.substr(header_end + 4);
          headers_done = true;
        }
      } else {
        body += buffer;
      }

      if (headers_done && !waiting_for_space) {
        size_t pos = 0;
        while (pos < body.size()) {
          size_t next = body.find('\n', pos);
          if (next == std::string::npos) break;
          std::string line = body.substr(pos, next - pos + 1);
          std::cout << line;
          line_count++;
          pos = next + 1;

          if (line_count >= LINES_PER_SCREEN) {
            std::cout << "Press space to scroll down" << std::endl;
            waiting_for_space = true;
            break;
          }
        }
        if (pos < body.size()) {
          body = body.substr(pos);
        } else {
          body.clear();
        }
      }
    }

    if (FD_ISSET(STDIN_FILENO, & read_fds) && waiting_for_space) {
      char input;
      if (read(STDIN_FILENO, & input, 1) > 0 && input == ' ') {
        waiting_for_space = false;
        line_count = 0;
        size_t pos = 0;
        while (pos < body.size()) {
          size_t next = body.find('\n', pos);
          if (next == std::string::npos) break;
          std::string line = body.substr(pos, next - pos + 1);
          std::cout << line;
          line_count++;
          pos = next + 1;

          if (line_count >= LINES_PER_SCREEN) {
            std::cout << "Press space to scroll down" << std::endl;
            waiting_for_space = true;
            break;
          }
        }
        if (pos < body.size()) {
          body = body.substr(pos);
        } else {
          body.clear();
        }
      }
    }
  }

  close(sock);
  return 0;
}
