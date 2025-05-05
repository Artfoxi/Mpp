#include <iostream>
#include <deque>
#include <string>
#include <pthread.h>
#include <cstring>
#include <unistd.h>

class MessageQueue {
  private: std::deque < std::string > queue;
  pthread_mutex_t mutex;
  pthread_cond_t cond_slots; // Для ожидания свободных слотов
  pthread_cond_t cond_items; // Для ожидания сообщений
  bool dropped;

  public: MessageQueue(): dropped(false) {
      pthread_mutex_init( & mutex, nullptr);
      pthread_cond_init( & cond_slots, nullptr);
      pthread_cond_init( & cond_items, nullptr);
    }

    ~MessageQueue() {
      mymsgdestroy();
    }

  int mymsgput(const char * msg) {
    pthread_mutex_lock( & mutex);
    if (dropped) {
      pthread_mutex_unlock( & mutex);
      return 0;
    }

    // Обрезаем строку до 80 символов
    std::string str(msg);
    if (str.size() > 80) str = str.substr(0, 80);
    int len = str.size();

    // Ждём, пока в очереди < 10 элементов
    while (queue.size() >= 10 && !dropped) {
      pthread_cond_wait( & cond_slots, & mutex);
    }
    if (dropped) {
      pthread_mutex_unlock( & mutex);
      return 0;
    }

    queue.push_back(str);
    pthread_cond_signal( & cond_items); // Сигнализируем о новом сообщении
    pthread_mutex_unlock( & mutex);
    return len;
  }

  int mymsgget(char * buf, size_t bufsize) {
    pthread_mutex_lock( & mutex);
    if (dropped) {
      pthread_mutex_unlock( & mutex);
      return 0;
    }

    // Ждём, пока очередь не пуста
    while (queue.empty() && !dropped) {
      pthread_cond_wait( & cond_items, & mutex);
    }
    if (dropped) {
      pthread_mutex_unlock( & mutex);
      return 0;
    }

    std::string msg = queue.front();
    queue.pop_front();

    // Обрезаем до размера буфера
    size_t copy_len = std::min(msg.size(), bufsize - 1);
    strncpy(buf, msg.c_str(), copy_len);
    buf[copy_len] = '\0';

    pthread_cond_signal( & cond_slots); // Сигнализируем о свободном слоте
    pthread_mutex_unlock( & mutex);
    return copy_len;
  }

  void mymsgdrop() {
    pthread_mutex_lock( & mutex);
    dropped = true;
    queue.clear();
    pthread_cond_broadcast( & cond_slots); // Разблокируем put
    pthread_cond_broadcast( & cond_items); // Разблокируем get
    pthread_mutex_unlock( & mutex);
  }

  void mymsgdestroy() {
    mymsgdrop();
    pthread_mutex_destroy( & mutex);
    pthread_cond_destroy( & cond_slots);
    pthread_cond_destroy( & cond_items);
  }
};

// Потоки-производители и потребители (идентичны заданию 25)
void * producer(void * arg) {
  MessageQueue * q = (MessageQueue * ) arg;
  for (int i = 1; i <= 5; ++i) {
    std::string msg = "Message " + std::to_string(i);
    int len = q -> mymsgput(msg.c_str());
    if (len == 0) break;
    std::cout << "Producer sent: " << msg << " (" << len << " chars)\n";
    sleep(1);
  }
  return nullptr;
}

void * consumer(void * arg) {
  MessageQueue * q = (MessageQueue * ) arg;
  char buf[100];
  while (true) {
    int len = q -> mymsgget(buf, sizeof(buf));
    if (len == 0) break;
    std::cout << "Consumer received: " << buf << " (" << len << " chars)\n";
    sleep(2);
  }
  return nullptr;
}

int main() {
  MessageQueue q;

  // Создание потоков
  pthread_t prod1, prod2, cons1, cons2;
  pthread_create( & prod1, nullptr, producer, & q);
  pthread_create( & prod2, nullptr, producer, & q);
  pthread_create( & cons1, nullptr, consumer, & q);
  pthread_create( & cons2, nullptr, consumer, & q);

  // Ждём 10 секунд, затем сбрасываем очередь
  sleep(10);
  std::cout << "Dropping queue\n";
  q.mymsgdrop();

  // Ждём завершения потоков
  pthread_join(prod1, nullptr);
  pthread_join(prod2, nullptr);
  pthread_join(cons1, nullptr);
  pthread_join(cons2, nullptr);

  return 0;
}
