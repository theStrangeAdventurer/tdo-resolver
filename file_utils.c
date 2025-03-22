#include "./file_utils.h"
#include "tdo_utils.h"
#include <dirent.h> // Подключаем для opendir, readdir, closedir
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Функция для проверки наличия требуемых строк
int contains_required_strings(const char *content, const char **require_strings,
                              size_t require_strings_count) {
  if (!content || require_strings_count == 0)
    return 1; // Если фильтр пустой, включаем все файлы
  for (size_t i = 0; i < require_strings_count; i++) {
    if (strstr(content, require_strings[i])) {
      return 1; // Нашли хотя бы одну строку
    }
  }
  return 0; // Не нашли ни одну строку
}

// Функция для проверки, нужно ли игнорировать элемент
int should_ignore(const char *path, const char **ignore_list,
                  size_t ignore_count) {
  const char *name = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;

  for (size_t i = 0; i < ignore_count; i++) {
    if (strcmp(name, ignore_list[i]) == 0) {
      return 1; // Игнорировать
    }
  }
  return 0; // Не игнорировать
}

// Функция для проверки расширения
int has_ignored_extension(const char *name, const char **ignore_exts,
                          size_t ignore_exts_count) {
  const char *ext = strrchr(name, '.');
  if (!ext)
    return 0; // Нет расширения

  for (size_t i = 0; i < ignore_exts_count; i++) {
    if (strcmp(ext, ignore_exts[i]) == 0) {
      return 1; // Игнорировать
    }
  }
  return 0; // Не игнорировать
}

// void free_list(todo_t **list, int total_files) {
// for (int i = 0; i < total_files; i++) {
//   free(list[i]);
// }
// free(list);
// }

void free_file_tree(file_tree_t *tree) {
  if (!tree) {
    printf("free_file_tree: tree is NULL\n");
    return;
  }

  printf("Freeing tree at %p\n", (void *)tree);
  if (tree->path) {
    printf("Freeing path at %p: %s\n", (void *)tree->path, tree->path);
    free(tree->path);
  }

  if (tree->is_dir) {
    if (tree->content.files) {
      printf("Directory with %zu files at %p\n", tree->num_files,
             (void *)tree->content.files);
      for (size_t i = 0; i < tree->num_files; i++) {
        printf("Recursively freeing file %zu at %p\n", i,
               (void *)tree->content.files[i]);
        free_file_tree(tree->content.files[i]);
      }
      printf("Freeing files array at %p\n", (void *)tree->content.files);
      free(tree->content.files);
    } else {
      printf("No files array, num_files = %zu\n", tree->num_files);
    }
  } else {
    if (tree->content.file_source) {
      printf("Freeing file_source at %p\n", (void *)tree->content.file_source);
      free(tree->content.file_source);
    }
  }

  printf("Freeing tree structure at %p\n", (void *)tree);
  free(tree);
}

