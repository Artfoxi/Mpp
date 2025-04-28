#include <iostream>
#include <pthread.h>
#include <semaphore.h> // Для семафоров
#include <unistd.h>
#include <sched.h>
#include <time.h>

// Глобальные семафоры
sem_t sem_parent; // Семафор для родительского потока
sem_t sem_child; // Семафор для дочернего потока

// Функция для дочернего потока
void * threadFunction(void * arg) {
  for (int i = 0; i < 10; ++i) {
    // Ждем, пока родительский поток не завершит свою итерацию
    sem_wait( & sem_child);

    // Выводим строку
    std::cout << "Новый поток: строка " << i + 1 << std::endl;

    // Разрешаем родительскому потоку продолжить
    sem_post( & sem_parent);

    usleep(100000); // 100 мс для наглядности
    sched_yield();
  }
  return nullptr;
}

int main() {
  pthread_t thread;

  // Инициализация семафоров
  sem_init( & sem_parent, 0, 1); // Родительский поток начинает (1)
  sem_init( & sem_child, 0, 0); // Дочерний поток ждет (0)

  // Создаем поток
  if (pthread_create( & thread, nullptr, threadFunction, nullptr) != 0) {
    std::cerr << "Ошибка при создании потока!" << std::endl;
    return 1;
  }

  // Родительский поток
  for (int i = 0; i < 10; ++i) {
    // Ждем своей очереди
    sem_wait( & sem_parent);

    // Выводим строку
    std::cout << "Родительский поток: строка " << i + 1 << std::endl;

    // Разрешаем дочернему потоку продолжить
    sem_post( & sem_child);

    // Задержка с помощью nanosleep
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 100000000; // 100 мс
    nanosleep( & ts, nullptr);

    sched_yield();
  }

  // Ожидаем завершения дочернего потока
  pthread_join(thread, nullptr);

  // Уничтожаем семафоры
  sem_destroy( & sem_parent);
  sem_destroy( & sem_child);

  return 0;
}
