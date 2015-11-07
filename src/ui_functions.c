#include "../inc/ui_functions.h"

static void fm_scr_init(void);
static void generate_list(int win);
static void print_border_and_title(int win);
static int is_hidden(const struct dirent *current_file);
static void initialize_tab_cwd(int win);
static void scroll_helper_func(int x, int direction);
static void colored_folders(int win, const char *name);
static void helper_print(void);
static void create_helper_win(void);
static void remove_helper_win(void);
static void show_stat(int init, int end, int win);
static void change_unit(float size, char *str);
static void erase_stat(void);

struct scrstr {
    WINDOW *fm;
    int width;
    int delta;
    int stat_active;
    char tot_size[30];
};

static struct scrstr mywin[MAX_TABS];
static WINDOW *helper_win, *info_win;
static int dim, asking_question, resizing;
static int (*sorting_func)(const struct dirent **d1, const struct dirent **d2) = alphasort; // file list sorting function, defaults to alphasort

/*
 * Initializes screen, colors etc etc, and calls fm_scr_init.
 */
void screen_init(void) {
    setlocale(LC_ALL, "");
    initscr();
    start_color();
    init_pair(1, COLOR_BLUE, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_CYAN, COLOR_BLACK);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);
    raw();
    noecho();
    curs_set(0);
    cont = 1;
    fm_scr_init();
}

/*
 * Used to initialize fm win and info_win at program startup and after a resize.
 */
static void fm_scr_init(void) {
    int i;

    dim = LINES - INFO_HEIGHT;
    for (i = 0; i < cont; i++) {
        new_tab(i);
        if (resizing) {
            if (ps[i].curr_pos > dim - 2 - INITIAL_POSITION) {
                mywin[i].delta = ps[i].curr_pos - (dim - 2 - INITIAL_POSITION);
            } else {
                mywin[i].delta = 0;
            }
            if (mywin[i].stat_active == STATS_IDLE) {
                mywin[i].stat_active = STATS_ON;
            }
            list_everything(i, mywin[i].delta, 0);
        }
    }
    info_win = subwin(stdscr, INFO_HEIGHT, COLS, dim, 0);
    keypad(info_win, TRUE);
}

/*
 * Clear any existing window, and, if quitting (ie: not resizing) remove stdscr
 */
void screen_end(void) {
    int i;

    for (i = 0; i < cont; i++) {
        delete_tab(i);
    }
    if (helper_win) {
        wclear(helper_win);
        delwin(helper_win);
    }
    wclear(info_win);
    delwin(info_win);
    endwin();
    if (quit) {
        delwin(stdscr);
    }
}

/*
 * Creates a list of strings from current win path's files and print them to screen (list_everything)
 * If program cannot allocate memory, it will leave.
 */
static void generate_list(int win) {
    int i;
    struct dirent **files;
    char str[PATH_MAX] = {0};
    
    if (mywin[win].stat_active) {
        memset(mywin[win].tot_size, 0, strlen(mywin[win].tot_size));
        mywin[win].stat_active = STATS_ON;
    }
    ps[win].number_of_files = scandir(ps[win].my_cwd, &files, is_hidden, sorting_func);
    free(ps[win].nl);
    if (!(ps[win].nl = calloc(ps[win].number_of_files, PATH_MAX))) {
        quit = MEM_ERR_QUIT;
    }
    str_ptr[win] = ps[win].nl;
    if (strcmp(ps[win].my_cwd, "/") != 0) {
        strcpy(str, ps[win].my_cwd);
    }
    for (i = 0; i < ps[win].number_of_files; i++) {
        if (!quit) {
            sprintf(ps[win].nl[i], "%s/%s", str, files[i]->d_name);
        }
        free(files[i]);
    }
    free(files);
    if (!quit) {
        reset_win(win);
        list_everything(win, 0, 0);
    }
}

int sizesort(const struct dirent **d1, const struct dirent **d2) {
    struct stat stat1, stat2;
    float result;
    
    stat((*d1)->d_name, &stat1);
    stat((*d2)->d_name, &stat2);
    result = stat1.st_size - stat2.st_size;
    return (result > 0) ? -1 : 1;
}

