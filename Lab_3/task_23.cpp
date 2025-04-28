#include <iostream>
#include <vector>
#include <string>
#include <pthread.h>
#include <unistd.h>

using namespace std;

// Структура для узла связного списка
struct Node {
  string str;
  Node * next;
  Node(const string & s): str(s), next(nullptr) {}
};

// Структура для передачи данных в поток
struct ThreadData {
  string str;
  size_t length;
  Node ** head; // Указатель на начало списка
  Node ** tail; // Указатель на конец списка
  pthread_mutex_t * list_mutex; // Мьютекс для списка
  int * finished_count; // Счётчик завершённых потоков
  pthread_mutex_t * count_mutex; // Мьютекс для счётчика
  pthread_cond_t * cond; // Условная переменная для ожидания
  int total_threads; // Общее число потоков
};

// Функция для добавления строки в конец списка
void add_to_list(Node ** head, Node ** tail,
  const string & str, pthread_mutex_t * list_mutex) {
  pthread_mutex_lock(list_mutex);
  Node * new_node = new Node(str);
  if ( * head == nullptr) {
    // Список пуст, новый узел становится головой и хвостом
    * head = new_node;
    * tail = new_node;
  } else {
    // Добавляем в конец списка
    ( * tail) -> next = new_node;
    * tail = new_node;
  }
  pthread_mutex_unlock(list_mutex);
}

// Функция, выполняемая каждым потоком
void * sortString(void * arg) {
  ThreadData * data = (ThreadData * ) arg;

  // Спим время, пропорциональное длине строки (в микросекундах)
  usleep(data -> length * 10000);

  // Добавляем строку в конец списка
  add_to_list(data -> head, data -> tail, data -> str, data -> list_mutex);

  // Увеличиваем счётчик завершённых потоков
  pthread_mutex_lock(data -> count_mutex);
  ( * data -> finished_count) ++;
  if ( * data -> finished_count == data -> total_threads) {
    // Если это последний поток, сигнализируем основному потоку
    pthread_cond_signal(data -> cond);
  }
  pthread_mutex_unlock(data -> count_mutex);

  delete data; // Освобождаем память
  return nullptr;
}

int main() {
  vector < string > strings;
  string input;

  // Считываем строки до конца ввода
  while (getline(cin, input)) {
    if (input.empty() && cin.eof()) break;
    strings.push_back(input);
  }

  const int n = strings.size();
  if (n > 100) {
    cerr << "Ошибка: допустимо 100 строк максимум" << endl;
    return 1;
  }

  // Инициализация синхронизационных примитивов
  pthread_mutex_t list_mutex;
  pthread_mutex_t count_mutex;
  pthread_cond_t cond;
  pthread_mutex_init( & list_mutex, nullptr);
  pthread_mutex_init( & count_mutex, nullptr);
  pthread_cond_init( & cond, nullptr);

  Node * head = nullptr; // Начало связного списка
  Node * tail = nullptr; // Конец связного списка
  int finished_count = 0; // Счётчик завершённых потоков

  // Массив потоков
  vector < pthread_t > threads(n);

  // Создаем потоки
  for (int i = 0; i < n; i++) {
    ThreadData * data = new ThreadData {
      strings[i],
        strings[i].length(), &
        head, &
        tail, &
        list_mutex, &
        finished_count, &
        count_mutex, &
        cond,
        n
    };

    int result = pthread_create( & threads[i], nullptr, sortString, (void * ) data);
    if (result != 0) {
      cerr << "Ошибка создания потока " << i << endl;
      delete data;
      return 1;
    }
  }

  // Ожидаем завершения всех потоков
  pthread_mutex_lock( & count_mutex);
  while (finished_count < n) {
    pthread_cond_wait( & cond, & count_mutex);
  }
  pthread_mutex_unlock( & count_mutex);

  // Присоединяем потоки для корректного завершения
  for (int i = 0; i < n; i++) {
    pthread_join(threads[i], nullptr);
  }

  // Выводим связный список
  Node * current = head;
  while (current != nullptr) {
    cout << current -> str << endl;
    Node * temp = current;
    current = current -> next;
    delete temp; // Освобождаем память
  }

  // Уничтожаем синхронизационные примитивы
  pthread_mutex_destroy( & list_mutex);
  pthread_mutex_destroy( & count_mutex);
  pthread_cond_destroy( & cond);

  return 0;
}
