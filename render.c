#include "render.h"
#include "tdo_utils.h"
#include "term.h"
#include <stdio.h>
#include <unistd.h>

int global_skip_banner = 0;

void render_loop(todo_t *list, int *active_index, int *opened_index,
                 int *start_index, int total_files, const char *editor) {
  print_todo_list(list, total_files, active_index, opened_index, start_index,
                  global_skip_banner);
  fflush(stdout);
  char c;
  while (1) {
    if (read(STDIN_FILENO, &c, 1) == 1) {
      if (c == '\033') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
          continue;
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
          continue;
        if (seq[0] == '[') {
          if (seq[1] == 'A' && *active_index > 0) {
            (*active_index)--;
            if (*active_index < *start_index) {
              (*start_index)--;
            }
          } else if (seq[1] == 'B' && (*active_index < (total_files - 1))) {
            (*active_index)++;
            if (*active_index > (*start_index + MAX_RENDER_ITEMS)) {
              (*start_index)++;
            }
          }
          clear_screen();
          print_todo_list(list, total_files, active_index, opened_index,
                          start_index, global_skip_banner);

          fflush(stdout);
        }
      } else if (c == 'j' &&
                 *active_index < total_files - 1) { // Клавиша j (вниз)
        (*active_index)++; // TODO: move increment & decrement to separate
                           // functions
        if (*active_index < *start_index) {
          (*start_index)--;
        }
        clear_screen();
        print_todo_list(list, total_files, active_index, opened_index,
                        start_index, global_skip_banner);
      } else if (c == 'k' && *active_index > 0) { // Клавиша k (вверх)
        (*active_index)--;
        if (*active_index > (*start_index + MAX_RENDER_ITEMS)) {
          (*start_index)++;
        }
        clear_screen();
        print_todo_list(list, total_files, active_index, opened_index,
                        start_index, global_skip_banner);
        fflush(stdout);
      } else if (c == '\n' || c == '\r' || c == 'l') { // Enter (\r) или l
        if (!global_skip_banner) {
          global_skip_banner = 1;
        } else if (*opened_index == *active_index) {
          *opened_index = -1;
        } else {
          *opened_index = *active_index; // Устанавливаем opened_index равным
        }
        clear_screen();
        print_todo_list(list, total_files, active_index, opened_index,
                        start_index, global_skip_banner);
        fflush(stdout);

      } else if (c == 'q') { // exit
        break;
      } else if (c == ' ') {
        if (*opened_index != -1 && *opened_index) {
          open_source_in_editor(editor, list[*opened_index].path,
                                (list[*opened_index].line));
          *opened_index = -1;
          clear_screen();
          print_todo_list(list, total_files, active_index, opened_index,
                          start_index, global_skip_banner);
          fflush(stdout);
        }
      }
    }
  }
}
