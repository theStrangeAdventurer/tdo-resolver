#include <stdio.h>
#include <termios.h>
#include <unistd.h>

static struct termios orig_termios; // Статическая глобальная переменная

void clear_screen() { printf("\033[2J\033[1;1H"); }

void enable_raw_mode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  struct termios raw = orig_termios;
  // ECHO: Флаг, который включает отображение (эхо) вводимых символов на экране.
  // ICANON: Флаг, который включает канонический режим (обработку ввода
  // построчно с ожиданием Enter).
  raw.c_lflag &= ~(ECHO | ICANON |
                   ICRNL); // c_lflag — это поле структуры
                           // представляющее собой битовую маску
                           // объединяет значения флагов ECHO и ICANON в одну
                           // битовую маску применяет маску ~(ECHO | ICANON) к
                           // текущему значению raw.c_lflag, оставляя все биты,
                           // кроме ECHO и ICANON, без изменений, а эти два бита
                           // сбрасывает в 0. ICRNL - нужен чтобы нажатие Enter
                           // интерпретировалось как \r а не \n

  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // Применение настроек
}

void disable_raw_mode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }
