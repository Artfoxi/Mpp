#include <iostream>
#include <vector>
#include <string>
#include <pthread.h>
#include <unistd.h>

using namespace std;

// Структура для передачи данных в поток
struct ThreadData {
  string str;
  size_t length;
};

// Функция, выполняемая каждым потоком
void * printString(void * arg) {
  ThreadData * data = (ThreadData * ) arg;
  // Спим время, пропорциональное длине строки (в микросекундах)
  usleep(data -> length * 10000); // Коэффициент 10000 микросекунд
  cout << data -> str << endl;
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

  // Массив потоков
  vector < pthread_t > threads(n);

  // Создаем потоки
  for (int i = 0; i < n; i++) {
    ThreadData * data = new ThreadData {
      strings[i], strings[i].length()
    };

    // Создаем поток
    int result = pthread_create( & threads[i], nullptr, printString, (void * ) data);
    if (result != 0) {
      cerr << "Ошибка создания потока " << i << endl;
      delete data;
      return 1;
    }

    // Отсоединяем поток, чтобы он очищался автоматически после завершения
    pthread_detach(threads[i]);
  }

  // Даем потокам время на завершение
  sleep((n + 1) * 2); // Ждем достаточно долго

  return 0;
}
