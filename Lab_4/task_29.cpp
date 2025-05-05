#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <aio.h>
#include <fcntl.h>
#include <string>
#include <vector>

const int BUFFER_SIZE = 4096;
const int LINES_PER_SCREEN = 25;

// Парсинг URL (идентичен заданию 28)
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

// Подключение к серверу (идентичен заданию 28)
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

  // Установка неблокирующего режима для AIO
  fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_ASYNC);
  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_ASYNC);

  // Отправка GET-запроса
  std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
  if (write(sock, request.c_str(), request.size()) < 0) {
    std::cerr << "write failed: " << strerror(errno) << std::endl;
    close(sock);
    return 1;
  }

  char sock_buffer[BUFFER_SIZE];
  char stdin_buffer[1];
  bool headers_done = false;
  std::string body;
  int line_count = 0;
  bool waiting_for_space = false;

  // Настройка AIO для сокета
  struct aiocb sock_cb;
  memset( & sock_cb, 0, sizeof(sock_cb));
  sock_cb.aio_fildes = sock;
  sock_cb.aio_buf = sock_buffer;
  sock_cb.aio_nbytes = BUFFER_SIZE - 1;
  sock_cb.aio_offset = 0;

  // Настройка AIO для stdin
  struct aiocb stdin_cb;
  memset( & stdin_cb, 0, sizeof(stdin_cb));
  stdin_cb.aio_fildes = STDIN_FILENO;
  stdin_cb.aio_buf = stdin_buffer;
  stdin_cb.aio_nbytes = 1;
  stdin_cb.aio_offset = 0;

  // Инициализация асинхронного чтения
  if (!waiting_for_space) {
    aio_read( & sock_cb);
  }
  if (waiting_for_space) {
    aio_read( & stdin_cb);
  }

  while (true) {
    struct aiocb * cblist[] = {
      & sock_cb,
      & stdin_cb
    };
    if (aio_suspend(cblist, 2, nullptr) < 0) {
      std::cerr << "aio_suspend failed: " << strerror(errno) << std::endl;
      break;
    }

    // Проверка завершения чтения из сокета
    if (aio_error( & sock_cb) == 0) {
      ssize_t n = aio_return( & sock_cb);
      if (n <= 0) {
        if (n < 0) std::cerr << "aio_read failed: " << strerror(errno) << std::endl;
        break;
      }
      sock_buffer[n] = '\0';

      if (!headers_done) {
        body += sock_buffer;
        size_t header_end = body.find("\r\n\r\n");
        if (header_end != std::string::npos) {
          body = body.substr(header_end + 4);
          headers_done = true;
        }
      } else {
        body += sock_buffer;
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
            aio_read( & stdin_cb);
            break;
          }
        }
        if (pos < body.size()) {
          body = body.substr(pos);
        } else {
          body.clear();
        }
      }

      if (!waiting_for_space) {
        aio_read( & sock_cb);
      }
    }

    // Проверка завершения чтения из stdin
    if (aio_error( & stdin_cb) == 0 && waiting_for_space) {
      ssize_t n = aio_return( & stdin_cb);
      if (n > 0 && stdin_buffer[0] == ' ') {
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
            aio_read( & stdin_cb);
            break;
          }
        }
        if (pos < body.size()) {
          body = body.substr(pos);
        } else {
          body.clear();
        }
        if (!waiting_for_space) {
          aio_read( & sock_cb);
        }
      }
    }
  }

  close(sock);
  return 0;
}
