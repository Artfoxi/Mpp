#include <iomanip>
#include <iostream>
#include <pthread.h>
#include <cstdlib>
#include <csignal>
#include <unistd.h>

// Глобальная переменная-флаг для обработки SIGINT
volatile sig_atomic_t stop_flag = 0;

// Структура для передачи аргументов в поток и хранения результата
struct ThreadArg {
  long thread_id;
  long num_threads;
  double partial_sum;
  long iterations_done; // Количество выполненных итераций
};

// Обработчик сигнала SIGINT
void signal_handler(int signum) {
  stop_flag = 1;
}

// Функция, выполняемая каждым потоком
void * calculate_pi(void * arg) {
  ThreadArg * thread_arg = (ThreadArg * ) arg;
  long thread_id = thread_arg -> thread_id;
  long num_threads = thread_arg -> num_threads;
  double partial_sum = 0.0;
  long i = thread_id;
  long iterations_done = 0;

  // Бесконечный цикл для вычисления ряда Лейбница
  while (true) {
    // Вычисляем 1,000,000 итераций перед проверкой флага
    for (long j = 0; j < 1000000; ++j) {
      partial_sum += 1.0 / (i * 4.0 + 1.0);
      partial_sum -= 1.0 / (i * 4.0 + 3.0);
      i += num_threads;
      ++iterations_done;
    }

    // Проверяем флаг остановки
    if (stop_flag) {
      break;
    }
  }

  // Сохраняем результаты
  thread_arg -> partial_sum = partial_sum;
  thread_arg -> iterations_done = iterations_done;

  // Завершаем поток
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

  // Устанавливаем обработчик SIGINT
  signal(SIGINT, signal_handler);

  // Создаём массив потоков и аргументов для них
  pthread_t * threads = new pthread_t[num_threads];
  ThreadArg * thread_args = new ThreadArg[num_threads];

  // Инициализируем аргументы для каждого потока
  for (long i = 0; i < num_threads; ++i) {
    thread_args[i].thread_id = i;
    thread_args[i].num_threads = num_threads;
    thread_args[i].partial_sum = 0.0;
    thread_args[i].iterations_done = 0;

    // Создаём поток
    if (pthread_create( & threads[i], nullptr, calculate_pi, (void * ) & thread_args[i]) != 0) {
      std::cerr << "Ошибка создания потока " << i << "\n";
      delete[] threads;
      delete[] thread_args;
      return EXIT_FAILURE;
    }
  }

  // Ожидаем завершения потоков после получения SIGINT
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
    std::cout << "Поток " << i << " выполнил " << result -> iterations_done << " итераций\n";
    thread_args[i].partial_sum = result -> partial_sum;
    thread_args[i].iterations_done = result -> iterations_done;
  }

  // Находим минимальное количество итераций, выполненных всеми потоками
  long min_iterations = thread_args[0].iterations_done;
  for (long i = 1; i < num_threads; ++i) {
    if (thread_args[i].iterations_done < min_iterations) {
      min_iterations = thread_args[i].iterations_done;
    }
  }

  // Корректируем частичные суммы, чтобы учесть только min_iterations
  double pi = 0.0;
  for (long i = 0; i < num_threads; ++i) {
    long excess_iterations = thread_args[i].iterations_done - min_iterations;
    double excess_sum = 0.0;

    // Вычисляем сумму лишних итераций, чтобы вычесть их
    long start_index = i + min_iterations * num_threads;
    for (long j = 0; j < excess_iterations; ++j) {
      long idx = start_index + j * num_threads;
      excess_sum += 1.0 / (idx * 4.0 + 1.0);
      excess_sum -= 1.0 / (idx * 4.0 + 3.0);
    }

    // Корректируем частичную сумму
    pi += thread_args[i].partial_sum - excess_sum;
  }

  // Умножаем на 4 для получения числа Пи
  pi *= 4.0;

  // Выводим результат
  std::cout << "Число итераций (по минимуму): " << min_iterations * num_threads << "\n";
  std::cout << "pi done - " << std::fixed << std::setprecision(15) << pi << "\n";

  // Освобождаем память
  delete[] threads;
  delete[] thread_args;

  return EXIT_SUCCESS;
}
