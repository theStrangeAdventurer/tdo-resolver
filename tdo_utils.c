#include "tdo_utils.h"
#include "emoji.h"
#include "file_utils.h"
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ANSI-color codes
#define COLOR_RESET "\033[0m"
#define COLOR_BLUE "\033[34m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"
#define BOLD_SET "\033[1m"
#define BOLD_RESET "\033[22m"
#define COLOR_DIMMED "\033[2m"
#define COLOR_DIMMED_RESET "\033[0m"
#define COLOR_DIMMED_DEPRECATED "\033[2;37m"
#define COLOR_DIMMED_DEPRECATED_RESET "\033[0;0m"

#define SURROUND_CONTEXT_LINES 5
#define ANIMATION_TIME 50000
#define PROGRESS_BAR_WIDTH 30

void setGreenColor() { printf(COLOR_GREEN); }
void resetColor() { printf(COLOR_RESET); }
void setDimmedDeprecatedColor() { printf(COLOR_DIMMED_DEPRECATED); }
void setDimmedColor() { printf(COLOR_DIMMED); }
void resetDimmedColor() { printf(COLOR_DIMMED_RESET); }
void resetDimmedDeprecatedColor() { printf(COLOR_DIMMED_DEPRECATED_RESET); }

char *escape_string(char *value) {
  if (!value)
    return NULL;

  size_t spec_symbols_count = 0;
  const char *p = value;
  while (*p) {
    if (*p == '"' || *p == '\\') {
      spec_symbols_count += 1;
    } else if ((unsigned char)*p <= 0x1F) {
      switch (*p) {
      case '\b':
      case '\f':
      case '\n':
      case '\r':
      case '\t':
        spec_symbols_count += 1; // \b, \f, \n, \r, \t
        break;
      default:
        spec_symbols_count += 5; // \uXXXX symbols
        break;
      }
    }
    p++;
  }
  char *result = malloc(strlen(value) + spec_symbols_count + 1);

  if (!result)
    return NULL;

  char *out = result;
  for (p = value;         // Reset pointer to start input str value
       *p != '\0'; p++) { // Go through the string until we meet null terminator

    unsigned char c = *p;
    if (c == '"') {
      *out++ = '\\';
      *out++ = '"'; // \" для кавычек
    } else if (c == '\\') {
      *out++ = '\\';
      *out++ = '\\';        // \\ для слеша
    } else if (c <= 0x1F) { // Управляющие символы
      *out++ = '\\';
      switch (c) {
      case '\b':
        *out++ = 'b';
        break;
      case '\f':
        *out++ = 'f';
        break;
      case '\n':
        *out++ = 'n';
        break;
      case '\r':
        *out++ = 'r';
        break;
      case '\t':
        *out++ = 't';
        break;
      default:
        snprintf(out, 7, "u%04X", c); // 0x01, 0x02, ...  to \uXXXX
        out += 6;
        break;
      }
    } else {
      *out++ = c; // Rest symbols
    }
  }
  *out = '\0';
  return result;
}

void open_source_in_editor(const char *editor, char *file_path,
                           int line_number) {
  char command[256];
  snprintf(command, sizeof(command), "%s +%d %s", editor, line_number,
           file_path);
  int result = system(command);

  if (result == -1) {
    perror("Error during 'open_source_in_editor' command execution");
    return;
  }
}
// Функция для подсчёта строк до позиции
static int get_line_number(const char *start, const char *pos) {
  int line = 1;
  for (const char *p = start; p < pos; p++) {
    if (*p == '\n')
      line++;
  }
  return line;
}

