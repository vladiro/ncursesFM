/* BEGIN_COMMON_COPYRIGHT_HEADER
 *
 * NcursesFM: file manager in C with ncurses UI for linux.
 * https://github.com/FedeDP/ncursesFM
 *
 * Copyright (C) 2015  Federico Di Pierro <nierro92@gmail.com>
 *
 * This file is part of ncursesFM.
 * ncursesFM is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "fm_functions.h"

#ifdef LIBX11_PRESENT
static void xdg_open(const char *str);
#endif
static void open_file(const char *str);
static void iso_mount_service(const char *str);
static void cpr(int n);
static int recursive_copy(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
static void check_pasted(void);
static int recursive_remove(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
static int rmrf(const char *path);
static int recursive_search(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
static int search_inside_archive(const char *path, int i);
static int search_file(const char *path);
static void *search_thread(void *x);
#ifdef LIBCUPS_PRESENT
static void *print_file(void *filename);
#endif
static void archiver_func(void);
static int recursive_archive(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
static void *try_extractor(void *x);
static void extractor_thread(struct archive *a);

static struct archive *archive = NULL;
static int distance_from_root;

void change_dir(const char *str)
{
    if (chdir(str) != -1) {
        pthread_mutex_lock(&lock);
        getcwd(ps[active].my_cwd, PATH_MAX);
        pthread_mutex_unlock(&lock);
        generate_list(active);
        print_info(NULL, INFO_LINE);
    } else {
        print_info(strerror(errno), ERR_LINE);
    }
}

void switch_hidden(void)
{
    int i;

    config.show_hidden = !config.show_hidden;
    for (i = 0; i < cont; i++) {
        generate_list(i);
    }
}

void manage_file(const char *str)
{
    int fd = open(str, O_RDONLY);

    if ((running_h) && (flock(fd, LOCK_EX | LOCK_NB))) {
        print_info(file_used_by_thread, ERR_LINE);
    } else {
        if (get_mimetype(str, "iso")) {
            iso_mount_service(str);
        } else if (is_archive(str)) {
            init_thread(EXTRACTOR_TH, try_extractor, str);
        }
        #ifdef LIBX11_PRESENT
        else if (access("/usr/bin/xdg-open", X_OK) != -1) {
            xdg_open(str);
        }
        #endif
        else {
            open_file(str);
        }
    }
    close(fd);
}

#ifdef LIBX11_PRESENT
static void xdg_open(const char *str)
{
    pid_t pid;
    Display* display = XOpenDisplay(NULL);

    if (display) {
        XCloseDisplay(display);
        pid = vfork();
        if (pid == 0) {
            int fd = open("/dev/null",O_WRONLY);
            dup2(fd, 1);
            dup2(fd, 2);
            close(fd);
            execl("/usr/bin/xdg-open", "/usr/bin/xdg-open", str, NULL);
        }
    } else {
        return open_file(str);
    }
}
#endif

static void open_file(const char *str)
{
    pid_t pid;

    if ((config.editor) && (access(config.editor, X_OK) != -1)) {
        if ((get_mimetype(str, "text/")) || (get_mimetype(str, "x-empty"))) {
            endwin();
            pid = vfork();
            if (pid == 0) {
                execl(config.editor, config.editor, str, NULL);
            } else {
                waitpid(pid, NULL, 0);
            }
            refresh();
        }
    } else {
        print_info(editor_missing, ERR_LINE);
    }
}

static void iso_mount_service(const char *str)
{
    pid_t pid;
    char mount_point[strlen(str)];

    if (access("/usr/bin/fuseiso", F_OK) == -1) {
        print_info(fuseiso_missing, ERR_LINE);
        return;
    }
    if (access(ps[active].my_cwd, W_OK) != 0) {
        print_info(no_w_perm, ERR_LINE);
        return;
    }
    strcpy(mount_point, str);
    mount_point[strlen(str) - strlen(strrchr(str, '.'))] = '\0';
    pid = vfork();
    if (pid == 0) {
        int fd = open("/dev/null",O_WRONLY);
        dup2(fd, 1);
        dup2(fd, 2);
        close(fd);
        if (mkdir(mount_point, ACCESSPERMS) == -1) {
            execl("/usr/bin/fusermount", "/usr/bin/fusermount", "-u", mount_point, NULL);
        } else {
            execl("/usr/bin/fuseiso", "/usr/bin/fuseiso", str, mount_point, NULL);
        }
    } else {
        waitpid(pid, NULL, 0);
        if (rmdir(mount_point) == 0) {
            print_info(unmounted, INFO_LINE);
        } else {
            print_info(mounted, INFO_LINE);
        }
        sync_changes(ps[active].my_cwd);
    }
}

void *new_file(void *x)
{
    FILE *f;
    char current_dir[PATH_MAX];

    if (running_h->type == NEW_FILE_TH) {
            f = fopen(running_h->full_path, "w");
            fclose(f);
    } else {
        mkdir(running_h->full_path, 0700);
    }
    strcpy(current_dir, running_h->full_path);
    current_dir[strlen(current_dir) - strlen(strrchr(current_dir, '/'))] = '\0';
    sync_changes(current_dir);
    execute_thread(file_created, INFO_LINE);
    return NULL;
}

void *remove_file(void *x)
{
    const char *str = removed;
    char current_dir[PATH_MAX];
    int line = INFO_LINE;

    print_info(NULL, INFO_LINE);
    if (rmrf(running_h->full_path) == -1) {
        str = rm_fail;
        line = ERR_LINE;
    } else {
        strcpy(current_dir, running_h->full_path);
        current_dir[strlen(current_dir) - strlen(strrchr(current_dir, '/'))] = '\0';
        sync_changes(current_dir);
    }
    execute_thread(str, line);
    return NULL;
}

void manage_c_press(char c)
{
    if (!current_th)
        thread_h = add_thread(thread_h);
    if ((!current_th->selected_files) || (remove_from_list(ps[active].nl[ps[active].curr_pos]) == 0)) {
        current_th->selected_files = select_file(c, current_th->selected_files,  ps[active].nl[ps[active].curr_pos]);
        print_info(file_sel1, INFO_LINE);
    } else {
        if (current_th->selected_files) {
            print_info(file_sel2, INFO_LINE);
        } else {
            print_info(file_sel3, INFO_LINE);
        }
    }
}

void *paste_file(void *x)
{
    char pasted_file[PATH_MAX], copied_file_dir[PATH_MAX];
    struct stat file_stat_copied, file_stat_pasted;
    file_list *tmp = NULL;
    int i = 0;

    lstat(running_h->full_path, &file_stat_pasted);
    for (tmp = running_h->selected_files; tmp; tmp = tmp->next) {
        strcpy(copied_file_dir, tmp->name);
        copied_file_dir[strlen(tmp->name) - strlen(strrchr(tmp->name, '/'))] = '\0';
        if (strcmp(running_h->full_path, copied_file_dir) == 0) {
            tmp->cut = CANNOT_PASTE_SAME_DIR;
        } else {
            lstat(copied_file_dir, &file_stat_copied);
            if ((tmp->cut == 1) && (file_stat_copied.st_dev == file_stat_pasted.st_dev)) {
                sprintf(pasted_file, "%s%s", running_h->full_path, strrchr(tmp->name, '/'));
                tmp->cut = MOVED_FILE;
                if (rename(tmp->name, pasted_file) == - 1) {
                    print_info(strerror(errno), ERR_LINE);
                }
            } else {
                i++;
            }
        }
    }
    cpr(i);
    execute_thread(pasted_mesg, INFO_LINE);
    return NULL;
}

static void cpr(int n)
{
    file_list *tmp = running_h->selected_files;

    print_info(NULL, INFO_LINE);
    while ((n > 0) && (tmp)) {
        if ((tmp->cut != MOVED_FILE) && (tmp->cut != CANNOT_PASTE_SAME_DIR)) {
            distance_from_root = strlen(tmp->name) - strlen(strrchr(tmp->name, '/'));
            nftw(tmp->name, recursive_copy, 64, FTW_MOUNT | FTW_PHYS);
        }
        tmp = tmp->next;
    }
    check_pasted();
}

static int recursive_copy(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int buff[BUFF_SIZE];
    int len, fd_to, fd_from;
    char pasted_file[PATH_MAX];

    sprintf(pasted_file, "%s%s", running_h->full_path, path + distance_from_root);
    if (typeflag == FTW_D) {
        mkdir(pasted_file, sb->st_mode);
    } else {
        fd_to = open(pasted_file, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, sb->st_mode);
        fd_from = open(path, O_RDONLY);
        if ((fd_to != -1) && (fd_from != -1)) {
            flock(fd_to, LOCK_EX);
            flock(fd_from, LOCK_EX);
            len = read(fd_from, buff, sizeof(buff));
            while (len > 0) {
                write(fd_to, buff, len);
                len = read(fd_from, buff, sizeof(buff));
            }
            close(fd_to);
            close(fd_from);
            flock(fd_to, LOCK_UN);
            flock(fd_from, LOCK_UN);
        }
    }
    return 0;
}

static void check_pasted(void)
{
    file_list *tmp = running_h->selected_files;
    char copied_file_dir[PATH_MAX];

    sync_changes(running_h->full_path);
    while (tmp) {
        if (tmp->cut == 1) {
            if (rmrf(tmp->name) == -1) {
                print_info(rm_fail, ERR_LINE);
            }
        }
        if ((tmp->cut == 1) || (tmp->cut == MOVED_FILE)) {
            strcpy(copied_file_dir, tmp->name);
            copied_file_dir[strlen(tmp->name) - strlen(strrchr(tmp->name, '/'))] = '\0';
            sync_changes(copied_file_dir);
        }
        tmp = tmp->next;
    }
}

void *rename_file_folders(void *x)
{
    const char *str = renamed;
    char current_dir[PATH_MAX];
    int line = INFO_LINE;

    if (rename(running_h->full_path, running_h->selected_files->name) == - 1) {
        str = strerror(errno);
        line = ERR_LINE;
    } else {
        strcpy(current_dir, running_h->full_path);
        current_dir[strlen(current_dir) - strlen(strrchr(current_dir, '/'))] = '\0';
        sync_changes(current_dir);
    }
    execute_thread(str, line);
    return NULL;
}

static int recursive_remove(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int res, fd;

    fd = open(path, O_RDONLY);
    flock(fd, LOCK_EX);
    res = remove(path);
    close(fd);
    return res;
}

static int rmrf(const char *path)
{
    if (access(path, W_OK) == 0) {
        return nftw(path, recursive_remove, 64, FTW_DEPTH | FTW_PHYS | FTW_MOUNT);
    }
    return -1;
}

static int recursive_search(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    char fixed_str[PATH_MAX];
    int i = 0;

    if (ftwbuf->level == 0) {
        return 0;
    }
    while ((sv.found_searched[i]) && (i < MAX_NUMBER_OF_FOUND)) {
        i++;
    }
    if (i == MAX_NUMBER_OF_FOUND) {
        free_str(sv.found_searched);
        return 1;
    }
    if ((sv.search_archive) && (is_archive(strrchr(path, '/')))) {
        return search_inside_archive(path, i);
    } else {
        strcpy(fixed_str, strrchr(path, '/') + 1);
        if (strncmp(fixed_str, sv.searched_string, strlen(sv.searched_string)) == 0) {
            if (!(sv.found_searched[i] = safe_malloc(sizeof(char) * PATH_MAX, search_mem_fail))) {
                return 1;
            }
            strcpy(sv.found_searched[i], path);
            if (typeflag == FTW_D) {
                strcat(sv.found_searched[i], "/");
            }
        }
    }
    return 0;
}

static int search_inside_archive(const char *path, int i)
{
    char str[PATH_MAX];
    struct archive *a = archive_read_new();
    struct archive_entry *entry;

    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    if (archive_read_open_filename(a, path, BUFF_SIZE) == ARCHIVE_OK) {
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            strcpy(str, archive_entry_pathname(entry));
            if (str[strlen(str) - 1] == '/') {// check if we're on a dir
                str[strlen(str) - 1] = '\0';
            }
            if (strrchr(str, '/')) {
                strcpy(str, strrchr(str, '/') + 1);
            }
            if (strncmp(str, sv.searched_string, strlen(sv.searched_string)) == 0) {
                if (!(sv.found_searched[i] = safe_malloc(sizeof(char) * PATH_MAX, search_mem_fail))) {
                    archive_read_free(a);
                    return 1;
                }
                sprintf(sv.found_searched[i], "%s/%s", path, archive_entry_pathname(entry));
                i++;
            }
        }
    }
    archive_read_free(a);
    return 0;
}

static int search_file(const char *path)
{
    return nftw(path, recursive_search, 64, FTW_MOUNT | FTW_PHYS);
}

void search(void)
{
    pthread_t search_th;
    char c;

    ask_user(search_insert_name, sv.searched_string, 20, 0);
    if (strlen(sv.searched_string) < 5) {
        print_info(searched_string_minimum, INFO_LINE);
        return;
    }
    ask_user(search_archives, &c, 1, 'n');
    if (c == 'y') {
        sv.search_archive = 1;
    }
    sv.searching = 1;
    print_info(NULL, INFO_LINE);
    pthread_create(&search_th, NULL, search_thread, NULL);
    pthread_detach(search_th);
}

static void *search_thread(void *x)
{
    int ret = search_file(ps[active].my_cwd);

    if (!sv.found_searched[0]) {
        sv.searching = 0;
        sv.search_archive = 0;
        if (ret == 1) {
            print_info(too_many_found, INFO_LINE);
        } else {
            print_info(no_found, INFO_LINE);
        }
    } else {
        sv.searching = 2;
        print_info(NULL, INFO_LINE);
    }
    return NULL;
}

void list_found(void)
{
    int i = 0;
    char str[20];

    sv.searching = 3 + active;
    while (sv.found_searched[i]) {
        i++;
    }
    ps[active].delta = 0;
    ps[active].curr_pos = 0;
    wclear(ps[active].fm);
    list_everything(active, 0, 0, sv.found_searched);
    sprintf(str, "%d files found.", i);
    print_info(str, INFO_LINE);
    search_loop();
}

void search_loop(void)
{
    char arch_str[PATH_MAX];
    char input;
    int c, len;

    arch_str[0] = '\0';
    do {
        c = wgetch(ps[active].fm);
        switch (c) {
        case KEY_UP:
            scroll_up(sv.found_searched);
            break;
        case KEY_DOWN:
            scroll_down(sv.found_searched);
            break;
        case 10:
            if (sv.search_archive) {
                strcpy(arch_str, sv.found_searched[ps[active].curr_pos]);
                while (strlen(arch_str) && !is_archive(strrchr(arch_str, '/'))) {
                    arch_str[strlen(arch_str) - strlen(strrchr(arch_str, '/'))] = '\0';
                }
            }
            len = strlen(strrchr(sv.found_searched[ps[active].curr_pos], '/'));
            if ((!strlen(arch_str)) && (len != 1)) { // is a file
                ask_user(search_loop_str1, &input, 1, 'y');
                if (input != 'n') {
                    if (input == 'y') {// is a file and user wants to open it
                        manage_file(sv.found_searched[ps[active].curr_pos]);
                    }
                    break;
                }
            } // is a dir or an archive or a file but user wants to switch to its dir
            len = strlen(sv.found_searched[ps[active].curr_pos]) - len;
            if (strlen(arch_str)) { // is an archive
                ask_user(search_loop_str2, &input, 1, 'y');
                if (input == 'y') {
                    len = strlen(arch_str) - strlen(strrchr(arch_str, '/'));
                } else {
                    break;
                }
            }
            sv.found_searched[ps[active].curr_pos][len] = '\0';
            c = 'q';
            break;
        case 9: // tab to change tab
            if (cont == MAX_TABS) {
                active = 1 - active;
                chdir(ps[active].my_cwd);
                return;
            }
            break;
        case 'q':
        case 'Q':
            strcpy(sv.found_searched[ps[active].curr_pos], ps[active].my_cwd);
            break;
        }
    } while ((c != 'q') && (c != 'Q'));
    sv.searching = 0;
    sv.search_archive = 0;
    change_dir(sv.found_searched[ps[active].curr_pos]);
    free_str(sv.found_searched);
}

#ifdef LIBCUPS_PRESENT
void print_support(char *str)
{
    pthread_t print_thread;
    char c;
    int fd = open(str, O_RDONLY);

    if ((running_h) && (flock(fd, LOCK_EX | LOCK_NB))) {
        print_info(file_used_by_thread, ERR_LINE);
        close(fd);
        return;
    }
    ask_user(print_question, &c, 1, 'y');
    if (c == 'y') {
        pthread_create(&print_thread, NULL, print_file, str);
        pthread_detach(print_thread);
    }
    close(fd);
}

static void *print_file(void *filename)
{
    cups_dest_t *dests, *default_dest;
    int num_dests = cupsGetDests(&dests);

    if (num_dests > 0) {
        default_dest = cupsGetDest(NULL, NULL, num_dests, dests);
        cupsPrintFile(default_dest->name, (char *)filename, "ncursesFM job", default_dest->num_options, default_dest->options);
        print_info(print_ok, INFO_LINE);
    } else {
        print_info(print_fail, ERR_LINE);
    }
    return NULL;
}
#endif

void *create_archive(void *x)
{
    const char *str = archive_ready;
    int line = INFO_LINE, fd;

    archive = archive_write_new();
    if (((archive_write_add_filter_gzip(archive) == ARCHIVE_FATAL) || (archive_write_set_format_pax_restricted(archive) == ARCHIVE_FATAL)) ||
        (archive_write_open_filename(archive, running_h->full_path) == ARCHIVE_FATAL)) {
        str = strerror(archive_errno(archive));
        line = ERR_LINE;
        archive_write_free(archive);
        archive = NULL;
    } else {
        fd = open(running_h->full_path, O_RDONLY);
        flock(fd, LOCK_EX);
        archiver_func();
        close(fd);
    }
    execute_thread(str, line);
    return NULL;
}

static void archiver_func(void)
{
    file_list *tmp = running_h->selected_files;
    char str[PATH_MAX];

    print_info(NULL, INFO_LINE);
    while (tmp) {
        distance_from_root = strlen(tmp->name) - strlen(strrchr(tmp->name, '/'));
        nftw(tmp->name, recursive_archive, 64, FTW_MOUNT | FTW_PHYS);
        tmp = tmp->next;
    }
    archive_write_free(archive);
    archive = NULL;
    strcpy(str, running_h->full_path);
    str[strlen(running_h->full_path) - strlen(strrchr(running_h->full_path, '/'))] = '\0';
    sync_changes(str);
}

static int recursive_archive(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    char buff[BUFF_SIZE], entry_name[PATH_MAX];
    int len, fd;
    struct archive_entry *entry = archive_entry_new();

    strcpy(entry_name, path + distance_from_root + 1);
    archive_entry_set_pathname(entry, entry_name);
    archive_entry_copy_stat(entry, sb);
    archive_write_header(archive, entry);
    archive_entry_free(entry);
    fd = open(path, O_RDONLY);
    if (fd != -1) {
        flock(fd, LOCK_EX);
        len = read(fd, buff, sizeof(buff));
        while (len > 0) {
            archive_write_data(archive, buff, len);
            len = read(fd, buff, sizeof(buff));
        }
        close(fd);
    }
    return 0;
}

static void *try_extractor(void *x)
{
    struct archive *a;
    const char *str = extracted;
    int line = INFO_LINE, fd;

    a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    if (archive_read_open_filename(a, running_h->full_path, BUFF_SIZE) == ARCHIVE_OK) {
        fd = open(running_h->full_path, O_RDONLY);
        flock(fd, LOCK_EX);
        extractor_thread(a);
        close(fd);
    } else {
        str = archive_error_string(a);
        line = ERR_LINE;
        archive_read_free(a);
    }
    execute_thread(str, line);
    return NULL;
}

static void extractor_thread(struct archive *a)
{
    struct archive *ext;
    struct archive_entry *entry;
    int flags, len, fd;
    char buff[BUFF_SIZE], current_dir[PATH_MAX], fullpathname[PATH_MAX];

    strcpy(current_dir, running_h->full_path);
    current_dir[strlen(current_dir) - strlen(strrchr(current_dir, '/'))] = '\0';
    print_info(NULL, INFO_LINE);
    ext = archive_write_disk_new();
    flags = ARCHIVE_EXTRACT_TIME;
    flags |= ARCHIVE_EXTRACT_PERM;
    flags |= ARCHIVE_EXTRACT_ACL;
    flags |= ARCHIVE_EXTRACT_FFLAGS;
    archive_write_disk_set_options(ext, flags);
    archive_write_disk_set_standard_lookup(ext);
    while (archive_read_next_header(a, &entry) != ARCHIVE_EOF) {
        sprintf(fullpathname, "%s/%s", current_dir, archive_entry_pathname(entry));
        archive_entry_set_pathname(entry, fullpathname);
        archive_write_header(ext, entry);
        fd = open(fullpathname, O_RDONLY);
        flock(fd, LOCK_EX);
        len = archive_read_data(a, buff, sizeof(buff));
        while (len > 0) {
            archive_write_data(ext, buff, len);
            len = archive_read_data(a, buff, sizeof(buff));
        }
        close(fd);
    }
    archive_read_free(a);
    archive_write_free(ext);
    sync_changes(current_dir);
}

void change_tab(void)
{
    pthread_mutex_lock(&lock);
    active = !active;
    pthread_mutex_unlock(&lock);
    if (sv.searching != 3 + active) {
        chdir(ps[active].my_cwd);
    } else {
        search_loop();
    }
}