#define _GNU_SOURCE
#include <ftw.h>
#include "ui_functions.h"
#include <sys/wait.h>
#include <fcntl.h>
#include <archive.h>
#include <archive_entry.h>
#ifdef OPENSSL_PRESENT
#include <openssl/sha.h>
#include <openssl/md5.h>
#endif
#ifdef LIBX11_PRESENT
#include <X11/Xlib.h>
#endif
#ifdef LIBCUPS_PRESENT
#include <cups/cups.h>
#endif

#define MAX_NUMBER_OF_FOUND 100
#define BUFF_SIZE 8192
#define CANNOT_PASTE_SAME_DIR -2
#define MOVED_FILE -1

void change_dir(const char *str);
void switch_hidden(void);
void manage_file(const char *str);
void new_file(void);
void remove_file(void);
void manage_c_press(char c);
void paste_file(void);
void rename_file_folders(void);
void create_dir(void);
void search(void);
void list_found(void);
void search_loop(void);
#ifdef LIBCUPS_PRESENT
void print_support(char *str);
#endif
void create_archive(void);
#ifdef OPENSSL_PRESENT
void integrity_check(const char *str);
#endif
void change_tab(void);