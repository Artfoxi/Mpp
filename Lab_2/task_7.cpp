#include <errno.h>
#include <iostream>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <vector>

struct CopyArgs {
  std::string src_path;
  std::string dest_path;
};

// Функция копирования файла
void * copy_file(void * arg) {
  CopyArgs * args = (CopyArgs * ) arg;

  int src_fd = open(args -> src_path.c_str(), O_RDONLY);
  while (src_fd == -1 && errno == EMFILE) {
    sleep(1); // Ждем при превышении лимита файлов
    src_fd = open(args -> src_path.c_str(), O_RDONLY);
  }
  if (src_fd == -1) {
    perror("open source file");
    delete args;
    return nullptr;
  }

  int dest_fd = open(args -> dest_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  while (dest_fd == -1 && errno == EMFILE) {
    sleep(1);
    dest_fd = open(args -> dest_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  }
  if (dest_fd == -1) {
    perror("open destination file");
    close(src_fd);
    delete args;
    return nullptr;
  }

  char buffer[4096];
  ssize_t bytes_read;
  while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
    write(dest_fd, buffer, bytes_read);
  }

  close(src_fd);
  close(dest_fd);
  delete args;
  return nullptr;
}

// Функция обхода директории
void * process_directory(void * arg) {
  CopyArgs * args = (CopyArgs * ) arg;
  std::vector < pthread_t > threads;

  DIR * dir = opendir(args -> src_path.c_str());
  while (dir == nullptr && errno == EMFILE) {
    sleep(1); // Ждем при превышении лимита файлов
    dir = opendir(args -> src_path.c_str());
  }
  if (dir == nullptr) {
    perror("opendir");
    delete args;
    return nullptr;
  }

  long name_max = pathconf(args -> src_path.c_str(), _PC_NAME_MAX);
  if (name_max == -1) name_max = 255;
  size_t dirent_size = sizeof(struct dirent) + name_max + 1;
  struct dirent * entry = (struct dirent * ) malloc(dirent_size);
  struct dirent * result;

  // Создаем целевую директорию
  mkdir(args -> dest_path.c_str(), 0755);

  while (readdir_r(dir, entry, & result) == 0 && result != nullptr) {
    if (strcmp(entry -> d_name, ".") == 0 || strcmp(entry -> d_name, "..") == 0) {
      continue;
    }

    std::string src_item = args -> src_path + "/" + entry -> d_name;
    std::string dest_item = args -> dest_path + "/" + entry -> d_name;

    struct stat stat_buf;
    if (stat(src_item.c_str(), & stat_buf) == -1) {
      continue;
    }

    if (S_ISDIR(stat_buf.st_mode)) {
      // Для директории создаем новый поток
      CopyArgs * dir_args = new CopyArgs {
        src_item,
        dest_item
      };
      pthread_t thread;
      pthread_create( & thread, nullptr, process_directory, dir_args);
      threads.push_back(thread);
    } else if (S_ISREG(stat_buf.st_mode)) {
      // Для обычного файла создаем поток копирования
      CopyArgs * file_args = new CopyArgs {
        src_item,
        dest_item
      };
      pthread_t thread;
      pthread_create( & thread, nullptr, copy_file, file_args);
      threads.push_back(thread);
    }
    // Другие типы файлов игнорируем
  }

  free(entry);
  closedir(dir);

  // Ждем завершения всех потоков
  for (pthread_t thread: threads) {
    pthread_join(thread, nullptr);
  }

  delete args;
  return nullptr;
}

int main(int argc, char * argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <source_dir> <dest_dir>" << std::endl;
    return 1;
  }

  std::string src_dir = argv[1];
  std::string dest_dir = argv[2];

  CopyArgs * args = new CopyArgs {
    src_dir,
    dest_dir
  };
  pthread_t main_thread;
  pthread_create( & main_thread, nullptr, process_directory, args);
  pthread_join(main_thread, nullptr);

  return 0;
}
