#include <iostream>
#include <pthread.h>

// Структура для передачи параметров в поток
struct ThreadData {
	const char * lines[10]; // Массив строк (до 10 строк на поток)
	int lineCount; // Количество строк для вывода
};
// Функция, выполняемая потоками
void * threadFunction(void * arg) {
	ThreadData * data = static_cast < ThreadData * > (arg); // Приводим void* к структуре
	for(int i = 0; i < data -> lineCount; ++i) {
		std::cout << "Поток: " << data -> lines[i] << std::endl;
	}
	return nullptr;
}
int main() {
	const int NUM_THREADS = 4;
	pthread_t threads[NUM_THREADS];
	ThreadData threadData[NUM_THREADS];

	// Подготовка уникальных последовательностей строк для каждого потока
	threadData[0] = {{"Первая строка потока 1", "Вторая строка потока 1"}, 2};
	threadData[1] = {{"Сообщение от потока 2", "Ещё одно от потока 2", "Третье от потока 2"}, 3};
	threadData[2] = {{"Поток 3 приветствует", "Поток 3 прощается"}, 2};
	threadData[3] = {{"Единственная строка потока 4"},1};

	// Создание четырёх потоков
	for(int i = 0; i < NUM_THREADS; ++i) {
		if(pthread_create( & threads[i], nullptr, threadFunction, & threadData[i]) != 0) {
			std::cerr << "Ошибка при создании потока " << i << "!" << std::endl;
			return 1;
		}
	}
	// Ожидание завершения всех потоков
	for(int i = 0; i < NUM_THREADS; ++i) {
		pthread_join(threads[i], nullptr);
	}
	std::cout << "Все потоки завершили работу." << std::endl;
	return 0;
}
