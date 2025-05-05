#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <vector>
#include <errno.h>

const int MAX_CONNECTIONS = 510; // Максимум соединений
const int BUFFER_SIZE = 4096; // Размер буфера

struct Connection {
  int client_fd; // Клиентский сокет
  int server_fd; // Серверный сокет
  bool active; // Флаг активности
  Connection(): client_fd(-1), server_fd(-1), active(false) {}
};

// Установка соединения с сервером N:P'
int connect_to_server(const char * node,
  const char * port) {
  struct addrinfo hints, * res;
  memset( & hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(node, port, & hints, & res) != 0) {
    std::cerr << "getaddrinfo failed: " << gai_strerror(errno) << std::endl;
    return -1;
  }

  int server_fd = socket(res -> ai_family, res -> ai_socktype, res -> ai_protocol);
  if (server_fd < 0) {
    std::cerr << "socket failed: " << strerror(errno) << std::endl;
    freeaddrinfo(res);
    return -1;
  }

  if (connect(server_fd, res -> ai_addr, res -> ai_addrlen) < 0) {
    std::cerr << "connect failed: " << strerror(errno) << std::endl;
    close(server_fd);
    freeaddrinfo(res);
    return -1;
  }

  freeaddrinfo(res);
  return server_fd;
}

// Закрытие соединения
void close_connection(Connection & conn) {
  if (conn.client_fd >= 0) {
    close(conn.client_fd);
    conn.client_fd = -1;
  }
  if (conn.server_fd >= 0) {
    close(conn.server_fd);
    conn.server_fd = -1;
  }
  conn.active = false;
}

int main(int argc, char * argv[]) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " <listen_port> <node> <node_port>" << std::endl;
    return 1;
  }

  int listen_port = std::atoi(argv[1]);
  const char * node = argv[2];
  const char * node_port = argv[3];

  // Создание слушающего сокета
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    std::cerr << "socket failed: " << strerror(errno) << std::endl;
    return 1;
  }

  // Настройка адреса
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(listen_port);

  // Разрешаем повторное использование порта
  int opt = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, & opt, sizeof(opt));

  if (bind(listen_fd, (struct sockaddr * ) & addr, sizeof(addr)) < 0) {
    std::cerr << "bind failed: " << strerror(errno) << std::endl;
    close(listen_fd);
    return 1;
  }

  if (listen(listen_fd, 10) < 0) {
    std::cerr << "listen failed: " << strerror(errno) << std::endl;
    close(listen_fd);
    return 1;
  }

  std::cout << "Server listening on port " << listen_port << ", forwarding to " << node << ":" << node_port << std::endl;

  // Массив соединений
  std::vector < Connection > connections(MAX_CONNECTIONS);
  std::vector < struct pollfd > fds;
  fds.push_back({
    listen_fd,
    POLLIN,
    0
  }); // Слушающий сокет

  char buffer[BUFFER_SIZE];

  while (true) {
    // Ожидание событий с poll
    if (poll(fds.data(), fds.size(), -1) < 0) {
      std::cerr << "poll failed: " << strerror(errno) << std::endl;
      break;
    }

    // Обработка событий
    for (size_t i = 0; i < fds.size(); ++i) {
      if (fds[i].revents == 0) continue;

      if (fds[i].fd == listen_fd) {
        // Новое клиентское соединение
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr * ) & client_addr, & client_len);
        if (client_fd < 0) {
          std::cerr << "accept failed: " << strerror(errno) << std::endl;
          continue;
        }

        // Подключение к серверу N
        int server_fd = connect_to_server(node, node_port);
        if (server_fd < 0) {
          close(client_fd);
          continue;
        }

        // Найти свободное место для соединения
        bool found = false;
        for (int j = 0; j < MAX_CONNECTIONS; ++j) {
          if (!connections[j].active) {
            connections[j].client_fd = client_fd;
            connections[j].server_fd = server_fd;
            connections[j].active = true;
            fds.push_back({
              client_fd,
              POLLIN,
              0
            });
            fds.push_back({
              server_fd,
              POLLIN,
              0
            });
            found = true;
            break;
          }
        }
        if (!found) {
          std::cerr << "Max connections reached" << std::endl;
          close(client_fd);
          close(server_fd);
        }
      } else {
        // Данные от клиента или сервера
        int fd = fds[i].fd;
        bool is_client = false;
        int conn_idx = -1;
        for (int j = 0; j < MAX_CONNECTIONS; ++j) {
          if (connections[j].active && (connections[j].client_fd == fd || connections[j].server_fd == fd)) {
            is_client = (fd == connections[j].client_fd);
            conn_idx = j;
            break;
          }
        }
        if (conn_idx == -1) continue;

        int src_fd = is_client ? connections[conn_idx].client_fd : connections[conn_idx].server_fd;
        int dst_fd = is_client ? connections[conn_idx].server_fd : connections[conn_idx].client_fd;

        ssize_t n = read(src_fd, buffer, BUFFER_SIZE);
        if (n <= 0) {
          // Соединение закрыто или ошибка
          close_connection(connections[conn_idx]);
          // Удаляем сокеты из fds
          fds.erase(fds.begin() + i);
          for (size_t j = 0; j < fds.size(); ++j) {
            if (fds[j].fd == dst_fd) {
              fds.erase(fds.begin() + j);
              break;
            }
          }
          continue;
        }

        // Пересылка данных
        ssize_t written = write(dst_fd, buffer, n);
        if (written != n) {
          std::cerr << "write failed: " << strerror(errno) << std::endl;
          close_connection(connections[conn_idx]);
          fds.erase(fds.begin() + i);
          for (size_t j = 0; j < fds.size(); ++j) {
            if (fds[j].fd == dst_fd) {
              fds.erase(fds.begin() + j);
              break;
            }
          }
        }
      }
    }
  }

  // Очистка
  close(listen_fd);
  for (auto & conn: connections) {
    close_connection(conn);
  }

  return 0;
}
