#include "tdo_utils.h"
#include "file_utils.h"
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Функция для подсчёта строк до позиции
static int get_line_number(const char *start, const char *pos) {
  int line = 1;
  for (const char *p = start; p < pos; p++) {
    if (*p == '\n')
      line++;
  }
  return line;
}
// Получение контекста (строки до и после)
static char *get_context(const char *start, const char *pos, int lines_before,
                         int lines_after) {
  const char *p = pos;
  int current_line = 0;

  // Назад для строк до
  while (p > start && current_line < lines_before) {
    if (*p == '\n')
      current_line++;
    p--;
  }
  while (p < pos && (*p == '\n' || *p == ' ' || *p == '\t'))
    p++;
  const char *context_start = p;

  // Вперед для строк после
  p = pos;
  current_line = 0;
  while (*p && current_line < lines_after) {
    if (*p == '\n')
      current_line++;
    p++;
  }
  const char *context_end = p;

  size_t len = context_end - context_start;
  char *context = (char *)malloc(len + 1);
  if (context) {
    memcpy(context, context_start, len);
    context[len] = '\0';
  }
  return context;
}
// Сбор TODO из одного файла
static int collect_todos_from_file(const char *path, const char *file_content,
                                   todo_t **todos, size_t *out_count) {
  if (!file_content || !path) {
    *out_count = 0;
    return 0;
  }

  const char *pattern = "(FIXME|TODO)(\\([[:alnum:]]+\\))?:";
  regex_t regex;
  int ret = regcomp(&regex, pattern, REG_EXTENDED);
  if (ret != 0) {
    char error_buf[100];
    regerror(ret, &regex, error_buf, sizeof(error_buf));
    fprintf(stderr, "Regex compilation failed: %s\n", error_buf);
    *out_count = 0;
    return 0;
  }

  // Подсчёт совпадений
  size_t count = 0;
  const char *pos = file_content;
  regmatch_t matches[3];
  while (regexec(&regex, pos, 3, matches, 0) == 0) {
    count++;
    pos += matches[0].rm_eo;
    if (!*pos)
      break;
  }

  if (count == 0) {
    regfree(&regex);
    *out_count = 0;
    return 0;
  }

  // Выделение памяти под todos
  todo_t *result = (todo_t *)malloc(count * sizeof(todo_t));
  if (!result) {
    regfree(&regex);
    *out_count = 0;
    return 0;
  }

  // Заполнение todos
  // TODO: test task name
  pos = file_content;
  size_t i = 0;
  while (i < count) {
    ret = regexec(&regex, pos, 3, matches, 0);
    if (ret != 0)
      break;

    todo_t *todo = &result[i];

    // Путь
    // FIXME: test fixme task
    todo->path = strdup(path);
    if (!todo->path)
      goto cleanup;

    // Номер строки
    todo->line = get_line_number(file_content, pos + matches[0].rm_so);

    // Заголовок
    const char *title_start = pos + matches[0].rm_so;
    const char *title_end = title_start;
    while (*title_end && *title_end != '\n')
      title_end++;
    size_t title_len = title_end - title_start;
    todo->title = (char *)malloc(title_len + 1);
    if (!todo->title)
      goto cleanup;
    memcpy(todo->title, title_start, title_len);
    todo->title[title_len] = '\0';

    // Контекст
    todo->surround_content_before =
        get_context(file_content, pos + matches[0].rm_so, 2, 0);
    todo->surround_content_after =
        get_context(file_content, pos + matches[0].rm_eo, 0, 2);

    i++;
    pos += matches[0].rm_eo;
    if (!*pos)
      break;
  }

  regfree(&regex);
  *todos = result;
  *out_count = count;
  return 1;

cleanup:
  for (size_t j = 0; j < i; j++) {
    free(result[j].path);
    free(result[j].title);
    free(result[j].surround_content_before);
    free(result[j].surround_content_after);
  }
  free(result);
  regfree(&regex);
  *out_count = 0;
  return 0;
}

// Сбор всех TODO из дерева
int collect_all_todos(file_tree_t *tree, todo_t **todos, size_t *out_count) {
  if (!tree || !out_count) {
    *out_count = 0;
    return 0;
  }

  size_t total_count = 0;
  todo_t *all_todos = NULL;

  if (tree->is_dir) {
    // Обход подкаталогов
    printf("Обход подкаталогов %s\n", tree->path);
    for (size_t i = 0; i < tree->num_files; i++) {
      size_t sub_count = 0;
      todo_t *sub_todos = NULL;
      printf(">> %s\n", tree->content.files[i]->path);

      if (!collect_all_todos(tree->content.files[i], &sub_todos, &sub_count)) {
        printf("Ошибка при обходе");
        goto cleanup;
      }

      if (sub_count > 0) {
        printf("Найдено %d файлов\n", (int)sub_count);

        todo_t *tmp = (todo_t *)realloc(all_todos, (total_count + sub_count) *
                                                       sizeof(todo_t));
        if (!tmp) {
          printf("Ошибка при реаллоцировании памяти для sub %d файлов\n",
                 (int)(total_count + sub_count) * (int)sizeof(todo_t));

          for (size_t j = 0; j < sub_count; j++) {
            free(sub_todos[j].path);
            free(sub_todos[j].title);
            free(sub_todos[j].surround_content_before);
            free(sub_todos[j].surround_content_after);
          }
          free(sub_todos);
          goto cleanup;
        }
        all_todos = tmp;
        printf("Собрали инфу о сабах, копируем в all_todos\n");

        memcpy(all_todos + total_count, sub_todos, sub_count * sizeof(todo_t));
        printf("Скопировали в all_todos\n");

        total_count += sub_count;
        free(sub_todos);
        printf("Очистка прошла успешно\n");
      }
    }
  } else {
    printf("Начинаю обработка файла %s\n", tree->path);
    // Обработка файла
    size_t file_count = 0;
    todo_t *file_todos = NULL;
    if (!collect_todos_from_file(tree->path, tree->content.file_source,
                                 &file_todos, &file_count)) {
      printf("Ошибка при сборе тудушек\n");
      goto cleanup;
    }
    printf("Собрали тудушки с файла : count: %d, %s\n", (int)file_count,
           tree->path);

    all_todos = file_todos;
    total_count = file_count;
  }

  *todos = all_todos;
  *out_count = total_count;
  return 1;

cleanup:
  printf("Провалились в cleanup");
  for (size_t i = 0; i < total_count; i++) {
    free(all_todos[i].path);
    free(all_todos[i].title);
    free(all_todos[i].surround_content_before);
    free(all_todos[i].surround_content_after);
  }
  free(all_todos);
  *out_count = 0;
  return 0;
}

void print_todo_list(todo_t *list, int listc, int *active_index,
                     int *opened_index) {
  if (!list)
    return;
  for (int i = 0; i < listc; i++) {
    if (i == *active_index && *active_index == *opened_index) {
      printf("╠» [%s]\n", list[i].title);
    } else if (i == *active_index) {
      printf("╠» %s\n", list[i].title);
    } else {
      printf("║  %s\n", list[i].title);
    }
  }
}

void print_help(void) {
  printf("Usage: tdo [OPTIONS] <command>\n");
  printf("Commands:\n");
  printf("  list    - Show the file tree\n");
  printf("  export  - Export the file tree\n");
  printf("  add     - Add a file or directory\n");
  printf("  remove  - Remove a file or directory\n");
  printf("Options:\n");
  printf("  --dir <path>  - Specify the directory to process\n");
}
