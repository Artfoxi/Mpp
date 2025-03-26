#include <iomanip>
#include <iostream>
#include <pthread.h>
#include <cstdlib>
#define num_steps 2000000000

// Структура для передачи аргументов в поток и хранения результата
struct ThreadArg {
  long thread_id;
  long num_threads;
  double partial_sum;
};

// Функция, выполняемая каждым потоком
void * calculate_pi(void * arg) {
  ThreadArg * thread_arg = (ThreadArg * ) arg;
  long thread_id = thread_arg -> thread_id;
  long num_threads = thread_arg -> num_threads;
  double partial_sum = 0.0;

  // Каждый поток вычисляет свою часть ряда Лейбница
  for (long i = thread_id; i < num_steps; i += num_threads) {
    partial_sum += 1.0 / (i * 4.0 + 1.0);
    partial_sum -= 1.0 / (i * 4.0 + 3.0);
  }

  // Сохраняем частичную сумму в структуре
  thread_arg -> partial_sum = partial_sum;

  // Завершаем поток, возвращая указатель на структуру
  pthread_exit((void * ) thread_arg);
}

int main(int argc, char * argv[]) {
  // Проверяем, передан ли аргумент с количеством потоков
  if (argc != 2) {
    std::cerr << "Использование: " << argv[0] << " <число_потоков>\n";
    return EXIT_FAILURE;
  }

  // Получаем количество потоков из аргумента командной строки
  long num_threads = std::atol(argv[1]);
  if (num_threads <= 0) {
    std::cerr << "Число потоков должно быть положительным\n";
    return EXIT_FAILURE;
  }

  // Создаём массив потоков и аргументов для них
  pthread_t * threads = new pthread_t[num_threads];
  ThreadArg * thread_args = new ThreadArg[num_threads];

  // Инициализируем аргументы для каждого потока
  for (long i = 0; i < num_threads; ++i) {
    thread_args[i].thread_id = i;
    thread_args[i].num_threads = num_threads;
    thread_args[i].partial_sum = 0.0;

    // Создаём поток
    if (pthread_create( & threads[i], nullptr, calculate_pi, (void * ) & thread_args[i]) != 0) {
      std::cerr << "Ошибка создания потока " << i << "\n";
      delete[] threads;
      delete[] thread_args;
      return EXIT_FAILURE;
    }
  }

  // Суммируем результаты всех потоков
  double pi = 0.0;
  for (long i = 0; i < num_threads; ++i) {
    void * thread_result;
    if (pthread_join(threads[i], & thread_result) != 0) {
      std::cerr << "Ошибка при объединении потока " << i << "\n";
      delete[] threads;
      delete[] thread_args;
      return EXIT_FAILURE;
    }

    // Добавляем частичную сумму к общему результату
    ThreadArg * result = (ThreadArg * ) thread_result;
    pi += result -> partial_sum;
  }

  // Умножаем на 4 для получения числа Пи
  pi *= 4.0;

  // Выводим результат
  std::cout << "pi done - " << std::fixed << std::setprecision(15) << pi << "\n";

  // Освобождаем память
  delete[] threads;
  delete[] thread_args;

  return EXIT_SUCCESS;
}
