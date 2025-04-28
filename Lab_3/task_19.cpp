#include <iostream>
#include <pthread.h>
#include <unistd.h> // Для sleep
#include <string>
#include <list>     // Для связанного списка

// Структура для записи в списке (строка + мьютекс)
struct ListNode {
  std::string data;
  pthread_mutex_t mutex;

  ListNode(const std::string & str): data(str) {
      pthread_mutexattr_t mutex_attr;
      pthread_mutexattr_init( & mutex_attr);
      pthread_mutexattr_settype( & mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
      pthread_mutex_init( & mutex, & mutex_attr);
      pthread_mutexattr_destroy( & mutex_attr);
    }

    ~ListNode() {
      pthread_mutex_destroy( & mutex);
    }
};

// Глобальные переменные
std::list < ListNode > string_list; // Связанный список с мьютексами для каждой записи
pthread_mutex_t list_mutex; // Мьютекс для заголовка списка

// Функция для разделения строки на части по 80 символов
void split_string(const std::string & input) {
  size_t len = input.length();
  size_t pos = 0;

  while (pos < len) {
    size_t chunk_size = std::min < size_t > (80, len - pos);
    std::string chunk = input.substr(pos, chunk_size);
    pthread_mutex_lock( & list_mutex);
    string_list.push_front(ListNode(chunk));
    pthread_mutex_unlock( & list_mutex);
    pos += chunk_size;
  }
}

// Функция для вывода списка
void print_list() {
  pthread_mutex_lock( & list_mutex);
  if (string_list.empty()) {
    std::cout << "Список пуст\n";
  } else {
    std::cout << "Текущее состояние списка:\n";
    int index = 1;
    for (auto & node: string_list) {
      pthread_mutex_lock( & node.mutex);
      std::cout << index++ << ". " << node.data << "\n";
      pthread_mutex_unlock( & node.mutex);
    }
  }
  pthread_mutex_unlock( & list_mutex);
}

// Функция пузырьковой сортировки с мьютексами и задержкой
void bubble_sort(std::list < ListNode > & lst) {
  if (lst.empty()) return;

  bool swapped;
  do {
    swapped = false;
    auto it = lst.begin();
    auto next_it = std::next(it);

    while (next_it != lst.end()) {
      auto prev_it = it;
      if (it == lst.begin()) {
        prev_it = lst.end();
      } else {
        prev_it = std::prev(it);
      }

      // Захватываем мьютексы в порядке от начала к концу списка
      if (prev_it != lst.end()) {
        pthread_mutex_lock( & prev_it -> mutex);
      }
      pthread_mutex_lock( & it -> mutex);
      pthread_mutex_lock( & next_it -> mutex);

      // Сравниваем строки
      if (it -> data > next_it -> data) {
        // Меняем данные местами
        std::swap(it -> data, next_it -> data);
        swapped = true;

        // Задержка 1 секунда после перестановки
        std::cout << "Перестановка: " << it -> data << " и " << next_it -> data << "\n";
        sleep(1); // Задержка для наблюдения
      }

      // Освобождаем мьютексы
      pthread_mutex_unlock( & next_it -> mutex);
      pthread_mutex_unlock( & it -> mutex);
      if (prev_it != lst.end()) {
        pthread_mutex_unlock( & prev_it -> mutex);
      }

      ++it;
      ++next_it;
    }
  } while (swapped);
}

// Функция для дочернего потока (сортировка каждые 5 секунд)
void * sortThread(void * arg) {
  while (true) {
    sleep(5); // Ждем 5 секунд

    pthread_mutex_lock( & list_mutex);
    if (!string_list.empty()) {
      std::cout << "Дочерний поток: начинаю сортировку\n";
      bubble_sort(string_list);
      std::cout << "Дочерний поток: список отсортирован (пузырьковая сортировка)\n";
    }
    pthread_mutex_unlock( & list_mutex);
  }
  return nullptr;
}

int main() {
  pthread_t thread;

  // Инициализация мьютекса для заголовка списка
  pthread_mutexattr_t mutex_attr;
  pthread_mutexattr_init( & mutex_attr);
  pthread_mutexattr_settype( & mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
  pthread_mutex_init( & list_mutex, & mutex_attr);

  // Создаем дочерний поток для сортировки
  if (pthread_create( & thread, nullptr, sortThread, nullptr) != 0) {
    std::cerr << "Ошибка при создании потока!\n";
    return 1;
  }

  // Родительский поток: ввод строк
  std::string input;
  std::cout << "Введите строки (пустая строка для вывода списка, Ctrl+D для выхода):\n";
  while (std::getline(std::cin, input)) {
    if (input.empty()) {
      print_list();
    } else {
      split_string(input);
    }
    std::cout << "Введите следующую строку:\n";
  }

  // Ожидаем завершения дочернего потока
  pthread_cancel(thread);
  pthread_join(thread, nullptr);

  // Уничтожаем мьютекс и атрибуты
  pthread_mutex_destroy( & list_mutex);
  pthread_mutexattr_destroy( & mutex_attr);

  return 0;
}
