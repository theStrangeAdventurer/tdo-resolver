#include "emoji.h"
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wchar.h>

const char *random_emoji() {
  static const char *emojis[] = {"(◕‿◕)", "(◕ᴗ◕)", "(◉‿◉)", "(◡‿◡)"};

  static int initialized = 0;
  if (!initialized) {
    srand(time(0));
    initialized = 1;
  }

  int random_index = rand() % 4;
  return emojis[random_index];
}

void print_unicode_progress(int current, int total, int bar_width) {
  printf(">> %d/%d/%d\n", current, total, bar_width);
  // Устанавливаем локаль для поддержки Unicode
  if (setlocale(LC_ALL, "en_US.UTF-8") == NULL) {
    // Если не получилось установить UTF-8 локаль, пробуем C.UTF-8 или пустую
    setlocale(LC_ALL, "C.UTF-8");
  }

  // Проверяем, нужно ли вообще показывать прогресс
  if (total <= 0 || current < 0 || current > total) {
    wprintf(L"[Invalid progress]\n");
    return;
  }

  // Рассчитываем заполненную часть
  float progress = (float)current / total;
  int filled = (int)(progress * bar_width);

  // Печатаем прогресс-бар
  wprintf(L"[");
  for (int i = 0; i < bar_width; i++) {
    if (i < filled) {
      wprintf(L"█"); // Полный блок (U+2588)
    } else if (i == filled) {
      wprintf(L"▌"); // Полублок (U+258C)
    } else {
      wprintf(L" "); // Пустое пространство
    }
  }
  wprintf(L"] %d%% (%d/%d)\n", (int)(progress * 100), current, total);
}
