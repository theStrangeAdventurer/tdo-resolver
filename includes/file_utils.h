#pragma once
#include <stdio.h>

typedef struct file_tree file_tree_t;

struct file_tree {
  int is_dir;
  char *path;
  union {
    file_tree_t **files;
    char *file_source;
  } content;
  size_t num_files;
};

char **get_list_from_tree(file_tree_t *tree);

char *get_file_content(const char *path, size_t content_size);

size_t count_files_from_tree(file_tree_t *tree);

void free_file_tree(file_tree_t *tree);
void write_file(char *path, char *content);

char *construct_file_path(char *file_path, char *dir, char *file_name,
                          size_t max_len);

void print_file_list(char **list, int listc, int *active_index,
                     int *opened_index);

file_tree_t *get_file_tree(char *path, const char **ignore_dirs,
                           size_t ignore_dirs_count, const char **ignore_exts,
                           size_t ignore_exts_count);
