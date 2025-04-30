#include "../includes/file_utils.h"
#include "../includes/render.h"
#include "../includes/tdo_utils.h"
#include "../includes/term.h"
#include <getopt.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <getopt.h> // На Linux getopt_long в <getopt.h>
#else
#include <unistd.h> // На macOS getopt_long в <unistd.h>
#endif

#define MAX_PATH_LEN 1024

typedef struct {
  char mode[10];          // view | export
  char dir[MAX_PATH_LEN]; // where from view or export todos or notes
  char to[MAX_PATH_LEN];  // where to export
  int to_specified;       // --to option was specified though cli interface
} ProgramOptions;

static int parse_arguments(int argc, char *argv[], ProgramOptions *options) {
  if (argc < 2) {
    fprintf(stderr,
            "Usage: %s <view|export> --dir <directory> [--to <filename>]\n",
            argv[0]);
    return 0;
  }

  // Parse tool mode (view/export)
  strncpy(options->mode, argv[1], sizeof(options->mode) - 1);
  options->mode[sizeof(options->mode) - 1] = '\0';

  if (strcmp(options->mode, "--help") == 0) {
    printf("Usage: %s <view|export> --dir <directory> [--to <filename>]\n",
           argv[0]);
    return 0;
  }

  if (strcmp(options->mode, "view") != 0 &&
      strcmp(options->mode, "export") != 0) {
    fprintf(stderr, "Error: First argument must be 'view' or 'export'\n");
    return 0;
  }

  static struct option long_options[] = {{"dir", required_argument, 0, 'd'},
                                         {"to", required_argument, 0, 't'},
                                         {0, 0, 0, 0}};

  options->dir[0] = '\0';
  options->to[0] = '\0';
  options->to_specified = 0;

  int opt;
  int option_index = 0;
  while ((opt = getopt_long(argc - 1, argv + 1, "d:f:", long_options,
                            &option_index)) != -1) {
    switch (opt) {
    case 'd':
      strncpy(options->dir, optarg, MAX_PATH_LEN - 1);
      options->dir[MAX_PATH_LEN - 1] = '\0';
      size_t len = strlen(options->dir);
      if (len > 0 && options->dir[len - 1] == '/') {
        options->dir[len - 1] = '\0'; // Удаляем последний символ
      }
      break;
    case 't':
      if (strcmp(options->mode, "export") != 0) {
        fprintf(stderr, "Warning: --to is only valid for 'export' mode\n");
      } else {
        strncpy(options->to, optarg, MAX_PATH_LEN - 1);
        options->to[MAX_PATH_LEN - 1] = '\0';
        options->to_specified = 1;
      }
      break;
    default:
      return 0;
    }
  }

  if (options->dir[0] == '\0') {
    fprintf(stderr, "Error: --dir parameter is required\n");
    return 0;
  }

  return 1;
}

// Global vars >>
static file_tree_t *global_tree = NULL;
static todo_t *global_list = NULL;
static size_t global_total_files = 0;
// << Global Vars;

static void signal_handler(int sig) {
  // Restore terminal from raw mode
  disable_raw_mode();
  clear_screen();

  if (global_tree) {
    free_file_tree(global_tree);
    global_tree = NULL; // Предотвращаем повторное освобождение
  }
  if (global_list) {
    free(global_list);
    global_list = NULL;
  }

  printf("Terminated by Ctrl+C (signal %d)\n", sig);
  fflush(stdout);
  exit(1);
}

static int is_editor_available(const char *editor) {
  char command[256];
  snprintf(command, sizeof(command), "command -v %s > /dev/null 2>&1", editor);
  return system(command) == 0;
}

static const char *get_default_editor() {
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
  ProgramOptions options;

  if (!parse_arguments(argc, argv, &options)) {
    return EXIT_FAILURE;
  }

  signal(SIGINT, signal_handler);

  const char *editor = getenv("TDO_EDITOR");

  if (editor == NULL) {
    editor = getenv("EDITOR");
  }

  if (editor == NULL) {
    editor = get_default_editor();
    if (!editor)
      editor = "<nothing>";
    printf("$EDITOR env var do not exists, use \"%s\" as a default editor \n",
           editor);
  }
  char *default_export_file =
      "/tdo_export.json"; // '/' only for concatenation purposes

  const char *ignore_dirs[] = {"node_modules", "dist", "build",
                               ".git"}; // TODO: move to env vars
  size_t ignore_dirs_count = sizeof(ignore_dirs) / sizeof(ignore_dirs[0]);
  const char *ignore_exts[] = {
      ".o",   ".bin", ".svg",  ".jpg", ".png",
      ".ico", ".exe", ".json", ".webp"}; // TODO: move to env vars
  size_t ignore_extensions_count = sizeof(ignore_exts) / sizeof(ignore_exts[0]);

  global_tree = get_file_tree(options.dir, ignore_dirs, ignore_dirs_count,
                              ignore_exts, ignore_extensions_count);

  int active_index = 0; // index of an active item in list
  int start_index = 0;  // the index to start rendering the list from
  int opened_index =
      -1; // index of an opened file (showing the surrounding context)

  collect_all_todos(global_tree, &global_list, &global_total_files);

  switch (options.mode[0]) {
  case 'v': { // view mode
    clear_screen();
    enable_raw_mode();

    render_loop(global_list, &active_index, &opened_index, &start_index,
                global_total_files, editor);

    disable_raw_mode();

    clear_screen();

    free_file_tree(global_tree);
    global_tree = NULL;
    free(global_list);
    global_list = NULL;
    break;
  }
  case 'e': { // export mode
    char file_path[MAX_PATH_LEN] = {0};

    if (options.to_specified) {
      printf("Export path: %s\n", options.to);
      if (options.to[0] != '/' &&
          options.to[0] != '~') { // Если передан не полный путь к файлу
        char temp_path[MAX_PATH_LEN] = {0};
        snprintf(temp_path, sizeof(temp_path), "/%s", options.to);
        construct_file_path(file_path, options.dir, temp_path, MAX_PATH_LEN);
      } else {
        // Передан полный путь до файла экспорта
        strncat(file_path, options.to, MAX_PATH_LEN - 1);
        file_path[MAX_PATH_LEN - 1] = '\0';
      }
    } else {
      construct_file_path(
          file_path, options.dir, default_export_file,
          MAX_PATH_LEN); // Экспортируем в дефолтный файл в options.dir
    }
    char *json_content = get_todos_json(global_list, global_total_files);
    write_file(file_path, json_content);
    printf("\n\n ✓ Report saved to: %s\n\n", file_path);
    free(json_content);
    json_content = NULL;
    free_file_tree(global_tree);
    global_tree = NULL;
    free(global_list);
    global_list = NULL;
    break;
  }

    return 0;
  }

  return 0;
}