int last_mod_sort(const struct dirent **d1, const struct dirent **d2) {
    struct stat stat1, stat2;
    
    stat((*d1)->d_name, &stat1);
    stat((*d2)->d_name, &stat2);
    return (stat2.st_mtime - stat1.st_mtime);
}

void reset_win(int win)
{
    wclear(mywin[win].fm);
    mywin[win].delta = 0;
    ps[win].curr_pos = 0;
}

/*
 * Prints to window 'win' "end" strings, startig from old_dim.
 *  If end == 0, it means it needs to print every string until the end of available rows,
 * Checks if window 'win' is in search/device mode, and takes care.
 * If stat_active == STATS_ON for 'win', and 'win' is not in search mode, it prints stats about size and permissions for every file.
 */
void list_everything(int win, int old_dim, int end) {
    int i;
    
    if (end == 0) {
        end = dim - 2;
    }
    wattron(mywin[win].fm, A_BOLD);
    for (i = old_dim; (i < ps[win].number_of_files) && (i  < old_dim + end); i++) {
        colored_folders(win, *(str_ptr[win] + i));
        if ((sv.searching == 3 + win) || (device_mode == 1 + win)) {
            mvwprintw(mywin[win].fm, INITIAL_POSITION + i - mywin[win].delta, 4, "%.*s", mywin[win].width - 5, *(str_ptr[win] + i));
        } else {
            mvwprintw(mywin[win].fm, INITIAL_POSITION + i - mywin[win].delta, 4, "%.*s", MAX_FILENAME_LENGTH, strrchr(*(str_ptr[win] + i), '/') + 1);
        }
        wattroff(mywin[win].fm, COLOR_PAIR);
    }
    wattroff(mywin[win].fm, A_BOLD);
    mvwprintw(mywin[win].fm, INITIAL_POSITION + ps[win].curr_pos - mywin[win].delta, 1, "->");
    if ((sv.searching != 3 + win) && (device_mode != 1 + win) && (mywin[win].stat_active == STATS_ON)) {
        show_stat(old_dim, end, win);
    }
    print_border_and_title(win);
}

/*
 * Helper function that prints borders and title of 'win'.
 * If win has stat_active == (STATS_ON || STATS_IDLE), adds current folder total size
 * to the right border's corner.
 */
