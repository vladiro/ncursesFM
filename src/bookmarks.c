#include "../inc/bookmarks.h"

static void get_xdg_dirs(void);
static void remove_bookmark(void);
static void update_bookmarks_tabs(void);

static int num_bookmarks, xdg_bookmarks;
static const char *bookmarks_file = "/.config/ncursesFM-bookmarks";
static char home_dir[PATH_MAX + 1];
static char fullpath[PATH_MAX + 1];
static char (bookmarks[MAX_BOOKMARKS])[PATH_MAX + 1];

void get_bookmarks(void) {
    FILE *f;
    
    strncpy(home_dir, getpwuid(getuid())->pw_dir, PATH_MAX);
    snprintf(fullpath, PATH_MAX, "%s%s", home_dir, bookmarks_file);
    get_xdg_dirs();
    if (num_bookmarks < MAX_BOOKMARKS) {
        if ((f = fopen(fullpath, "r"))) {
            int i = num_bookmarks;
            while (fscanf(f, "%s", bookmarks[i]) == 1 && i < MAX_BOOKMARKS) {
                i++;
            }
            fclose(f);
            if (i == MAX_BOOKMARKS) {
                WARN(too_many_bookmarks);
            }
            num_bookmarks = i;
        } else {
            WARN(bookmarks_file_err);
        }
    }
}

static void get_xdg_dirs(void) {
    int i = 0;
    FILE *f;
    char str[PATH_MAX + 1] = {0};
    char line[1000], file_path[PATH_MAX + 1];
    const char path[] = "/.config/user-dirs.dirs";

    snprintf(file_path, PATH_MAX, "%s%s", home_dir, path);
    if ((f = fopen(file_path, "r"))) {
        while (fgets(line, sizeof(line), f) && i < MAX_BOOKMARKS) {
            if (*line == '#') {
                continue;
            }
            strncpy(str, strchr(line, '/') + 1, PATH_MAX);
            str[strlen(str) - 2] = '\0'; // -1 for newline - 1 for closing Double quotation mark
            snprintf(bookmarks[i], PATH_MAX, "%s/%s", home_dir, str);
            i++;
        }
        fclose(f);
        if (i == MAX_BOOKMARKS) {
            WARN(too_many_bookmarks);
        }
        num_bookmarks = i;
        xdg_bookmarks = i;
    }
}

void add_file_to_bookmarks(const char *str) {
    FILE *f;
    char c;

    ask_user(_(bookmarks_add_quest), &c, 1);
    if (quit || c == _(no)[0] || c == 27) {
        return;
    }
    if ((f = fopen(fullpath, "a+"))) {
        fprintf(f, "%s\n", str);
        fclose(f);
        print_info(bookmark_added, INFO_LINE);
        if (num_bookmarks < MAX_BOOKMARKS) {
            strncpy(bookmarks[num_bookmarks], str, PATH_MAX);
            num_bookmarks++;
            update_bookmarks_tabs();
        } else {
            print_info(too_many_bookmarks, ERR_LINE);
        }
    } else {
        print_info(bookmarks_file_err, ERR_LINE);
    }
}

void remove_bookmark_from_file(void) {
    char c;

    if (ps[active].curr_pos < xdg_bookmarks) {
        print_info(bookmarks_xdg_err, ERR_LINE);
    } else {
        ask_user(_(bookmarks_rm_quest), &c, 1);
        if (quit || c ==  _(no)[0] || c == 27) {
            return;
        }
        remove_bookmark();
    }
}

static void remove_bookmark(void) {
    FILE *f;
    int i = ps[active].curr_pos;
    
    if ((f = fopen(fullpath, "w"))) {
        memmove(&bookmarks[i], &bookmarks[i + 1], (num_bookmarks - 1 - i) * sizeof(bookmarks[0]));
        num_bookmarks--;
        print_info(bookmarks_rm, INFO_LINE);
        for (i = xdg_bookmarks; i < num_bookmarks; i++) {
            fprintf(f, "%s\n", bookmarks[i]);
        }
        fclose(f);
        update_bookmarks_tabs();
    } else {
        print_info(bookmarks_file_err, ERR_LINE);
    }
}

static void update_bookmarks_tabs(void) {
    for (int i = 0; i < cont; i++) {
        if (ps[i].mode == bookmarks_) {
            update_special_mode(num_bookmarks, i, bookmarks);
        }
    }
}

void show_bookmarks(void) {
    if (num_bookmarks) {
        show_special_tab(num_bookmarks, bookmarks, bookmarks_mode_str, bookmarks_);
    } else {
        print_info(no_bookmarks, INFO_LINE);
    }
}

void manage_enter_bookmarks(struct stat current_file_stat) {
    char c;
    char str[PATH_MAX + 1];
    
    if (access(str_ptr[active][ps[active].curr_pos], F_OK ) != -1 ) {
        strncpy(str, str_ptr[active][ps[active].curr_pos], PATH_MAX);
        if (!S_ISDIR(current_file_stat.st_mode)) {
            strncpy(ps[active].old_file, strrchr(str_ptr[active][ps[active].curr_pos], '/') + 1, PATH_MAX);
            int len = strlen(str_ptr[active][ps[active].curr_pos]) - strlen(ps[active].old_file);
            str[len] = '\0';
        } else {
            memset(ps[active].old_file, 0, strlen(ps[active].old_file));
        }
        leave_special_mode(str);
    } else {
        ask_user(_(inexistent_bookmark), &c, 1);
        if (!quit && c != _(no)[0] && c != 27) {
            remove_bookmark();
        }
    }
}