// Getting surround context
// pos - start or end cursor of the todo|fixme line
static char *get_context(const char *start, const char *pos, int lines_before,
                         int lines_after) {
  const char *p = pos;
  int current_line = 0;
  int is_direction_after = lines_after && !lines_before;
  int is_direction_before = lines_before && !lines_after;

  // Go back for surround before lines
  while (p > start && current_line < lines_before) {
    if (*p == '\n')
      current_line++;
    p--;
  }

  if (is_direction_after) {
    while (*p) {
      if (*p == '\n') {
        p++;
        break;
      }
      p++;
    }
  }

  if (is_direction_before) {
    while (*p) {
      if (*p == '\n') {
        p++;
        break;
      }
      p--;
    }
  }
  const char *context_start = p;

  // Go forward for surround after lines
  p = pos;
  current_line = 0;
  while (*p && current_line < lines_after) {
    if (*p == '\n')
      current_line++;
    p++;
  }

  if (is_direction_before) {
    while (*p) {
      if (*p == '\n')
        break;
      p--;
    }
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

static int collect_todos_from_file(const char *path, const char *file_content,
                                   todo_t **todos, size_t *out_count) {
  if (!file_content || !path) {
    *out_count = 0;
    return 0;
  }

  if (strstr(file_content, EXPORT_MARKER)) { // ignore export report file
    *out_count = 0;
    return -1;
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
    return -1;
  }

  // Выделение памяти под todos
  todo_t *result = (todo_t *)malloc(count * sizeof(todo_t));
  if (!result) {
    regfree(&regex);
    *out_count = 0;
    return 0;
  }

  pos = file_content;
  size_t i = 0;
  while (i < count) {
    ret = regexec(&regex, pos, 3, matches, 0);
    if (ret != 0)
      break;

    todo_t *todo = &result[i];
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

    while (*title_start && *title_start != '\n') {
      title_start--;
    }
    title_start++; // fix for unused \n before raw_title
    size_t title_raw_len = title_end - title_start;
    todo->raw_title = (char *)malloc(title_raw_len + 1);
    if (!todo->raw_title)
      goto cleanup;
    memcpy(todo->raw_title, title_start, title_raw_len);
    todo->raw_title[title_raw_len] = '\0';

    // Surround context >>
    todo->surround_content_before = get_context(
        file_content, pos + matches[0].rm_so, SURROUND_CONTEXT_LINES, 0);
    todo->surround_content_after = get_context(
        file_content, pos + matches[0].rm_eo, 0, SURROUND_CONTEXT_LINES);
    // << Surround context

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

// Вспомогательная функция для очистки
void free_todos_array(todo_t *todos, size_t count) {
  if (!todos)
    return;

  for (size_t i = 0; i < count; i++) {
    free(todos[i].path);
    free(todos[i].title);
    free(todos[i].raw_title);
    free(todos[i].surround_content_before);
    free(todos[i].surround_content_after);
  }
  free(todos);
}

int collect_all_todos(file_tree_t *tree, todo_t **todos, size_t *out_count) {

  // Проверка входных параметров
  if (!tree || !out_count) {
    *out_count = 0;
    fprintf(stderr, "Error: Invalid parameters\n");
    return 0;
  }
  static int depth = 0;     // Для отслеживания глубины рекурсии
  const int MAX_DEPTH = 32; // Лимит для защиты от переполнения стека

  if (depth > MAX_DEPTH) {
    fprintf(stderr, "Error: Maximum recursion depth exceeded\n");
    *out_count = 0;
    return 0;
  }

  depth++;

  size_t total_count = 0;
  todo_t *all_todos = NULL;
  int ret = 1; // Флаг успеха

  if (tree->is_dir) {
    for (size_t i = 0; i < tree->num_files; i++) {
      size_t sub_count = 0;
      todo_t *sub_todos = NULL;

      if (!collect_all_todos(tree->content.files[i], &sub_todos, &sub_count)) {
        fprintf(stderr, "Error processing subdirectory\n");
        ret = 0;
        continue; // Продолжаем обработку других файлов
      }

      if (sub_count > 0) {
        todo_t *tmp =
            realloc(all_todos, (total_count + sub_count) * sizeof(todo_t));
        if (!tmp) {
          fprintf(stderr, "Memory allocation failed\n");
          free_todos_array(sub_todos, sub_count);
          ret = 0;
          break;
        }

        all_todos = tmp;
        // Копируем только структуры, не указатели!
        for (size_t j = 0; j < sub_count; j++) {
          all_todos[total_count + j] = sub_todos[j];
        }
        total_count += sub_count;
        // Не освобождаем sub_todos, так как данные теперь в all_todos
        free(sub_todos); // Освобождаем только массив структур
      }
    }
  } else {
    if (!collect_todos_from_file(tree->path, tree->content.file_source,
                                 &all_todos, &total_count)) {
      fprintf(stderr, "Error processing file: %s\n", tree->path);
      ret = 0;
    }
  }

  depth--;

  if (ret) {
    *todos = all_todos;
    *out_count = total_count;
  } else {
    free_todos_array(all_todos, total_count);
    *out_count = 0;
  }

  return ret;
}
void print_banner() {
  setbuf(stdout, NULL);
  setGreenColor();
  printf("\n"
         "  ████████╗██████╗  ██████╗\n"
         "  ╚══██╔══╝██╔══██╗██╔═══██╗\n"
         "     ██║   ██║  ██║██║   ██║\n"
         "     ██║   ██║  ██║██║   ██║\n"
         "     ██║   ██████╔╝╚██████╔╝\n"
         "     ╚═╝   ╚═════╝  ╚═════╝\n");
  printf("  >");
  usleep(ANIMATION_TIME);
  printf(">");
  usleep(ANIMATION_TIME);
  printf(">");
  usleep(ANIMATION_TIME);
  printf(" G");
  usleep(ANIMATION_TIME);
  printf("e");
  usleep(ANIMATION_TIME);
  printf("t ");
  usleep(ANIMATION_TIME);
  printf("S");
  usleep(ANIMATION_TIME);
  printf("h");
  usleep(ANIMATION_TIME);
  printf("i");
  usleep(ANIMATION_TIME);
  printf("t ");
  usleep(ANIMATION_TIME);
  printf("D");
  usleep(ANIMATION_TIME);
  printf("o");
  usleep(ANIMATION_TIME);
  printf("n");
  usleep(ANIMATION_TIME);
  printf("e");
  usleep(ANIMATION_TIME);
  printf(", ");
  usleep(ANIMATION_TIME);
  printf("N");
  usleep(ANIMATION_TIME);
  printf("o ");
  usleep(ANIMATION_TIME);
  printf("F");
  usleep(ANIMATION_TIME);
  printf("u");
  usleep(ANIMATION_TIME);
  printf("s");
  usleep(ANIMATION_TIME);
  printf("s");
  usleep(ANIMATION_TIME);
  printf(" <");
  usleep(ANIMATION_TIME);
  printf("<");
  usleep(ANIMATION_TIME);
  printf("<\n\n");
  usleep(ANIMATION_TIME);
  resetColor();
}

void print_space_hotkey() {

  // Второй ряд для Space
  printf(COLOR_YELLOW
         " ╔════════════════════════════════════════════╗\n" COLOR_RESET);
  printf(COLOR_YELLOW
         " ║ Space: open todo in your $EDITOR           ║\n" COLOR_RESET);
  printf(COLOR_YELLOW
         " ╚════════════════════════════════════════════╝\n" COLOR_RESET);
}
void print_hotkeys() {
  // Плашки с хоткеями: первый ряд
  printf(COLOR_BLUE " ╔════════════════╗ " COLOR_GREEN
                    "╔════════════════╗ " COLOR_YELLOW
                    "╔════════════════╗\n" COLOR_RESET);
  printf(COLOR_BLUE " ║ ↑/↓/J/K: Move  ║ " COLOR_GREEN
                    "║ Enter/L: Open  ║ " COLOR_YELLOW
                    "║ Q: Quit        ║\n" COLOR_RESET);
  printf(COLOR_BLUE " ╚════════════════╝ " COLOR_GREEN
                    "╚════════════════╝ " COLOR_YELLOW
                    "╚════════════════╝\n" COLOR_RESET);
  print_space_hotkey();
  printf(COLOR_BLUE);
  printf("\n  >> Press [Enter] to proceed...");
  printf(COLOR_RESET);
}

char *replace_newlines(char *src) {
  // Считаем количество \n
  size_t count = 0;
  const char *tmp = src;
  while ((tmp = strchr(tmp, '\n')) != NULL) {
    count++;
    tmp++;
  }

  char *result =
      malloc(strlen(src) + count * 5 +
             1); // 2 spaces + 3 bytes for unicode + 1 byte for terminator
  if (!result)
    return NULL;

  char *dst = result;
  while (*src) {
    if (*src == '\n') {
      *dst++ = '\n';    // Сохраняем исходный \n
      *dst++ = ' ';     // Пробел
      strcpy(dst, "║"); // Корректно для UTF-8
      dst += 3;
      *dst++ = ' '; // Пробел
      src++;
    } else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';
  return result;
}

// ANSI escape codes
// https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797
void print_todo_list(todo_t *list, int listc, int *active_index,
                     int *opened_index, int *start_index,
                     int should_skip_render_banner) {
  if (!should_skip_render_banner) {
    print_banner();
    print_hotkeys();
    return;
  }
  setGreenColor();
  if (!list) {
    printf("  Empty pointer to list, nothing to render...");
    resetColor();
    return;
  }

  int _opened = *opened_index;
  int _start = *start_index;
  int _end =
      (_start + MAX_RENDER_ITEMS) > listc ? listc : _start + MAX_RENDER_ITEMS;
  printf(" %s %s\n", random_emoji(), random_phrase());
  char *has_items_before_start_item = _start > 0 ? "▴" : "▬";
  char *has_items_after_end_item = _end < listc - 1 ? "▾" : "▬";
  printf(" %s\n", has_items_before_start_item);
  for (int i = _start; i < _end; i++) {
    if (i == *active_index && *active_index == *opened_index) {
      printf(" ║▸[%s]\n", list[i].title);
      setGreenColor();
    } else if (i == *active_index) {
      printf(" ║▸ %s\n", list[i].title);
    } else {
      printf(" ║ %s\n", list[i].title);
    }
  }
  printf(" %s\n", has_items_after_end_item);
  setDimmedColor();
  printf(" %s:%d \n", prettify_path(list[*active_index].path),
         list[*active_index].line);

  print_unicode_progress(*active_index + 1, listc, PROGRESS_BAR_WIDTH);
  resetDimmedColor();
  setGreenColor();
  printf("\n");
  if (_opened != -1) {
    resetColor();
    setDimmedDeprecatedColor();
    printf(" ▬\n");
    printf(" ║ %s\n", replace_newlines(list[_opened].surround_content_before));
    resetDimmedDeprecatedColor();
    printf(BOLD_SET);
    printf(" ║ %s\n", list[_opened].raw_title);
    printf(BOLD_RESET);
    setDimmedDeprecatedColor();
    printf(" ║ %s\n", replace_newlines(list[_opened].surround_content_after));
    printf(" ▬\n");
    resetDimmedDeprecatedColor();
    print_space_hotkey();
  }
  resetColor();
}

char *get_todos_json(todo_t *todos, size_t todos_num) {

  size_t json_len = 0;
  char json_start[100];
  char json_end[] = "]\n}";

  sprintf(json_start, "{ \"%s\": true,\"count\": %d,\"data\": [", EXPORT_MARKER,
          (int)todos_num);
  for (size_t i = 0; i < todos_num; i++) {
    // >>>
    char *escaped_title = escape_string(todos[i].title);
    json_len += strlen("{\"title\":");
    json_len += strlen(escaped_title);
    json_len += 3; // for title quotes and comma
                   // <<<
    // >>>
    json_len += strlen("{\"path\":");
    json_len += strlen(todos[i].path);
    json_len += 3; // for path quotes and comma
                   // <<<
    // >>>
    json_len += strlen("{\"line\":");
    json_len += sizeof(todos[i].line);
    json_len += 1; // for line comma
                   // <<<
    char *escaped_context_before =
        escape_string(todos[i].surround_content_before);
    char *escaped_context_after =
        escape_string(todos[i].surround_content_after);
    // >>>
    json_len += strlen("{\"surround_content_before\":") * 2;
    json_len += strlen(escaped_context_before);
    json_len += 3; // for quotes and comma
                   // <<<

    // >>>
    json_len += strlen("{\"surround_content_after\":");
    json_len += strlen(escaped_context_after);
    json_len += 3; // for quotes and comma
                   // <<<

    json_len += 1; // }
    if (i != (todos_num - 1)) {
      json_len += 1; // for whole item comma
    }
    free(escaped_context_after);
    free(escaped_context_before);
    free(escaped_title);
  }
  size_t json_start_len = strlen(json_start);
  size_t json_end_len = strlen(json_end);
  json_len += json_start_len;
  json_len += json_end_len;

  char *result = (char *)malloc(json_len * sizeof(char));
  if (!result)
    return NULL;
  char *ptr = result; // Бегунок, который будет бегать по строке
  ptr += sprintf(ptr, "%s",
                 json_start); // Записываем json_start и двигаем бегунок
  for (size_t i = 0; i < todos_num; i++) {
    char *escaped_context_before =
        escape_string(todos[i].surround_content_before);
    char *escaped_context_after =
        escape_string(todos[i].surround_content_after);
    char *escaped_title = escape_string(todos[i].title);

    ptr += sprintf(ptr, "{\"title\":\"%s\"", escaped_title);
    ptr += sprintf(ptr, ",\"surround_content_before\":\"%s\"",
                   escaped_context_before);
    ptr += sprintf(ptr, ",\"surround_content_after\":\"%s\"",
                   escaped_context_after);
    ptr += sprintf(ptr, ",\"path\":\"%s\"", todos[i].path);
    ptr += sprintf(ptr, ",\"line\":%d", todos[i].line);
    ptr += sprintf(ptr, "}");
    free(escaped_context_after);
    free(escaped_context_before);
    free(escaped_title);
    if (i != (todos_num - 1)) {
      ptr += sprintf(ptr, ",");
    }
  }
  sprintf(ptr, "%s", json_end);
  return result;
}
