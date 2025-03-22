#pragma once

#include "file_utils.h"
typedef struct todo {
  char *path;                    // Путь до файла
  char *title;                   // Заголовок TODO / FIXME : <task text>
  int line;                      // Номер строки
  char *surround_content_before; // Сколько-то строк до
  char *surround_content_after;  // Сколько-то строк после
} todo_t;

int collect_all_todos(file_tree_t *tree, todo_t **todos, size_t *out_count);

void print_todo_list(todo_t *list, int listc, int *active_index,
                     int *opened_index);
void print_help(void);
