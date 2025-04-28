#include <iostream>
#include <semaphore.h>
#include <fcntl.h>    // Для O_CREAT
#include <sys/stat.h> // Для S_IRUSR, S_IWUSR
#include <unistd.h>   // Для fork, sleep, usleep
#include <sched.h>
#include <time.h>
#include <sys/wait.h> // Для wait

int main() {
  // Именованные семафоры
  sem_t * sem_parent = sem_open("/sem_parent", O_CREAT, S_IRUSR | S_IWUSR, 1); // Родительский процесс начинает
  sem_t * sem_child = sem_open("/sem_child", O_CREAT, S_IRUSR | S_IWUSR, 0); // Дочерний процесс ждет

  if (sem_parent == SEM_FAILED || sem_child == SEM_FAILED) {
    std::cerr << "Ошибка при создании семафоров!" << std::endl;
    return 1;
  }

  // Создаем дочерний процесс
  pid_t pid = fork();

  if (pid < 0) {
    std::cerr << "Ошибка при создании процесса!" << std::endl;
    return 1;
  }

  if (pid == 0) { // Дочерний процесс
    for (int i = 0; i < 10; ++i) {
      sem_wait(sem_child); // Ждем своей очереди

      // Выводим строку
      std::cout << "Новый процесс: строка " << i + 1 << std::endl;

      sem_post(sem_parent); // Разрешаем родительскому процессу продолжить

      usleep(100000); // 100 мс
      sched_yield();
    }

    // Закрываем семафоры в дочернем процессе
    sem_close(sem_parent);
    sem_close(sem_child);
  } else { // Родительский процесс
    for (int i = 0; i < 10; ++i) {
      sem_wait(sem_parent); // Ждем своей очереди

      // Выводим строку
      std::cout << "Родительский процесс: строка " << i + 1 << std::endl;

      sem_post(sem_child); // Разрешаем дочернему процессу продолжить

      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 100000000; // 100 мс
      nanosleep( & ts, nullptr);

      sched_yield();
    }

    // Ожидаем завершения дочернего процесса
    wait(nullptr);

    // Закрываем и удаляем семафоры
    sem_close(sem_parent);
    sem_close(sem_child);
    sem_unlink("/sem_parent");
    sem_unlink("/sem_child");
  }

  return 0;
}
