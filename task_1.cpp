#include <iostream>
#include <pthread.h>

void * threadFunction(void * arg) {
    for (int i = 0; i < 10; ++i) {
        std::cout << "Новый поток: строка " << i + 1 << std::endl;
    }
    return nullptr;
}
int main() {
    pthread_t thread;
    // Создаём поток с атрибутами по умолчанию (nullptr в качестве атрибутов)
    if (pthread_create( & thread, nullptr, threadFunction, nullptr) != 0) {
        std::cerr << "Ошибка при создании потока!" << std::endl;
        return 1;
    }
    // Родительский поток выводит свои строки
    for (int i = 0; i < 10; ++i) {
        std::cout << "Родительский поток: строка " << i + 1 << std::endl;
    }
    // Ожидаем завершения потока (можно убрать, если не нужно по условию)
    pthread_join(thread, nullptr);
    return 0;
}
