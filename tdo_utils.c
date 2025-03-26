#include "tdo_utils.h"
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
#define COLOR_DIMMED "\033[2;37m"
#define COLOR_DIMMED_RESET "\033[0;0m"

#define SURROUND_CONTEXT_LINES 5
#define ANIMATION_TIME 50000

void setGreenColor() { printf(COLOR_GREEN); }
void resetColor() { printf(COLOR_RESET); }
void setDimmedColor() { printf(COLOR_DIMMED); }
void resetDimmedColor() { printf(COLOR_DIMMED_RESET); }

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
int collect_all_todos(file_tree_t *tree, todo_t **todos, size_t *out_count) {
  if (!tree || !out_count) {
    *out_count = 0;
    perror("collect_all_todos incorrent pointer or empty out");
    return 0;
  }

  size_t total_count = 0;
  todo_t *all_todos = NULL;

  if (tree->is_dir) {
    // Обход подкаталогов
    for (size_t i = 0; i < tree->num_files; i++) {
      size_t sub_count = 0;
      todo_t *sub_todos = NULL;

      if (!collect_all_todos(tree->content.files[i], &sub_todos, &sub_count)) {
        perror("collect_all_todos cannot collect sub todos");
        goto cleanup;
      }

      if (sub_count > 0) {
        todo_t *tmp = (todo_t *)realloc(all_todos, (total_count + sub_count) *
                                                       sizeof(todo_t));
        if (!tmp) {
          perror("collect_all_todos realloc");
          for (size_t j = 0; j < sub_count; j++) {
            free(sub_todos[j].path);
            free(sub_todos[j].title);
            free(sub_todos[j].raw_title);
            free(sub_todos[j].surround_content_before);
            free(sub_todos[j].surround_content_after);
          }
          free(sub_todos);
          goto cleanup;
        }
        all_todos = tmp;
        memcpy(all_todos + total_count, sub_todos, sub_count * sizeof(todo_t));
        total_count += sub_count;
        free(sub_todos);
      }
    }
  } else {
    // Обработка файла
    size_t file_count = 0;
    todo_t *file_todos = NULL;
    int collect_result = collect_todos_from_file(
        tree->path, tree->content.file_source, &file_todos, &file_count);
    if (collect_result == 0) {
      perror("collect_all_todos collect_todos_from_file");
      goto cleanup;
    }
    if (collect_result == -1) {
      *out_count = 0; // File without todos
      return 1;
    }
    all_todos = file_todos;
    total_count = file_count;
  }

  *todos = all_todos;
  *out_count = total_count;
  return 1;

cleanup:
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
                     int *opened_index, int should_skip_render_banner) {
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

  printf(" ╔═══»\n");

  for (int i = 0; i < listc; i++) {
    if (i == *active_index && *active_index == *opened_index) {
      printf(" ╠»[%s]\n", list[i].title);

      setGreenColor();
    } else if (i == *active_index) {
      printf(" ╠» %s\n", list[i].title);
    } else {
      printf(" ║ %s\n", list[i].title);
    }
  }
  printf(" ╚════»\n");

  if (_opened != -1) {
    resetColor();
    setDimmedColor();
    printf(" ╔═══ %s:%d \n", prettify_path(list[_opened].path),
           list[_opened].line);
    printf(" ║ %s\n", replace_newlines(list[_opened].surround_content_before));
    resetDimmedColor();
    printf(BOLD_SET);
    printf(" ║ %s\n", list[_opened].raw_title);
    printf(BOLD_RESET);
    setDimmedColor();
    printf(" ║ %s\n", replace_newlines(list[_opened].surround_content_after));
    printf(" ╚═══ \n");
    resetDimmedColor();
    print_space_hotkey();
  }
  resetColor();
}

void print_help(void) {
  printf("Usage: tdo [OPTIONS] <command>\n");
  printf("Options:\n");
  printf("  --dir <path>  - Specify the directory to process\n");
  printf("  --export <path>  - Specify the json file export path\n");
}