char *get_file_content(const char *path, size_t content_size) {
  // Открываем файл
  FILE *file = fopen(path, "r");
  if (!file) {
    perror("Ошибка открытия файла");
    return NULL;
  }

  // Выделяем память под содержимое файла (+1 для завершающего нуля)
  char *content = malloc(content_size + 1);
  if (!content) {
    perror("get_file_content_error content malloc");
    fclose(file);
    return NULL;
  }

  // Читаем содержимое файла
  size_t read_size = fread(content, 1, content_size, file);
  if (read_size != content_size) {
    perror("get_file_content_error read_size fread");
    free(content);
    fclose(file);
    return NULL;
  }

  // Добавляем завершающий нуль
  content[read_size] = '\0';

  // Закрываем файл
  fclose(file);
  return content;
}
file_tree_t *get_file_tree(char *path, const char **ignore_dirs,
                           size_t ignore_dirs_count, const char **ignore_exts,
                           size_t ignore_exts_count,
                           const char **require_strings,
                           size_t require_strings_count) {
  struct stat path_stat;
  if (stat(path, &path_stat) != 0) {
    perror("tdo_error:get_file_tree/path_stat");
    return NULL;
  }

  file_tree_t *node = malloc(sizeof(file_tree_t));

  if (node == NULL)
    return NULL;

  node->path = strdup(path);
  node->is_dir = S_ISDIR(path_stat.st_mode);
  node->num_files = 0;
  // Если это директория и она в списке игнорируемых
  if (node->is_dir &&
      should_ignore(node->path, ignore_dirs, ignore_dirs_count)) {
    free(node->path);
    free(node);
    return NULL;
  }
  if (!node->is_dir) {
    // Проверяем расширение
    if (has_ignored_extension(node->path, ignore_exts, ignore_exts_count)) {
      free(node->path);
      free(node);
      return NULL;
    }
    node->content.file_source = get_file_content(path, path_stat.st_size);
    if (!node->content.file_source) {
      free(node->path);
      free(node);
      return NULL;
    }
    // Проверяем содержимое на наличие требуемых строк
    if (!contains_required_strings(node->content.file_source, require_strings,
                                   require_strings_count)) {
      free(node->content.file_source);
      free(node->path);
      free(node);
      return NULL;
    }
    return node;
  }
  // Это директория
  DIR *dir = opendir(path);
  if (!dir) {
    free(node->path);
    free(node);
    return NULL;
  }

  struct dirent *entry;
  size_t count = 0;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
      count++;
    }
  }
  rewinddir(dir);

  node->content.files = malloc(count * sizeof(file_tree_t));
  if (!node->content.files) {
    free(node->path);
    free(node);
    closedir(dir);
    return NULL;
  }

  size_t i = 0;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

    file_tree_t *child = get_file_tree(
        full_path, ignore_dirs, ignore_dirs_count, ignore_exts,
        ignore_exts_count, require_strings, require_strings_count);
    if (child) {
      node->content.files[i++] = child;
    }
  }
  node->num_files = i;

  closedir(dir);
  return node;
}

char *prettify_path(char *path) {
  // Получаем путь до домашней директории
  const char *home = getenv("HOME");
  if (!home) {
    // Если HOME не установлен, возвращаем исходный путь
    return strdup(path);
  }

  // Проверяем, начинается ли путь с домашней директории
  size_t home_len = strlen(home);
  if (strncmp(path, home, home_len) == 0) {
    // Если да, создаём новый путь с "~"
    char *new_path = malloc(strlen(path) - home_len + 2); // +2 для "~" и нуля
    if (!new_path) {
      return NULL; // Ошибка выделения памяти
    }

    new_path[0] = '~';
    strcpy(new_path + 1, path + home_len); // Копируем оставшуюся часть пути
    return new_path;
  }

  // Если путь не начинается с домашней директории, возвращаем копию исходного
  // пути
  return strdup(path);
}

size_t count_files_from_tree(file_tree_t *tree) {
  if (!tree)
    return 0;
  if (!tree->is_dir)
    return 1;
  size_t total = 0;
  for (size_t i = 0; i < tree->num_files; i++) {
    total += count_files_from_tree(tree->content.files[i]);
  }
  return total;
}

void fill_list(file_tree_t *tree, char **list, size_t *index) {
  if (!tree)
    return;

  if (!tree->is_dir) {
    list[*index] = strdup(tree->path);
    (*index)++;
    return;
  }
  for (size_t i = 0; i < tree->num_files; i++) {
    fill_list(tree->content.files[i], list, index);
  }
}

char **get_list_from_tree(file_tree_t *tree) {
  int total = count_files_from_tree(tree);
  char **result = malloc((total + 1) * sizeof(char *));
  if (!result)
    return NULL;
  size_t index = 0;
  fill_list(tree, result, &index);
  return result;
}

void print_file_list(char **list, int listc, int *active_index,
                     int *opened_index) {
  if (!list)
    return;
  for (int i = 0; i < listc; i++) {
    if (i == *active_index && *active_index == *opened_index) {
      printf("╠» [%s]\n", prettify_path(list[i]));
    } else if (i == *active_index) {
      printf("╠» %s\n", prettify_path(list[i]));
    } else {
      printf("║  %s\n", prettify_path(list[i]));
    }
  }
}
