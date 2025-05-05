#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <queue>
#include <map>
#include <string>
#include <vector>

const int BUFFER_SIZE = 4096;
const int LISTEN_PORT = 8080;
const int MAX_CLIENTS_PER_THREAD = 50;

struct CacheEntry {
  std::string response;
};

struct Connection {
  int client_fd;
  int server_fd;
  std::string request;
  std::string response;
  bool headers_sent;
  bool is_client;
  std::string host;
  std::string path;
  Connection(): client_fd(-1), server_fd(-1), headers_sent(false), is_client(true) {}
};

struct WorkerData {
  std::queue < int > * client_queue;
  pthread_mutex_t * queue_mutex;
  pthread_cond_t * queue_cond;
  std::map < std::string, CacheEntry > * cache;
  pthread_mutex_t * cache_mutex;
  bool * running;
};

// Парсинг HTTP-запроса (идентичен заданию 31)
bool parse_request(const std::string & req, std::string & host, std::string & path) {
  size_t get_pos = req.find("GET ");
  if (get_pos == std::string::npos) return false;
  size_t http_pos = req.find(" HTTP/1.");
  if (http_pos == std::string::npos) return false;
  std::string url = req.substr(get_pos + 4, http_pos - get_pos - 4);
  if (url.substr(0, 7) == "http://") {
    url = url.substr(7);
  }
  size_t slash_pos = url.find('/');
  if (slash_pos == std::string::npos) {
    host = url;
    path = "/";
  } else {
    host = url.substr(0, slash_pos);
    path = url.substr(slash_pos);
  }
  return true;
}

