#pragma once
#include "tdo_utils.h"

void render_loop(todo_t *list, int *active_index, int *opened_index,
                 int *start_index, int total_files, const char *editor);