static void print_border_and_title(int win) {
    wborder(mywin[win].fm, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(mywin[win].fm, 0, 0, "%.*s", mywin[win].width - 1, ps[win].title);
    mvwprintw(mywin[win].fm, 0, mywin[win].width - strlen(mywin[win].tot_size), mywin[win].tot_size);
    wrefresh(mywin[win].fm);
}

/*
 * Helper function passed to scandir (in generate_list() )
 * Will return false for '.', and for every file starting with '.' (except for '..') if !show_hidden
 */
static int is_hidden(const struct dirent *current_file) {
    if (current_file->d_name[0] == '.') {
        if ((strlen(current_file->d_name) == 1) || ((!config.show_hidden) && current_file->d_name[1] != '.')) {
            return (FALSE);
        }
    }
    return (TRUE);
}

/*
 * Creates a new tab.
 * Then, only if !resizing, calls initialize_tab_cwd().
 */
void new_tab(int win) {
    mywin[win].width = COLS / cont + COLS % cont;
    mywin[win].fm = subwin(stdscr, dim, mywin[win].width, 0, (COLS * (win)) / cont);
    keypad(mywin[win].fm, TRUE);
    scrollok(mywin[win].fm, TRUE);
    idlok(mywin[win].fm, TRUE);
    if (!resizing) {
        initialize_tab_cwd(win);
    }
}

/*
 * Helper functions called in main.c before creating second tab.
 */
void restrict_first_tab(void) {
    mywin[active].width = COLS / cont;
    wresize(mywin[active].fm, dim, mywin[active].width);
    print_border_and_title(active);
}

/*
 * Helper function for new_tab().
 * Calculates new tab's cwd and save new tab's title. Then refreshes UI.
 */
static void initialize_tab_cwd(int win) {
    if (strlen(config.starting_dir)) {
        if ((cont == 1) || (config.second_tab_starting_dir)) {
            strcpy(ps[win].my_cwd, config.starting_dir);
        }
    }
    if (!strlen(ps[win].my_cwd)) {
        getcwd(ps[win].my_cwd, PATH_MAX);
    }
    sprintf(ps[win].title, "Current: %s", ps[win].my_cwd);
    tab_refresh(win);
}

/*
 * Removes a tab and if !resizing, free its list of files.
 */
void delete_tab(int win) {
    wclear(mywin[win].fm);
    delwin(mywin[win].fm);
    mywin[win].fm = NULL;
    if (!resizing) {
        mywin[win].stat_active = STATS_OFF;
        free(ps[win].nl);
        ps[win].nl = NULL;
    }
}

/*
 * Called in main_loop (main.c) after deletion of second tab.
 */
void enlarge_first_tab(void) {
    mywin[active].width = COLS;
    wborder(mywin[active].fm, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    wresize(mywin[active].fm, dim, mywin[active].width);
    print_border_and_title(active);
}

void scroll_down(void) {
    if (ps[active].curr_pos < ps[active].number_of_files - 1) {
        ps[active].curr_pos++;
        if (ps[active].curr_pos - (dim - 2) == mywin[active].delta) {
            scroll_helper_func(dim - 2, 1);
            mywin[active].stat_active = STATS_ON;
            list_everything(active, ps[active].curr_pos, 1);
        } else {
            mvwprintw(mywin[active].fm, ps[active].curr_pos - mywin[active].delta, 1, "  ");
            mvwprintw(mywin[active].fm, ps[active].curr_pos - mywin[active].delta + INITIAL_POSITION, 1, "->");
        }
    }
}

void scroll_up(void) {
    if (ps[active].curr_pos > 0) {
        ps[active].curr_pos--;
        if (ps[active].curr_pos < mywin[active].delta) {
            scroll_helper_func(INITIAL_POSITION, -1);
            mywin[active].stat_active = STATS_ON;
            list_everything(active, mywin[active].delta, 1);
        } else {
            mvwprintw(mywin[active].fm, ps[active].curr_pos - mywin[active].delta + 2, 1, "  ");
            mvwprintw(mywin[active].fm, ps[active].curr_pos - mywin[active].delta + 1, 1, "->");
        }
    }
}

static void scroll_helper_func(int x, int direction) {
    mvwprintw(mywin[active].fm, x, 1, "  ");
    wborder(mywin[active].fm, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    wscrl(mywin[active].fm, direction);
    mywin[active].delta += direction;
}

/*
 * Follows ls color scheme to color files/folders. 
 * In search mode, it highlights paths inside archives in yellow.
 * In device mode, everything is printed in yellow.
 */
static void colored_folders(int win, const char *name) {
    struct stat file_stat;

    if (lstat(name, &file_stat) == 0) {
        if (S_ISDIR(file_stat.st_mode)) {
            wattron(mywin[win].fm, COLOR_PAIR(1));
        } else if (S_ISLNK(file_stat.st_mode)) {
            wattron(mywin[win].fm, COLOR_PAIR(3));
        } else if ((S_ISREG(file_stat.st_mode)) && (file_stat.st_mode & S_IXUSR)) {
            wattron(mywin[win].fm, COLOR_PAIR(2));
        }
    } else {
        wattron(mywin[win].fm, COLOR_PAIR(4));
    }
}

void trigger_show_helper_message(void) {
    if (!helper_win) {
        create_helper_win();
    } else {
        remove_helper_win();
    }
}

/*
 * Changes "dim" global var;
 * if current position in the folder was > dim - 3 (where dim goes from 0 to dim - 1, and -2 is because of helper_win borders),
 * change it to dim - 3 + ps[i].delta.
 * Then create helper_win and print its strings.
 */
static void create_helper_win(void) {
    int i;

    dim = LINES - INFO_HEIGHT - HELPER_HEIGHT;
    for (i = 0; i < cont; i++) {
        wresize(mywin[i].fm, dim, mywin[i].width);
        print_border_and_title(i);
        if (ps[i].curr_pos > dim - 3) {
            ps[i].curr_pos = dim - 3 + mywin[i].delta;
            mvwprintw(mywin[i].fm, dim - 3 + INITIAL_POSITION, 1, "->");
        }
        wrefresh(mywin[i].fm);
    }
    helper_win = subwin(stdscr, HELPER_HEIGHT, COLS, LINES - INFO_HEIGHT - HELPER_HEIGHT, 0);
    wclear(helper_win);
    helper_print();
}

/*
 * Remove helper_win, resize every ps.fm win and prints last HELPER_HEIGHT lines for each ps.win.
 */
static void remove_helper_win(void) {
    int i;

    wclear(helper_win);
    delwin(helper_win);
    helper_win = NULL;
    dim = LINES - INFO_HEIGHT;
    for (i = 0; i < cont; i++) {
        mvwhline(mywin[i].fm, dim - 1 - HELPER_HEIGHT, 0, ' ', COLS);
        wresize(mywin[i].fm, dim, mywin[i].width);
        list_everything(i, dim - 2 - HELPER_HEIGHT + mywin[i].delta, HELPER_HEIGHT);
    }
}

static void helper_print(void) {
    int i;

    wborder(helper_win, '|', '|', '-', '-', '+', '+', '+', '+');
    for (i = HELPER_HEIGHT - INFO_HEIGHT - 1; i >= 0; i--) {
        mvwprintw(helper_win, i + 1, 0, "| * %.*s", COLS - 5, helper_string[i]);
    }
    mvwprintw(helper_win, 0, 0, "Helper");
    wrefresh(helper_win);
}

/*
 * init: from where to print stats.
 * end: how many files' stats we need to print. (0 means all)
 * win: window where we need to print.
 * Prints size and perms for each of the files of the win between init and init + end.
 * Plus, calculates full folder size if mywin[win].tot_size is empty (it is emptied in generate_list,
 * so it will be empty only when a full redraw of the win is needed).
 */
static void show_stat(int init, int end, int win) {
    int j, i = 0;
    int check = strlen(mywin[win].tot_size);
    int perm_bit[9] = {S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP, S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH};
    char perm_sign[3] = {'r', 'w', 'x'}, str[20];
    float total_size = 0;
    struct stat file_stat;
    
    if (check) {
        i = init;
    }
    for (; i < ps[win].number_of_files; i++) {
        stat(ps[win].nl[i], &file_stat);
        if (!check) {
            total_size += file_stat.st_size;
        }
        if (i < init + end) {
            change_unit(file_stat.st_size, str);
            mvwprintw(mywin[win].fm, i + INITIAL_POSITION - mywin[win].delta, STAT_COL, "%s\t", str);
            for (j = 0; j < 9; j++) {
                wprintw(mywin[win].fm, (file_stat.st_mode & perm_bit[j]) ? "%c" : "-", perm_sign[j % 3]);
            }
            if ((i == end - 1) && (check)) {
                break;
            }
        }
    }
    if (!check) {
        change_unit(total_size, str);
        sprintf(mywin[win].tot_size, "Total size: %s", str);
    }
    mywin[win].stat_active = STATS_IDLE;
}

/*
 * Helper function used in show_stat: received a size,
 * it changes the unit from Kb to Mb to Gb if size > 1024(previous unit)
 */
static void change_unit(float size, char *str) {
    char *unit[3] = {"KB", "MB", "GB"};
    int i = 0;

    size /= 1024;
    while ((size > 1024) && (i < 3)) {
        size /= 1024;
        i++;
    }
    sprintf(str, "%.2f%s", size, unit[i]);
}

void trigger_stats(void) {
    mywin[active].stat_active = !mywin[active].stat_active;
    if (mywin[active].stat_active) {
        show_stat(mywin[active].delta, dim - 2, active);
    } else {
        erase_stat();
    }
    print_border_and_title(active);
}

/*
 * Move to STAT_COL and clear to eol.
 * Then, reprint border and title.
 */
static void erase_stat(void) {
    int i;

    for (i = 0; (i < ps[active].number_of_files) && (i < dim - 2); i++) {
        wmove(mywin[active].fm, i + 1, STAT_COL);
        wclrtoeol(mywin[active].fm);
    }
    memset(mywin[active].tot_size, 0, strlen(mywin[active].tot_size));
}

/*
 * Given a string str, and a line i, prints str on the I line of INFO_WIN.
 * Plus, if thread_h is not NULL, prints thread_job_mesg message(depends on current job type) at the end of INFO_LINE
 * It searches for selected_files too, and prints a message at the end of INFO_LINE - (strlen(running_h mesg) if there's.
 * Finally, if a search is running, prints a message at the end of ERR_LINE;
 * If asking_question != 0, there's a question being asked to user, so we won't clear it inside the for,
 * and we won't print str if i == INFO_LINE (that's the line where the user is being asked the question!)
 */
void print_info(const char *str, int i) {
    int len = 0;
    char st[100], search_str[20];

    // "quit_with_running_thread" is the longest question string i print. If asking a question (asking_question == 1), i won't clear the question being asked.
    wmove(info_win, INFO_LINE, 1 + (asking_question * strlen(quit_with_running_thread)));
    wclrtoeol(info_win);
    wmove(info_win, ERR_LINE, 1);
    wclrtoeol(info_win);
    if (thread_h) {
        sprintf(st, "[%d/%d] %s", thread_h->num, num_of_jobs, thread_job_mesg[thread_h->type]);
        len = strlen(st) + 1;
        mvwprintw(info_win, INFO_LINE, COLS - strlen(st), st);
    }
    if (selected) {
        mvwprintw(info_win, INFO_LINE, COLS - len - strlen(selected_mess), selected_mess);
    }
    if (sv.searching) {
        if (sv.searching >= 3) {
            sprintf(search_str, "%d files found.", sv.found_cont);
            mvwprintw(info_win, ERR_LINE, COLS - strlen(search_str), search_str);
        } else {
            mvwprintw(info_win, ERR_LINE, COLS - strlen(searching_mess[sv.searching - 1]), searching_mess[sv.searching - 1]);
        }
    }
    if (str && (!asking_question || i == ERR_LINE)) {
        mvwprintw(info_win, i, 1, "%.*s", COLS - (strlen("I: ") + 1), str);
    }
    wrefresh(info_win);
}

/*
 * Given a str, a char input[dim], and a char c (that is default value if enter is pressed, if dim == 1),
 * asks the user "str" and saves in input the user response.
 */
void ask_user(const char *str, char *input, int dim, char c) {
    echo();
    print_info(str, INFO_LINE);
    asking_question = 1;
    if (dim == 1) {
        *input = tolower(wgetch(info_win));
        if (*input == 10) {
            *input = c;
        }
    } else {
        wgetnstr(info_win, input, NAME_MAX);
    }
    noecho();
    asking_question = 0;
    print_info(NULL, INFO_LINE);
}

int win_getch(void) {
    return wgetch(mywin[active].fm);
}

/*
 * Refreshes win UI if win is not in searching or device mode.
 * Mutex is needed because worker thread can call this function too.
 */
void tab_refresh(int win) {
    if ((sv.searching != 3 + win) && (device_mode != 1 + win)) {
        pthread_mutex_lock(&ui_lock);
        generate_list(win);
        pthread_mutex_unlock(&ui_lock);
    }
}

/*
 * Removes every win except for stdscr;
 * reprint every win, if help==1, it needs to reprint 
 * helper_win too.
 * Then prints any "sticky" message (eg: "searching"/"pasting..." etc etc)
 */
void resize_win(void) {
    int help = 0;
    
    if (helper_win) {
        help = 1;
    }
    resizing = 1;
    screen_end();
    fm_scr_init();
    if (help) {
        create_helper_win();
    }
    print_info(NULL, INFO_LINE);
    resizing = 0;
}

void change_sort(void) {
    int i;
    
    if (sorting_func == alphasort) {
        sorting_func = sizesort;
        print_info("Files will be sorted by size now.", INFO_LINE);
    } else if (sorting_func == sizesort) {
        sorting_func = last_mod_sort;
        print_info("Files will be sorted by last access now.", INFO_LINE);
    } else {
        sorting_func = alphasort;
        print_info("Files will be sorted alphabetically now.", INFO_LINE);
    }
    for (i = 0; i < cont; i++) {
        tab_refresh(i);
    }
}