#include "emoji.h"
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

const char *random_phrase() {
  // Массив с фразами
  static const char *phrases[] = {"Todos await!", "No procrastination!",
                                  "Tasks on fire!", "Go, hero!", "List calls!"};
  // Количество фраз
  const int num_phrases = sizeof(phrases) / sizeof(phrases[0]);

  // Инициализация генератора случайных чисел (вызывать один раз в программе)
  static int initialized = 0;
  if (!initialized) {
    srand(time(NULL));
    initialized = 1;
  }

  // Возвращаем случайную фразу
  return phrases[rand() % num_phrases];
}

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
  // Устанавливаем локаль для UTF-8
  setlocale(LC_ALL, "");

  // Проверка на валидность входных данных
  if (total <= 0 || current < 0 || current > total) {
    printf("[Invalid progress]\n");
    return;
  }

  // Рассчитываем прогресс
  float progress = (float)current / total;
  int filled = progress * bar_width;

  // UTF-8 символы для прогресс-бара
  const char *full_block = "┉";
  const char *partial_block = "┉";
  const char *empty_space = " "; // Печатаем прогресс-бар
                                 //
  printf(" [");
  for (int i = 0; i < bar_width; i++) {
    if (i < filled) {
      printf("%s", full_block);
    } else if (i == filled) {
      printf("%s", partial_block);
    } else {
      printf("%s", empty_space);
    }
  }
  printf("] %d%% (%d/%d)\n", (int)(progress * 100), current, total);
}
