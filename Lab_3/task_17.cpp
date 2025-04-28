#include <iostream>
#include <pthread.h>
#include <unistd.h> // Для sleep
#include <string>
#include <list>     // Для связанного списка

// Глобальные переменные
pthread_mutex_t mutex; // Мьютекс для защиты списка
std::list < std::string > string_list; // Связанный список для хранения строк

// Функция для разделения строки на части по 80 символов
void split_string(const std::string & input) {
  size_t len = input.length();
  size_t pos = 0;

  while (pos < len) {
    size_t chunk_size = std::min < size_t > (80, len - pos);
    std::string chunk = input.substr(pos, chunk_size);
    pthread_mutex_lock( & mutex);
    string_list.push_front(chunk); // Добавляем в начало списка
    pthread_mutex_unlock( & mutex);
    pos += chunk_size;
  }
}

// Функция для вывода списка
void print_list() {
  pthread_mutex_lock( & mutex);
  if (string_list.empty()) {
    std::cout << "Список пуст\n";
  } else {
    std::cout << "Текущее состояние списка:\n";
    int index = 1;
    for (const auto & str: string_list) {
      std::cout << index++ << ". " << str << "\n";
    }
  }
  pthread_mutex_unlock( & mutex);
}

// Функция пузырьковой сортировки для std::list (по возрастанию)
void bubble_sort(std::list < std::string > & lst) {
  if (lst.empty()) return;

  bool swapped;
  do {
    swapped = false;
    auto it = lst.begin();
    auto next_it = std::next(it);

    // Проходим по списку, сравнивая соседние элементы
    while (next_it != lst.end()) {
      if ( * it > * next_it) { // Сортировка по возрастанию (a < b)
        std::swap( * it, * next_it);
        swapped = true;
      }
      ++it;
      ++next_it;
    }
  } while (swapped); // Повторяем, пока есть обмены
}

// Функция для дочернего потока (сортировка каждые 5 секунд)
void * sortThread(void * arg) {
  while (true) {
    sleep(5); // Ждем 5 секунд

    pthread_mutex_lock( & mutex);
    if (!string_list.empty()) {
      bubble_sort(string_list); // Используем пузырьковую сортировку
      std::cout << "Дочерний поток: список отсортирован (пузырьковая сортировка)\n";
    }
    pthread_mutex_unlock( & mutex);
  }
  return nullptr;
}

int main() {
  pthread_t thread;

  // Инициализация мьютекса
  pthread_mutexattr_t mutex_attr;
  pthread_mutexattr_init( & mutex_attr);
  pthread_mutexattr_settype( & mutex_attr, PTHREAD_MUTEX_ERRORCHECK); // Используем ERRORCHECK для отладки
  pthread_mutex_init( & mutex, & mutex_attr);

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
      // Пустая строка — выводим список
      print_list();
    } else {
      // Разделяем строку, если она длиннее 80 символов, и добавляем в список
      split_string(input);
    }
    std::cout << "Введите следующую строку:\n";
  }

  // Ожидаем завершения дочернего потока
  pthread_cancel(thread); // Принудительно завершаем поток (для примера)
  pthread_join(thread, nullptr);

  // Уничтожаем мьютекс и атрибуты
  pthread_mutex_destroy( & mutex);
  pthread_mutexattr_destroy( & mutex_attr);

  return 0;
}
