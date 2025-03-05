#include <iostream>
#include <pthread.h>

// Функция, выполняемая дочерним потоком
void * threadFunction(void * arg) {
	for(int i = 0; i < 10; ++i) {
		std::cout << "Дочерний поток: строка " << i + 1 << std::endl;
	}
	return nullptr;
}
int main() {
	pthread_t thread;
	// Создаём поток с атрибутами по умолчанию
	if(pthread_create( & thread, nullptr, threadFunction, nullptr) != 0) {
		std::cerr << "Ошибка при создании потока!" << std::endl;
		return 1;
	}
	// Ждём завершения дочернего потока
	pthread_join(thread, nullptr);
	// После завершения дочернего потока выводим строки родительского
	for(int i = 0; i < 10; ++i) {
		std::cout << "Родительский поток: строка " << i + 1 << std::endl;
	}
	return 0;
}
