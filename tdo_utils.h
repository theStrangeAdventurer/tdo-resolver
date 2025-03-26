#pragma once

#include "file_utils.h"

typedef struct todo {
  char *path;                    // Путь до файла
  char *title;                   // Заголовок TODO / FIXME : <task text>
  char *raw_title;               // Вся строка с todo
  int line;                      // Номер строки
  char *surround_content_before; // Сколько-то строк до
  char *surround_content_after;  // Сколько-то строк после
} todo_t;

int collect_all_todos(file_tree_t *tree, todo_t **todos, size_t *out_count);

void print_todo_list(todo_t *list, int listc, int *active_index,
                     int *opened_index, int should_render_banner);
void print_help(void);
void print_hotkeys(void);

char *prettify_path(char *path);
void open_source_in_editor(const char *editor, char *file_path,
                           int line_number);
