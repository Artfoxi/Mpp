#include <iostream>
#include <pthread.h>
#include <unistd.h> // Для sleep()

// Функция очистки, вызываемая при завершении потока
void cleanupHandler(void * arg) {
	std::cout << "Дочерний поток: завершение работы!" << std::endl;
}
// Функция, выполняемая дочерним потоком
void * threadFunction(void * arg) {
	int count = 1;
	// Регистрируем обработчик очистки
	pthread_cleanup_push(cleanupHandler, nullptr);
	while(true) { // Бесконечный цикл для вывода текста
		std::cout << "Дочерний поток: строка " << count++ << std::endl;
		sleep(1); // Задержка 1 секунда между выводами
	}
	// Удаляем обработчик (никогда не выполнится из-за бесконечного цикла)
	pthread_cleanup_pop(0);
	return nullptr; // Никогда не достигнется
}

int main() {
	pthread_t thread;
	// Создаём дочерний поток
	if(pthread_create( & thread, nullptr, threadFunction, nullptr) != 0) {
		std::cerr << "Ошибка при создании потока!" << std::endl;
		return 1;
	}
	// Даём дочернему потоку поработать 2 секунды
	sleep(2);
	// Прерываем дочерний поток
	if(pthread_cancel(thread) != 0) {
		std::cerr << "Ошибка при отмене потока!" << std::endl;
		return 1;
	}
	// Ждём завершения дочернего потока
	pthread_join(thread, nullptr);
	std::cout << "Родительский поток: дочерний поток завершён." << std::endl;
	return 0;
}
