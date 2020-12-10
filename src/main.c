#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int is_it_time_to_terminate = 0;
const int OFFSET = sizeof(int);

void update_is_it_time_to_terminate(int num) {
  is_it_time_to_terminate = 1;
}

void child_work(char* from, int to) {
  int idx = OFFSET;
  while (1) {
    if (idx < OFFSET + ((int*)from)[0]) {
      char c = from[idx];
      if (c != 'a' && c != 'e' && c != 'i' && c != 'o' && c != 'u' && c != 'y' &&
          c != 'A' && c != 'E' && c != 'I' && c != 'O' && c != 'U' && c != 'Y') {
        write(to, &c, 1);
      }
      ++idx;
    } else {
      if (is_it_time_to_terminate) {
        break;
      }
    }
  }
  close(to);
}

void parrent_work(pid_t child1, char* child_map1, pid_t child2, char* child_map2) {
  char ch;
  int is_even = 0;
  int idx1 = OFFSET;
  int idx2 = OFFSET;
  while (read(STDIN_FILENO, &ch, 1) > 0) {
    if (!is_even) {
      child_map1[idx1++] = ch;
    } else {
      child_map2[idx2++] = ch;
    }
    if (ch == '\n') {
      if (!is_even) {
        ((int*)child_map1)[0] = idx1 - OFFSET;
      } else {
        ((int*)child_map2)[0] = idx2 - OFFSET;
      }
      is_even = !is_even;
    }
  }

  kill(child1, SIGUSR1);
  kill(child2, SIGUSR1);
  int res1;
  int res2;
  waitpid(child1, &res1, 0);
  waitpid(child2, &res2, 0);
  if (res1 != 0 || res2 != 0) {
    fprintf(stderr, "Something ended ne tak!\n%d %d\n", res1, res2);
  }
}

int read_name_and_open_file() {
  const size_t FILE_NAME_SIZE = 64;
  char f_name[FILE_NAME_SIZE];
  char buf[1];
  int idx = 0;
  while (idx < FILE_NAME_SIZE && read(STDIN_FILENO, buf, 1) > 0) {
    if (buf[0] == '\n') {
      break;
    }
    f_name[idx++] = buf[0];
  }
  f_name[idx++] = '\0';
  return open(f_name, O_WRONLY | O_TRUNC);
}

void error(char* buf, size_t size) { write(STDERR_FILENO, buf, size); }
void check_file_id(int id) {
  if (id == -1) {
    error("Файл не найден\n", 27);
    exit(-1);
  }
}
void* check_map_creation() {
  void* m_file = mmap(NULL, 2048, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (m_file == MAP_FAILED) {
    error("Не удалось создать отражение\n", 54);
    exit(-2);
  }
  ((int*)m_file)[0] = 0;
  return m_file;
}
pid_t check_fork() {
  pid_t fd = fork();
  if (fd == -1) {
    error("Не удалось создать процесс\n", 50);
    exit(-3);
  }
  return fd;
}
void add_signals() {
  void (*func)(int);
  func = signal(SIGUSR1, update_is_it_time_to_terminate);
  if (func == SIG_IGN) {
    error("Не удалось назначить сигнал\n", 54);
    exit(-4);
  }
}

int main(int argc, char* argv[]) {
  add_signals();

  int f1 = read_name_and_open_file();
  check_file_id(f1);
  int f2 = read_name_and_open_file();
  check_file_id(f2);

  char* m_file1 = check_map_creation();
  pid_t child1 = check_fork();
  if (child1 == 0) {
    close(f2);
    child_work(m_file1, f1);
    return 0;
  }
  close(f1);

  char* m_file2 = check_map_creation();
  pid_t child2 = check_fork();
  if (child2 == 0) {
    child_work(m_file2, f2);
    return 0;
  }
  close(f2);

  //printf("ids: %d %d %d\n", 0, child1, child2);
  parrent_work(child1, m_file1, child2, m_file2);

  return 0;
}