// Подключение к серверу (идентичен заданию 31)
int connect_to_server(const std::string & host) {
  struct addrinfo hints, * res;
  memset( & hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(host.c_str(), "80", & hints, & res) != 0) {
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

// Рабочий поток
void * worker_thread(void * arg) {
  WorkerData * data = (WorkerData * ) arg;
  std::vector < Connection > connections(MAX_CLIENTS_PER_THREAD);
  std::vector < struct pollfd > fds;
  char buffer[BUFFER_SIZE];

  while ( * (data -> running)) {
    pthread_mutex_lock(data -> queue_mutex);
    while (data -> client_queue -> empty() && * (data -> running)) {
      pthread_cond_wait(data -> queue_cond, data -> queue_mutex);
    }
    if (! * (data -> running) && data -> client_queue -> empty()) {
      pthread_mutex_unlock(data -> queue_mutex);
      break;
    }

    int client_fd = data -> client_queue -> front();
    data -> client_queue -> pop();
    pthread_mutex_unlock(data -> queue_mutex);

    bool found = false;
    for (size_t i = 0; i < connections.size(); ++i) {
      if (connections[i].client_fd == -1) {
        connections[i].client_fd = client_fd;
        connections[i].is_client = true;
        fds.push_back({
          client_fd,
          POLLIN,
          0
        });
        found = true;
        break;
      }
    }
    if (!found) {
      std::cerr << "Max clients per thread reached" << std::endl;
      close(client_fd);
      continue;
    }

    if (poll(fds.data(), fds.size(), 1000) < 0) {
      std::cerr << "poll failed: " << strerror(errno) << std::endl;
      continue;
    }

    for (size_t i = 0; i < fds.size(); ++i) {
      if (fds[i].revents == 0) continue;

      int fd = fds[i].fd;
      size_t conn_idx = 0;
      bool is_client = false;
      for (size_t j = 0; j < connections.size(); ++j) {
        if (connections[j].client_fd == fd) {
          conn_idx = j;
          is_client = true;
          break;
        } else if (connections[j].server_fd == fd) {
          conn_idx = j;
          break;
        }
      }

      Connection & conn = connections[conn_idx];
      ssize_t n = read(fd, buffer, BUFFER_SIZE - 1);
      if (n <= 0) {
        if (is_client) {
          close(conn.client_fd);
          if (conn.server_fd >= 0) close(conn.server_fd);
          conn.client_fd = -1;
          conn.server_fd = -1;
        } else {
          close(conn.server_fd);
          conn.server_fd = -1;
        }
        fds.erase(fds.begin() + i);
        --i;
        continue;
      }
      buffer[n] = '\0';

      if (is_client) {
        conn.request += buffer;
        size_t header_end = conn.request.find("\r\n\r\n");
        if (header_end != std::string::npos) {
          if (!parse_request(conn.request, conn.host, conn.path)) {
            std::cerr << "Invalid request" << std::endl;
            close(conn.client_fd);
            conn.client_fd = -1;
            fds.erase(fds.begin() + i);
            --i;
            continue;
          }

          std::string cache_key = conn.host + conn.path;
          pthread_mutex_lock(data -> cache_mutex);
          if (data -> cache -> find(cache_key) != data -> cache -> end()) {
            conn.response = ( * data -> cache)[cache_key].response;
            conn.headers_sent = true;
            write(conn.client_fd, conn.response.c_str(), conn.response.size());
            close(conn.client_fd);
            conn.client_fd = -1;
            fds.erase(fds.begin() + i);
            pthread_mutex_unlock(data -> cache_mutex);
            --i;
            continue;
          }
          pthread_mutex_unlock(data -> cache_mutex);

          conn.server_fd = connect_to_server(conn.host);
          if (conn.server_fd < 0) {
            close(conn.client_fd);
            conn.client_fd = -1;
            fds.erase(fds.begin() + i);
            --i;
            continue;
          }
          fds.push_back({
            conn.server_fd,
            POLLIN,
            0
          });
          write(conn.server_fd, conn.request.c_str(), conn.request.size());
        }
      } else {
        conn.response += buffer;
        if (!conn.headers_sent) {
          size_t header_end = conn.response.find("\r\n\r\n");
          if (header_end != std::string::npos) {
            conn.headers_sent = true;
            write(conn.client_fd, conn.response.c_str(), conn.response.size());
            std::string cache_key = conn.host + conn.path;
            pthread_mutex_lock(data -> cache_mutex);
            ( * data -> cache)[cache_key].response = conn.response;
            pthread_mutex_unlock(data -> cache_mutex);
          }
        } else {
          write(conn.client_fd, buffer, n);
          std::string cache_key = conn.host + conn.path;
          pthread_mutex_lock(data -> cache_mutex);
          ( * data -> cache)[cache_key].response += buffer;
          pthread_mutex_unlock(data -> cache_mutex);
        }
      }
    }
  }

  for (auto & conn: connections) {
    if (conn.client_fd >= 0) close(conn.client_fd);
    if (conn.server_fd >= 0) close(conn.server_fd);
  }
  return nullptr;
}

int main(int argc, char * argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <num_threads>" << std::endl;
    return 1;
  }

  int num_threads = std::atoi(argv[1]);
  if (num_threads <= 0) {
    std::cerr << "Invalid number of threads" << std::endl;
    return 1;
  }

  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    std::cerr << "socket failed: " << strerror(errno) << std::endl;
    return 1;
  }

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(LISTEN_PORT);

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

  std::cout << "Proxy listening on port " << LISTEN_PORT << " with " << num_threads << " worker threads" << std::endl;

  std::queue < int > client_queue;
  pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
  std::map < std::string, CacheEntry > cache;
  pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;
  bool running = true;

  WorkerData data = {
    & client_queue,
    & queue_mutex,
    & queue_cond,
    & cache,
    & cache_mutex,
    & running
  };

  std::vector < pthread_t > threads(num_threads);
  for (int i = 0; i < num_threads; ++i) {
    if (pthread_create( & threads[i], nullptr, worker_thread, & data) != 0) {
      std::cerr << "pthread_create failed: " << strerror(errno) << std::endl;
      running = false;
      break;
    }
  }

  while (running) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, (struct sockaddr * ) & client_addr, & client_len);
    if (client_fd < 0) {
      std::cerr << "accept failed: " << strerror(errno) << std::endl;
      continue;
    }

    pthread_mutex_lock( & queue_mutex);
    client_queue.push(client_fd);
    pthread_cond_signal( & queue_cond);
    pthread_mutex_unlock( & queue_mutex);
  }

  running = false;
  pthread_mutex_lock( & queue_mutex);
  pthread_cond_broadcast( & queue_cond);
  pthread_mutex_unlock( & queue_mutex);

  for (int i = 0; i < num_threads; ++i) {
    pthread_join(threads[i], nullptr);
  }

  close(listen_fd);
  pthread_mutex_destroy( & queue_mutex);
  pthread_cond_destroy( & queue_cond);
  pthread_mutex_destroy( & cache_mutex);
  return 0;
}
