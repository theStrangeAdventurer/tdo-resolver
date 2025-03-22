#include "./file_utils.h"
#include "./render.h"
#include "./tdo_utils.h"
#include "./term.h"
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef __linux__
#include <getopt.h> // На Linux getopt_long в <getopt.h>
#else
#include <unistd.h> // На macOS getopt_long в <unistd.h>
#endif

// Global vars >>
file_tree_t *global_tree = NULL;
todo_t *global_list = NULL;
size_t global_total_files = 0;
// << Global Vars;

void signal_handler(int sig) {
  // Восстанавливаем терминал из raw-режима
  disable_raw_mode();
  clear_screen();

  // Очищаем память
  if (global_tree) {
    free_file_tree(global_tree);
    global_tree = NULL; // Предотвращаем повторное освобождение
  }
  if (global_list) {
    // free_list(global_list, global_total_files);
    global_list = NULL;
  }

  // Выводим сообщение (опционально)
  printf("Terminated by Ctrl+C (signal %d)\n", sig);
  fflush(stdout);

  // Завершаем программу
  exit(1);
}

int main(int argc, char *argv[]) {
  signal(SIGINT, signal_handler);
  int opt;
  char *dir = NULL; // По умолчанию текущая директория
  char *ext = NULL; // По умолчанию все расширения
                    //
  const char *ignore_dirs[] = {"node_modules", ".git"};
  size_t ignore_dirs_count = sizeof(ignore_dirs) / sizeof(ignore_dirs[0]);
  const char *ignore_exts[] = {".o", ".bin"};
  size_t ignore_exts_count = sizeof(ignore_exts) / sizeof(ignore_exts[0]);
  const char *require_strings[] = {"TODO:", "FIXME:"};
  size_t require_strings_count =
      sizeof(require_strings) / sizeof(require_strings[0]);
  struct option long_options[] = {
      {"dir", required_argument, 0, 'd'}, // --dir или -d
      {"ext", required_argument, 0, 'e'}, // --ext или -e
      {"help", no_argument, 0, 'h'},      // --help или -h
      {0, 0, 0, 0}                        // Конец массива
  };

  while ((opt = getopt_long(argc, argv, "d:e:h", long_options, NULL)) != -1) {
    switch (opt) {
    case 'd':
      dir = optarg;
      break;
    case 'e':
      ext = optarg;
      break;
    case 'h':
      print_help();
      break;
    default:
      printf("Unknown option %d", opt);
    }
  }

  global_tree =
      get_file_tree(dir, ignore_dirs, ignore_dirs_count, ignore_exts,
                    ignore_exts_count, require_strings, require_strings_count);

  int active_index = 1;
  int opened_index = -1;

  printf("Collect_result: %d",
         collect_all_todos(global_tree, &global_list, &global_total_files));
  fflush(stdout);

  clear_screen();
  enable_raw_mode();

  render_loop(global_list, &active_index, &opened_index, global_total_files);

  disable_raw_mode();

  clear_screen();

  free_file_tree(global_tree);
  global_tree = NULL;
  // free_list(global_list, global_total_files);
  // global_list = NULL;
  return 0;
}
