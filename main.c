#include "./file_utils.h"
#include <stdio.h>
#ifdef __linux__
#include <getopt.h> // На Linux getopt_long в <getopt.h>
#else
#include <unistd.h> // На macOS getopt_long в <unistd.h>
#endif

int main(int argc, char *argv[]) {
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
      printf("usage: tdo [OPTIONS] list | export | add | remove");
      break;
    default:
      printf("Unknown option %d", opt);
    }
  }

  file_tree_t *tree =
      get_file_tree(dir, ignore_dirs, ignore_dirs_count, ignore_exts,
                    ignore_exts_count, require_strings, require_strings_count);

  printf("\n=== TDO ===\n");
  int total_files = count_files_from_tree(tree);
  int active_index = 1;
  char **list = get_list_from_tree(tree);
  print_file_list(list, total_files, &active_index);
  return 0;
}
