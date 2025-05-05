#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <string>
#include <vector>
#include <queue>

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

struct ThreadData {
  int sock;
  std::queue < std::string > * lines;
  pthread_mutex_t * mutex;
  pthread_cond_t * cond;
  bool * done;
  bool * headers_done;
  std::string * body;
};

// Сетевой поток: чтение данных из сокета
void * network_thread(void * arg) {
  ThreadData * data = (ThreadData * ) arg;
  char buffer[BUFFER_SIZE];

  while (true) {
    ssize_t n = read(data -> sock, buffer, BUFFER_SIZE - 1);
    if (n <= 0) {
      if (n < 0) std::cerr << "read failed: " << strerror(errno) << std::endl;
      pthread_mutex_lock(data -> mutex);
      *(data -> done) = true;
      pthread_cond_signal(data -> cond);
      pthread_mutex_unlock(data -> mutex);
      break;
    }
    buffer[n] = '\0';

    pthread_mutex_lock(data -> mutex);
    if (! * (data -> headers_done)) {
      *(data -> body) += buffer;
      size_t header_end = data -> body -> find("\r\n\r\n");
      if (header_end != std::string::npos) {
        *(data -> body) = data -> body -> substr(header_end + 4);
        *(data -> headers_done) = true;
      }
    } else {
      *(data -> body) += buffer;
    }

    if ( * (data -> headers_done)) {
      size_t pos = 0;
      while (pos < data -> body -> size()) {
        size_t next = data -> body -> find('\n', pos);
        if (next == std::string::npos) break;
        data -> lines -> push(data -> body -> substr(pos, next - pos + 1));
        pos = next + 1;
      }
      if (pos < data -> body -> size()) {
        *(data -> body) = data -> body -> substr(pos);
      } else {
        data -> body -> clear();
      }
      pthread_cond_signal(data -> cond);
    }
    pthread_mutex_unlock(data -> mutex);
  }
  return nullptr;
}

// Пользовательский поток: вывод данных и взаимодействие
void * user_thread(void * arg) {
  ThreadData * data = (ThreadData * ) arg;
  int line_count = 0;
  bool waiting_for_space = false;

  while (true) {
    pthread_mutex_lock(data -> mutex);
    while (data -> lines -> empty() && ! * (data -> done)) {
      pthread_cond_wait(data -> cond, data -> mutex);
    }
    if (data -> lines -> empty() && * (data -> done)) {
      pthread_mutex_unlock(data -> mutex);
      break;
    }

    if (!waiting_for_space) {
      while (!data -> lines -> empty() && line_count < LINES_PER_SCREEN) {
        std::cout << data -> lines -> front();
        data -> lines -> pop();
        line_count++;
      }
      if (line_count >= LINES_PER_SCREEN) {
        std::cout << "Press space to scroll down" << std::endl;
        waiting_for_space = true;
      }
    }
    pthread_mutex_unlock(data -> mutex);

    if (waiting_for_space) {
      char input;
      if (read(STDIN_FILENO, & input, 1) > 0 && input == ' ') {
        pthread_mutex_lock(data -> mutex);
        waiting_for_space = false;
        line_count = 0;
        pthread_mutex_unlock(data -> mutex);
      }
    }
  }
  return nullptr;
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

  // Подготовка данных для потоков
  std::queue < std::string > lines;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  bool done = false;
  bool headers_done = false;
  std::string body;

  pthread_mutex_init( & mutex, nullptr);
  pthread_cond_init( & cond, nullptr);

  ThreadData data = {
    sock,
    & lines,
    & mutex,
    & cond,
    & done,
    & headers_done,
    & body
  };

  // Создание потоков
  pthread_t network, user;
  pthread_create( & network, nullptr, network_thread, & data);
  pthread_create( & user, nullptr, user_thread, & data);

  // Ожидание завершения
  pthread_join(network, nullptr);
  pthread_join(user, nullptr);

  // Очистка
  pthread_mutex_destroy( & mutex);
  pthread_cond_destroy( & cond);
  close(sock);
  return 0;
}
