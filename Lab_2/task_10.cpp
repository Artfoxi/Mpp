#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <cstdlib>

const int PHILO = 5;
const int DELAY = 30000;
const int FOOD = 50;

pthread_mutex_t forks[PHILO];
pthread_t phils[PHILO];
pthread_mutex_t foodlock;
pthread_mutex_t cout_lock; // Мьютекс для синхронизации вывода
int sleep_seconds = 0;

void * philosopher(void * id);
int food_on_table();
void get_forks(int phil, int fork1, int fork2);
void down_forks(int fork1, int fork2);

int main(int argc, char ** argv) {
  if (argc == 2) {
    sleep_seconds = std::atoi(argv[1]);
  }

  pthread_mutex_init( & foodlock, nullptr);
  pthread_mutex_init( & cout_lock, nullptr); // Инициализируем мьютекс для вывода
  for (int i = 0; i < PHILO; ++i) {
    pthread_mutex_init( & forks[i], nullptr);
  }

  for (int i = 0; i < PHILO; ++i) {
    pthread_create( & phils[i], nullptr, philosopher, (void * )(long) i);
  }

  for (int i = 0; i < PHILO; ++i) {
    pthread_join(phils[i], nullptr);
  }

  pthread_mutex_destroy( & foodlock);
  pthread_mutex_destroy( & cout_lock); // Уничтожаем мьютекс для вывода
  for (int i = 0; i < PHILO; ++i) {
    pthread_mutex_destroy( & forks[i]);
  }

  return 0;
}

void * philosopher(void * num) {
  int id = (int)(long) num;
  pthread_mutex_lock( & cout_lock);
  std::cout << "Philosopher " << id << " садится к столу.\n";
  pthread_mutex_unlock( & cout_lock);

  int right_fork = id;
  int left_fork = (id + 1) % PHILO;

  int first_fork = std::min(left_fork, right_fork);
  int second_fork = std::max(left_fork, right_fork);

  int f;
  while ((f = food_on_table()) > 0) {
    if (id == 1) {
      sleep(sleep_seconds);
    }

    pthread_mutex_lock( & cout_lock);
    std::cout << "Philosopher " << id << ": начинает есть " << f << ".\n";
    pthread_mutex_unlock( & cout_lock);

    get_forks(id, first_fork, second_fork);

    pthread_mutex_lock( & cout_lock);
    std::cout << "Philosopher " << id << ": ест.\n";
    pthread_mutex_unlock( & cout_lock);

    usleep(DELAY * (FOOD - f + 1));

    down_forks(first_fork, second_fork);
  }

  pthread_mutex_lock( & cout_lock);
  std::cout << "Philosopher " << id << " поел.\n";
  pthread_mutex_unlock( & cout_lock);

  return nullptr;
}

int food_on_table() {
  static int food = FOOD;
  int myfood;

  pthread_mutex_lock( & foodlock);
  if (food > 0) {
    food--;
  }
  myfood = food;
  pthread_mutex_unlock( & foodlock);

  return myfood;
}

void get_forks(int phil, int fork1, int fork2) {
  pthread_mutex_lock( & forks[fork1]);
  pthread_mutex_lock( & cout_lock);
  std::cout << "Philosopher " << phil << ": взял вилку " << fork1 << ".\n";
  pthread_mutex_unlock( & cout_lock);

  pthread_mutex_lock( & forks[fork2]);
  pthread_mutex_lock( & cout_lock);
  std::cout << "Philosopher " << phil << ": взял вилку " << fork2 << ".\n";
  pthread_mutex_unlock( & cout_lock);
}

void down_forks(int fork1, int fork2) {
  pthread_mutex_unlock( & forks[fork1]);
  pthread_mutex_unlock( & forks[fork2]);
}
