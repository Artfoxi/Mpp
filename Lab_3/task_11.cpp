#include <iostream>
#include <pthread.h>
#include <unistd.h> // Для sleep и usleep
#include <sched.h>  // Для sched_yield
#include <time.h>   // Для nanosleep

// Глобальные переменные
pthread_mutex_t mutex; // Мьютекс для защиты std::cout
pthread_cond_t cond; // Условная переменная для синхронизации
bool parent_turn = true; // Флаг, указывающий, чья очередь (true - родительский поток)

// Функция для дочернего потока
void * threadFunction(void * arg) {
  for (int i = 0; i < 10; ++i) {
    pthread_mutex_lock( & mutex);

    // Ждем, пока не наступит очередь дочернего потока
    while (parent_turn) {
      pthread_cond_wait( & cond, & mutex);
    }

    // Выводим строку
    std::cout << "Новый поток: строка " << i + 1 << std::endl;

    // Переключаем очередь на родительский поток
    parent_turn = true;
    pthread_cond_signal( & cond); // Сигнализируем родительскому потоку

    pthread_mutex_unlock( & mutex);

    // Используем usleep и sleep для наглядности
    usleep(100000); // 100 мс
    sleep(1); // 1 секунда
    sched_yield();
  }
  return nullptr;
}

int main() {
  pthread_t thread;

  // Инициализация мьютекса и условной переменной
  pthread_mutexattr_t mutex_attr;
  pthread_mutexattr_init( & mutex_attr);
  // Устанавливаем тип мьютекса PTHREAD_MUTEX_ERRORCHECK, как рекомендовано
  pthread_mutexattr_settype( & mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
  pthread_mutex_init( & mutex, & mutex_attr);
  pthread_cond_init( & cond, nullptr);

  // Создаем поток
  if (pthread_create( & thread, nullptr, threadFunction, nullptr) != 0) {
    std::cerr << "Ошибка при создании потока!" << std::endl;
    return 1;
  }

  // Родительский поток
  for (int i = 0; i < 10; ++i) {
    pthread_mutex_lock( & mutex);

    // Ждем, пока не наступит очередь родительского потока
    while (!parent_turn) {
      pthread_cond_wait( & cond, & mutex);
    }

    // Выводим строку
    std::cout << "Родительский поток: строка " << i + 1 << std::endl;

    // Переключаем очередь на дочерний поток
    parent_turn = false;
    pthread_cond_signal( & cond); // Сигнализируем дочернему потоку

    pthread_mutex_unlock( & mutex);

    // Используем nanosleep и sleep для наглядности
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 100000000; // 100 мс
    nanosleep( & ts, nullptr);
    sleep(1); // 1 секунда
    sched_yield();
  }

  // Ожидаем завершения дочернего потока
  pthread_join(thread, nullptr);

  // Уничтожаем мьютекс, условную переменную и атрибуты
  pthread_mutex_destroy( & mutex);
  pthread_cond_destroy( & cond);
  pthread_mutexattr_destroy( & mutex_attr);

  return 0;
}
