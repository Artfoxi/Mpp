#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <map>
#include <string>

const int BUFFER_SIZE = 4096;
const int LISTEN_PORT = 8080;

struct CacheEntry {
  std::string response;
};

std::map < std::string, CacheEntry > cache;
pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

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

// Обработка клиента в отдельном потоке
void * handle_client(void * arg) {
  int client_fd = * (int * ) arg;
  delete(int * ) arg;

  char buffer[BUFFER_SIZE];
  std::string request;

  while (true) {
    ssize_t n = read(client_fd, buffer, BUFFER_SIZE - 1);
    if (n <= 0) {
      close(client_fd);
      return nullptr;
    }
    buffer[n] = '\0';
    request += buffer;
    if (request.find("\r\n\r\n") != std::string::npos) break;
  }

  std::string host, path;
  if (!parse_request(request, host, path)) {
    std::cerr << "Invalid request" << std::endl;
    close(client_fd);
    return nullptr;
  }

  std::string cache_key = host + path;
  std::string response;

  pthread_mutex_lock( & cache_mutex);
  if (cache.find(cache_key) != cache.end()) {
    response = cache[cache_key].response;
    pthread_mutex_unlock( & cache_mutex);
    write(client_fd, response.c_str(), response.size());
    close(client_fd);
    return nullptr;
  }
  pthread_mutex_unlock( & cache_mutex);

  int server_fd = connect_to_server(host);
  if (server_fd < 0) {
    close(client_fd);
    return nullptr;
  }

  write(server_fd, request.c_str(), request.size());

  while (true) {
    ssize_t n = read(server_fd, buffer, BUFFER_SIZE);
    if (n <= 0) break;
    response += std::string(buffer, n);
    write(client_fd, buffer, n);
  }

  pthread_mutex_lock( & cache_mutex);
  cache[cache_key].response = response;
  pthread_mutex_unlock( & cache_mutex);

  close(server_fd);
  close(client_fd);
  return nullptr;
}

int main(int argc, char * argv[]) {
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

  std::cout << "Proxy listening on port " << LISTEN_PORT << std::endl;

  while (true) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, (struct sockaddr * ) & client_addr, & client_len);
    if (client_fd < 0) {
      std::cerr << "accept failed: " << strerror(errno) << std::endl;
      continue;
    }

    pthread_t thread;
    int * client_fd_ptr = new int(client_fd);
    if (pthread_create( & thread, nullptr, handle_client, client_fd_ptr) != 0) {
      std::cerr << "pthread_create failed: " << strerror(errno) << std::endl;
      close(client_fd);
      delete client_fd_ptr;
      continue;
    }
    pthread_detach(thread);
  }

  close(listen_fd);
  pthread_mutex_destroy( & cache_mutex);
  return 0;
}
