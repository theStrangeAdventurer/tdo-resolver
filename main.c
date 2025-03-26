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
    free(global_list);
    global_list = NULL;
  }

  // Выводим сообщение (опционально)
  printf("Terminated by Ctrl+C (signal %d)\n", sig);
  fflush(stdout);

  // Завершаем программу
  exit(1);
}

// Функция для проверки доступности редактора в системе
int is_editor_available(const char *editor) {
  char command[256];
  snprintf(command, sizeof(command), "command -v %s > /dev/null 2>&1", editor);
  return system(command) == 0;
}

// Функция для получения редактора по умолчанию
const char *get_default_editor() {
  const char *editors[] = {"nvim", "nano", "vim", "vi",
                           NULL}; // Список редакторов для проверки
  for (int i = 0; editors[i] != NULL; i++) {
    if (is_editor_available(editors[i])) {
      return editors[i];
    }
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  signal(SIGINT, signal_handler);
  const char *editor = getenv("EDITOR");

  if (editor == NULL) {
    editor = get_default_editor();
    if (!editor)
      editor = "<nothing>";
    printf("$EDITOR env var do not exists, use \"%s\" as a default editor \n",
           editor);
  }
  int opt;
  char *dir = NULL;
  char *export_path = NULL;

  const char *ignore_dirs[] = {"node_modules", "dist", "build",
                               ".git"}; // FIXME: move to args or env vars
  size_t ignore_dirs_count = sizeof(ignore_dirs) / sizeof(ignore_dirs[0]);
  const char *ignore_exts[] = {".o", ".bin"};
  size_t ignore_extensions_count = sizeof(ignore_exts) / sizeof(ignore_exts[0]);
  const char *required_strings[] = {"FIXME:", "TODO:"};
  size_t required_str_count =
      sizeof(required_strings) / sizeof(required_strings[0]);

  struct option long_options[] = {
      {"dir", required_argument, 0, 'd'},    // --dir или -d
      {"export", required_argument, 0, 'e'}, // --ext или -e
      {"help", no_argument, 0, 'h'},         // --help или -h
      {0, 0, 0, 0}                           // Конец массива
  };
  while ((opt = getopt_long(argc, argv, "d:e:h", long_options, NULL)) != -1) {
    switch (opt) {
    case 'd':
      dir = optarg;
      break;
    case 'e':
      export_path = optarg;
      if (!export_path) {
        printf("You should provide path for export");
        return 1;
      }
      break;
    case 'h':
      print_help();
      break;
    default:
      printf("Unknown option %d", opt);
    }
  }

  if (export_path) {
    printf("Export your todos to json в %s\n", export_path);
    return 0;
  }

  global_tree = get_file_tree(dir, ignore_dirs, ignore_dirs_count, ignore_exts,
                              ignore_extensions_count, required_strings,
                              required_str_count);

  int active_index = 1;
  int opened_index = -1;
  collect_all_todos(global_tree, &global_list, &global_total_files);
  clear_screen();
  enable_raw_mode();

  render_loop(global_list, &active_index, &opened_index, global_total_files,
              editor);

  disable_raw_mode();

  clear_screen();

  free_file_tree(global_tree);
  global_tree = NULL;
  free(global_list);
  global_list = NULL;
  return 0;
}
