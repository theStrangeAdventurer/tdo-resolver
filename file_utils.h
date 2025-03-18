#pragma once
#include <stdio.h>

typedef struct file_tree file_tree_t;

struct file_tree {
  int is_dir;
  char *name;
  char *path;
  union {
    file_tree_t *files;
    char *file_source;
  } content;
  size_t num_files;
};

char **get_list_from_tree(file_tree_t *tree);

char *get_file_content(const char *path, size_t content_size);

size_t count_files_from_tree(file_tree_t *tree);

void free_file_tree(file_tree_t *tree);

void print_file_list(char **list, int listc, int *active_index);
void print_file_tree(file_tree_t *tree, int depth);

file_tree_t *get_file_tree(char *path, const char **ignore_dirs,
                           size_t ignore_dirs_count, const char **ignore_exts,
                           size_t ignore_exts_count,
                           const char **require_strings,
                           size_t require_strings_count);
