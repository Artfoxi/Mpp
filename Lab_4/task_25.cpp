#include <iostream>
#include <deque>
#include <string>
#include <pthread.h>
#include <semaphore.h>
#include <cstring>
#include <unistd.h>

class MessageQueue {
  private: std::deque < std::string > queue;
  sem_t sem_slots; // Счётчик свободных слотов
  sem_t sem_items; // Счётчик сообщений
  pthread_mutex_t mutex; // Мьютекс для доступа к очереди
  bool dropped; // Флаг сброса очереди

  public: MessageQueue(): dropped(false) {
      sem_init( & sem_slots, 0, 10); // Максимум 10 слотов
      sem_init( & sem_items, 0, 0); // Изначально 0 сообщений
      pthread_mutex_init( & mutex, nullptr);
    }

    ~MessageQueue() {
      mymsgdestroy();
    }

  int mymsgput(const char * msg) {
    if (dropped) return 0;

    // Обрезаем строку до 80 символов
    std::string str(msg);
    if (str.size() > 80) str = str.substr(0, 80);
    int len = str.size();

    sem_wait( & sem_slots); // Ждём свободный слот
    if (dropped) {
      sem_post( & sem_slots); // Восстанавливаем слот
      return 0;
    }

    pthread_mutex_lock( & mutex);
    queue.push_back(str);
    pthread_mutex_unlock( & mutex);

    sem_post( & sem_items); // Увеличиваем счётчик сообщений
    return len;
  }

  int mymsgget(char * buf, size_t bufsize) {
    if (dropped) return 0;

    sem_wait( & sem_items); // Ждём сообщение
    if (dropped) {
      sem_post( & sem_items); // Восстанавливаем счётчик
      return 0;
    }

    pthread_mutex_lock( & mutex);
    std::string msg = queue.front();
    queue.pop_front();
    pthread_mutex_unlock( & mutex);

    // Обрезаем до размера буфера
    size_t copy_len = std::min(msg.size(), bufsize - 1);
    strncpy(buf, msg.c_str(), copy_len);
    buf[copy_len] = '\0';

    sem_post( & sem_slots); // Освобождаем слот
    return copy_len;
  }

  void mymsgdrop() {
    pthread_mutex_lock( & mutex);
    dropped = true;
    queue.clear();
    pthread_mutex_unlock( & mutex);

    // Разблокируем все ожидающие потоки
    for (int i = 0; i < 10; ++i) {
      sem_post( & sem_slots);
      sem_post( & sem_items);
    }
  }

  void mymsgdestroy() {
    mymsgdrop(); // Гарантируем разблокировку
    pthread_mutex_destroy( & mutex);
    sem_destroy( & sem_slots);
    sem_destroy( & sem_items);
  }
};

// Потоки-производители
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

// Потоки-потребители
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
