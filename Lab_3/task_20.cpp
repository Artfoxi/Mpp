#include <iostream>
#include <pthread.h>
#include <unistd.h> // Для sleep
#include <string>
#include <list> // Для связанного списка

// Структура для записи в списке (строка + read-write lock)
struct ListNode {
  std::string data;
  pthread_rwlock_t rwlock;

  ListNode(const std::string & str): data(str) {
      pthread_rwlock_init( & rwlock, nullptr);
    }

    ~ListNode() {
      pthread_rwlock_destroy( & rwlock);
    }
};

// Глобальные переменные
std::list < ListNode > string_list; // Связанный список с rwlock для каждой записи
pthread_rwlock_t list_rwlock; // Read-write lock для заголовка списка

// Функция для разделения строки на части по 80 символов
void split_string(const std::string & input) {
  size_t len = input.length();
  size_t pos = 0;

  while (pos < len) {
    size_t chunk_size = std::min < size_t > (80, len - pos);
    std::string chunk = input.substr(pos, chunk_size);
    pthread_rwlock_wrlock( & list_rwlock); // Блокируем запись
    string_list.push_front(ListNode(chunk));
    pthread_rwlock_unlock( & list_rwlock);
    pos += chunk_size;
  }
}

// Функция для вывода списка (чтение)
void print_list() {
  pthread_rwlock_rdlock( & list_rwlock); // Блокируем только для чтения
  if (string_list.empty()) {
    std::cout << "Список пуст\n";
  } else {
    std::cout << "Текущее состояние списка:\n";
    int index = 1;
    for (auto & node: string_list) {
      pthread_rwlock_rdlock( & node.rwlock); // Чтение данных узла
      std::cout << index++ << ". " << node.data << "\n";
      pthread_rwlock_unlock( & node.rwlock);
    }
  }
  pthread_rwlock_unlock( & list_rwlock);
}

// Функция пузырьковой сортировки с rwlock
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

      // Захватываем rwlock в режиме записи (нужна эксклюзивная блокировка)
      if (prev_it != lst.end()) {
        pthread_rwlock_wrlock( & prev_it -> rwlock);
      }
      pthread_rwlock_wrlock( & it -> rwlock);
      pthread_rwlock_wrlock( & next_it -> rwlock);

      // Сравниваем строки
      if (it -> data > next_it -> data) {
        std::swap(it -> data, next_it -> data);
        swapped = true;
      }

      // Освобождаем rwlock
      pthread_rwlock_unlock( & next_it -> rwlock);
      pthread_rwlock_unlock( & it -> rwlock);
      if (prev_it != lst.end()) {
        pthread_rwlock_unlock( & prev_it -> rwlock);
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

    pthread_rwlock_wrlock( & list_rwlock); // Блокируем запись
    if (!string_list.empty()) {
      bubble_sort(string_list);
      std::cout << "Дочерний поток: список отсортирован (пузырьковая сортировка)\n";
    }
    pthread_rwlock_unlock( & list_rwlock);
  }
  return nullptr;
}

int main() {
  pthread_t thread;

  // Инициализация rwlock для заголовка списка
  pthread_rwlock_init( & list_rwlock, nullptr);

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

  // Уничтожаем rwlock
  pthread_rwlock_destroy( & list_rwlock);

  return 0;
}
