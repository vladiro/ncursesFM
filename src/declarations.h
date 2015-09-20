#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include "string_constants.h"

#define INITIAL_POSITION 1
#define MAX_TABS 2
#define MAX_NUMBER_OF_FOUND 100

#define INFO_LINE 0
#define ERR_LINE 1

#define MOVE_TH 0
#define PASTE_TH 1
#define RM_TH 2
#define ARCHIVER_TH 3
#define NEW_FILE_TH 4
#define CREATE_DIR_TH 5
#define RENAME_TH 6
#define EXTRACTOR_TH 7
#define FUSEISO_TH 8

#define NO_REFRESH 0
#define REFRESH 1   // the win may need a refresh, but we have to check whether it is really needed before.
#define FORCE_REFRESH 2     // the win surely needs a refresh. Skip check, it is useless, and refresh.

struct conf {
    char editor[PATH_MAX];
    int show_hidden;
    char starting_dir[PATH_MAX];
    int second_tab_starting_dir;
    int inhibit;
};

typedef struct list {
    char name[PATH_MAX];
    struct list *next;
} file_list;

struct vars {
    int curr_pos;
    char my_cwd[PATH_MAX];
    char (*nl)[PATH_MAX];
    int number_of_files;
    int needs_refresh;
};

struct search_vars {
    char searched_string[20];
    char found_searched[MAX_NUMBER_OF_FOUND][PATH_MAX];
    int searching;
    int search_archive;
    int found_cont;
};

typedef struct thread_list {
    file_list *selected_files;
    int (*f)(void);
    char full_path[PATH_MAX];
    char filename[PATH_MAX];
    struct thread_list *next;
    int num;
    int type;
} thread_job_list;

thread_job_list *thread_h;
file_list *selected;
struct conf config;
struct vars ps[MAX_TABS];
struct search_vars sv;
int active, quit, num_of_jobs, cont;