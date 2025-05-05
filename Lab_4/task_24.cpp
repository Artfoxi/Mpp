#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

sem_t sem_a, sem_b, sem_c, sem_module; // Семафоры для деталей и модуля
volatile bool running = true; // Флаг для завершения потоков

void * produce_a(void * ) {
  while (running) {
    sleep(1); // Изготовление детали A
    std::cout << "Produced A\n";
    sem_post( & sem_a); // Увеличиваем счётчик деталей A
  }
  return nullptr;
}

void * produce_b(void * ) {
  while (running) {
    sleep(2); // Изготовление детали B
    std::cout << "Produced B\n";
    sem_post( & sem_b); // Увеличиваем счётчик деталей B
  }
  return nullptr;
}

void * produce_c(void * ) {
  while (running) {
    sleep(3); // Изготовление детали C
    std::cout << "Produced C\n";
    sem_post( & sem_c); // Увеличиваем счётчик деталей C
  }
  return nullptr;
}

void * produce_module(void * ) {
  while (running) {
    sem_wait( & sem_a); // Ожидаем деталь A
    sem_wait( & sem_b); // Ожидаем деталь B
    std::cout << "Assembled module (A + B)\n";
    sem_post( & sem_module); // Увеличиваем счётчик модулей
  }
  return nullptr;
}

int main() {
  // Инициализация семафоров
  sem_init( & sem_a, 0, 0);
  sem_init( & sem_b, 0, 0);
  sem_init( & sem_c, 0, 0);
  sem_init( & sem_module, 0, 0);

  // Создание потоков
  pthread_t thread_a, thread_b, thread_c, thread_module;
  pthread_create( & thread_a, nullptr, produce_a, nullptr);
  pthread_create( & thread_b, nullptr, produce_b, nullptr);
  pthread_create( & thread_c, nullptr, produce_c, nullptr);
  pthread_create( & thread_module, nullptr, produce_module, nullptr);

  // Сборка винтиков (основной поток)
  for (int i = 0; i < 5; ++i) { // Собираем 5 винтиков для демонстрации
    sem_wait( & sem_module); // Ожидаем модуль
    sem_wait( & sem_c); // Ожидаем деталь C
    std::cout << "Assembled widget " << i + 1 << " (Module + C)\n";
  }

  // Завершение работы
  running = false;
  sem_post( & sem_a);
  sem_post( & sem_b);
  sem_post( & sem_c);
  sem_post( & sem_module); // Разблокируем потоки
  pthread_join(thread_a, nullptr);
  pthread_join(thread_b, nullptr);
  pthread_join(thread_c, nullptr);
  pthread_join(thread_module, nullptr);

  // Уничтожение семафоров
  sem_destroy( & sem_a);
  sem_destroy( & sem_b);
  sem_destroy( & sem_c);
  sem_destroy( & sem_module);

  return 0;
}
