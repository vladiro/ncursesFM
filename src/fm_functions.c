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
static int iso_mount_service(void);
static void cpr(file_list *tmp);
static int recursive_copy(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
static int recursive_remove(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
static void rmrf(const char *path);
static int recursive_search(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
static int search_inside_archive(const char *path);
static void *search_thread(void *x);
#ifdef LIBCUPS_PRESENT
static void *print_file(void *filename);
#endif
static void archiver_func(void);
static int recursive_archive(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
static int try_extractor(void);
static void extractor_thread(struct archive *a);

static struct archive *archive;
static int distance_from_root;

void change_dir(const char *str)
{
    if (chdir(str) != -1) {
        getcwd(ps[active].my_cwd, PATH_MAX);
        print_info(NULL, INFO_LINE);
        needs_refresh = FORCE_REFRESH;
    } else {
        print_info(strerror(errno), ERR_LINE);
    }
}

void switch_hidden(void)
{
    config.show_hidden = !config.show_hidden;
    needs_refresh = REFRESH;
}

/*
 * Check if it is an iso, then try to mount it.
 * If is an archive, initialize a thread job to extract it.
 * if compiled with X11 support, and xdg-open is found, open the file,
 * else open the file with $config.editor.
 */
void manage_file(const char *str)
{
    char c;

    if (get_mimetype(str, "iso")) {
        return init_thread(FUSEISO_TH, iso_mount_service, str);
    }
    if (is_archive(str)) {
        ask_user(extr_question, &c, 1, 'y');
        if (c == 'y') {
            return init_thread(EXTRACTOR_TH, try_extractor, str);
        }
        return;
    }
#ifdef LIBX11_PRESENT
    if (access("/usr/bin/xdg-open", X_OK) != -1) {
        return xdg_open(str);
    }
#endif
    return open_file(str);
}

/*
 * If we're on a X screen, open the file with xdg-open.
 */
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

/*
 * If file mimetype contains "text" or "x-empty" (empty file), open it with config.editor.
 */
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

/*
 * Create a dir with the file.iso name without extension, and mount there the iso through fuseiso.
 * If mkdir fails, it means folder is already present, so unmount the iso.
 */
static int iso_mount_service(void)
{
    pid_t pid;
    char mount_point[strlen(thread_h->full_path)];

    if ((access("/usr/bin/fuseiso", F_OK) == -1) || (access(thread_h->full_path, F_OK) == -1)) {
        return -1;
    }
    strcpy(mount_point, thread_h->full_path);
    mount_point[strlen(mount_point) - strlen(strrchr(mount_point, '.'))] = '\0';
    pid = vfork();
    if (pid == 0) {
        if (mkdir(mount_point, ACCESSPERMS) == -1) {
            execl("/usr/bin/fusermount", "/usr/bin/fusermount", "-u", mount_point, NULL);
        } else {
            execl("/usr/bin/fuseiso", "/usr/bin/fuseiso", thread_h->full_path, mount_point, NULL);
        }
    } else {
        waitpid(pid, NULL, 0);
        rmdir(mount_point);
    }
    return 0;
}

int new_file(void)
{
    FILE *f;

    if (thread_h->type == NEW_FILE_TH) {
        f = fopen(thread_h->full_path, "w");
        fclose(f);
    } else {
        mkdir(thread_h->full_path, 0700);
    }
    return 0;
}

int remove_file(void)
{
    int ok = 0;
    file_list *tmp = thread_h->selected_files;

    while (tmp) {
        if (access(tmp->name, W_OK) == 0) {
            ok++;
            rmrf(tmp->name);
        }
        tmp = tmp->next;
    }
    return (ok ? 0 : -1);
}

/*
 * If there are no selected files, or the file user selected wasn't already selected,
 * add this file to selecte list.
 */
void manage_space_press(const char *str)
{
    const char *s;

    if ((!selected) || (remove_from_list(str) == 0)) {
        selected = select_file(selected, str);
        s = file_sel[0];
    } else {
        ((selected) ? (s = file_sel[1]) : (s = file_sel[2]));
    }
    print_info(s, INFO_LINE);
}

int paste_file(void)
{
    char copied_file_dir[PATH_MAX];
    file_list *tmp = NULL;

    for (tmp = thread_h->selected_files; tmp; tmp = tmp->next) {
        strcpy(copied_file_dir, tmp->name);
        copied_file_dir[strlen(tmp->name) - strlen(strrchr(tmp->name, '/'))] = '\0';
        if (strcmp(thread_h->full_path, copied_file_dir) != 0) {
            cpr(tmp);
        }
    }
    return 0;
}

int move_file(void)
{
    char pasted_file[PATH_MAX], copied_file_dir[PATH_MAX];
    struct stat file_stat_copied, file_stat_pasted;
    file_list *tmp = NULL;

    lstat(thread_h->full_path, &file_stat_pasted);
    for (tmp = thread_h->selected_files; tmp; tmp = tmp->next) {
        strcpy(copied_file_dir, tmp->name);
        copied_file_dir[strlen(tmp->name) - strlen(strrchr(tmp->name, '/'))] = '\0';
        if (strcmp(thread_h->full_path, copied_file_dir) != 0) {
            lstat(copied_file_dir, &file_stat_copied);
            if (file_stat_copied.st_dev == file_stat_pasted.st_dev) { // if on the same fs, just rename the file
                sprintf(pasted_file, "%s%s", thread_h->full_path, strrchr(tmp->name, '/'));
                if (rename(tmp->name, pasted_file) == - 1) {
                    print_info(strerror(errno), ERR_LINE);
                }
            } else { // copy file and remove original file
                cpr(tmp);
                rmrf(tmp->name);
            }
        }
    }
    return 0;
}

static void cpr(file_list *tmp)
{
    distance_from_root = strlen(tmp->name) - strlen(strrchr(tmp->name, '/'));
    nftw(tmp->name, recursive_copy, 64, FTW_MOUNT | FTW_PHYS);
}

static int recursive_copy(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int buff[BUFF_SIZE];
    int len, fd_to, fd_from;
    char pasted_file[PATH_MAX];

    sprintf(pasted_file, "%s%s", thread_h->full_path, path + distance_from_root);
    if (typeflag == FTW_D) {
        mkdir(pasted_file, sb->st_mode);
    } else {
        fd_to = open(pasted_file, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, sb->st_mode);
        fd_from = open(path, O_RDONLY);
        if ((fd_to != -1) && (fd_from != -1)) {
            len = read(fd_from, buff, sizeof(buff));
            while (len > 0) {
                write(fd_to, buff, len);
                len = read(fd_from, buff, sizeof(buff));
            }
            close(fd_to);
            close(fd_from);
        }
    }
    return 0;
}

int rename_file_folders(void)
{
    return rename(thread_h->full_path, thread_h->selected_files->name);
}

static int recursive_remove(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    return remove(path);
}

static void rmrf(const char *path)
{
    nftw(path, recursive_remove, 64, FTW_DEPTH | FTW_PHYS | FTW_MOUNT);
}

static int recursive_search(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    char fixed_str[PATH_MAX];

    if (ftwbuf->level == 0) {
        return 0;
    }
    if (sv.found_cont == MAX_NUMBER_OF_FOUND) {
        free_str(sv.found_searched);
        return 1;
    }
    if ((sv.search_archive) && (is_archive(strrchr(path, '/')))) {
        return search_inside_archive(path);
    }
    strcpy(fixed_str, strrchr(path, '/') + 1);
    if (strncmp(fixed_str, sv.searched_string, strlen(sv.searched_string)) == 0) {
        if (!(sv.found_searched[sv.found_cont] = safe_malloc(sizeof(char) * PATH_MAX, search_mem_fail))) {
            return 1;
        }
        strcpy(sv.found_searched[sv.found_cont], path);
        if (typeflag == FTW_D) {
            strcat(sv.found_searched[sv.found_cont], "/");
        }
        sv.found_cont++;
    }
    return 0;
}

static int search_inside_archive(const char *path)
{
    char str[PATH_MAX];
    struct archive *a = archive_read_new();
    struct archive_entry *entry;

    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    if ((a) && (archive_read_open_filename(a, path, BUFF_SIZE) == ARCHIVE_OK)) {
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            strcpy(str, archive_entry_pathname(entry));
            if (str[strlen(str) - 1] == '/') {  // check if we're on a dir
                str[strlen(str) - 1] = '\0';
            }
            if (strrchr(str, '/')) {
                strcpy(str, strrchr(str, '/') + 1);
            }
            if (strncmp(str, sv.searched_string, strlen(sv.searched_string)) == 0) {
                if (!(sv.found_searched[sv.found_cont] = safe_malloc(sizeof(char) * PATH_MAX, search_mem_fail))) {
                    archive_read_free(a);
                    return 1;
                }
                sprintf(sv.found_searched[sv.found_cont], "%s/%s", path, archive_entry_pathname(entry));
                sv.found_cont++;
            }
        }
    }
    archive_read_free(a);
    return 0;
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
    sv.found_cont = 0;
    sv.search_archive = 0;
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
    int ret = nftw(ps[active].my_cwd, recursive_search, 64, FTW_MOUNT | FTW_PHYS);

    if (!sv.found_searched[0]) {
        sv.searching = 0;
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
    sv.searching = 3 + active;
    ps[active].files_ptr = sv.found_searched;
    ps[active].number_of_files = sv.found_cont;
    reset_win(active);
    list_everything(active, 0, 0);
    print_info(NULL, INFO_LINE);
}

void leave_search_mode(const char *str)
{
    sv.searching = 0;
    change_dir(str);
    free_str(sv.found_searched);
}

int search_loop_enter_press(const char *str)
{
    char arch_str[PATH_MAX] = {};

    if (sv.search_archive) {    // check if the current path is inside an archive
        strcpy(arch_str, str);
        while (strlen(arch_str) && !is_archive(strrchr(arch_str, '/'))) {
            arch_str[strlen(arch_str) - strlen(strrchr(arch_str, '/'))] = '\0';
        }
    }
    if (strlen(arch_str)) {
        return (strlen(arch_str) - strlen(strrchr(arch_str, '/')));
    }
    return (strlen(str) - strlen(strrchr(str, '/')));
}

#ifdef LIBCUPS_PRESENT
void print_support(char *str)
{
    pthread_t print_thread;
    char c;

    ask_user(print_question, &c, 1, 'y');
    if (c == 'y') {
        pthread_create(&print_thread, NULL, print_file, str);
        pthread_detach(print_thread);
    }
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

int create_archive(void)
{
    archive = archive_write_new();
    if (((archive_write_add_filter_gzip(archive) == ARCHIVE_FATAL) || (archive_write_set_format_pax_restricted(archive) == ARCHIVE_FATAL)) ||
        (archive_write_open_filename(archive, thread_h->full_path) == ARCHIVE_FATAL)) {
        archive_write_free(archive);
        archive = NULL;
        return -1;
    }
    archiver_func();
    return 0;
}

static void archiver_func(void)
{
    file_list *tmp = thread_h->selected_files;

    while (tmp) {
        distance_from_root = strlen(tmp->name) - strlen(strrchr(tmp->name, '/'));
        nftw(tmp->name, recursive_archive, 64, FTW_MOUNT | FTW_PHYS);
        tmp = tmp->next;
    }
    archive_write_free(archive);
    archive = NULL;
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
        len = read(fd, buff, sizeof(buff));
        while (len > 0) {
            archive_write_data(archive, buff, len);
            len = read(fd, buff, sizeof(buff));
        }
        close(fd);
    }
    return 0;
}

static int try_extractor(void)
{
    struct archive *a;

    a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    if ((a) && (archive_read_open_filename(a, thread_h->full_path, BUFF_SIZE) == ARCHIVE_OK)) {
        extractor_thread(a);
        return 0;
    }
    archive_read_free(a);
    return -1;
}

static void extractor_thread(struct archive *a)
{
    struct archive *ext;
    struct archive_entry *entry;
    int len;
    int flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS;
    char buff[BUFF_SIZE], current_dir[PATH_MAX], fullpathname[PATH_MAX];

    strcpy(current_dir, thread_h->full_path);
    current_dir[strlen(current_dir) - strlen(strrchr(current_dir, '/'))] = '\0';
    ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, flags);
    archive_write_disk_set_standard_lookup(ext);
    while (archive_read_next_header(a, &entry) != ARCHIVE_EOF) {
        sprintf(fullpathname, "%s/%s", current_dir, archive_entry_pathname(entry));
        archive_entry_set_pathname(entry, fullpathname);
        archive_write_header(ext, entry);
        len = archive_read_data(a, buff, sizeof(buff));
        while (len > 0) {
            archive_write_data(ext, buff, len);
            len = archive_read_data(a, buff, sizeof(buff));
        }
    }
    archive_read_free(a);
    archive_write_free(ext);
}

void change_tab(void)
{
    active = (!active + cont) % MAX_TABS;
    chdir(ps[active].my_cwd);
}