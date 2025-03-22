#include "tdo_utils.h"
#include "term.h"
#include <unistd.h>

void render_loop(todo_t *list, int *active_index, int *opened_index,
                 int total_files) {
  print_todo_list(list, total_files, active_index, opened_index);
  fflush(stdout);
  char c;
  while (1) {
    if (read(STDIN_FILENO, &c, 1) == 1) {
      printf("PRESSED %d\n", c);
      if (c == '\033') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
          continue;
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
          continue;
        if (seq[0] == '[') {
          if (seq[1] == 'A' && *active_index > 0)
            (*active_index)--;
          else if (seq[1] == 'B' && (*active_index < (total_files - 1)))
            (*active_index)++;
          clear_screen();
          print_todo_list(list, total_files, active_index, opened_index);

          fflush(stdout);
        }
      } else if (c == 'j' &&
                 *active_index < total_files - 1) { // Клавиша j (вниз)
        (*active_index)++;
        clear_screen();
        print_todo_list(list, total_files, active_index, opened_index);
      } else if (c == 'k' && *active_index > 0) { // Клавиша k (вверх)
        (*active_index)--;
        clear_screen();
        print_todo_list(list, total_files, active_index, opened_index);
        fflush(stdout);
      } else if (c == '\n' || c == '\r' || c == 'l') { // Enter (\r) или l
        if (*opened_index == *active_index) {
          *opened_index = -1;
        } else {
          *opened_index = *active_index; // Устанавливаем opened_index равным
        }
        // текущему active_index
        clear_screen();
        print_todo_list(list, total_files, active_index, opened_index);
        fflush(stdout);
      } else if (c == 'q') { // Выход
        break;
      }
    }
  }
}
