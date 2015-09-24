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

#include "helper_functions.h"

struct thread_mesg {
    const char *str;
    int line;
};

static void free_running_h(void);
static thread_job_list *add_thread(thread_job_list *h, int type, int (*f)(void));
#ifdef SYSTEMD_PRESENT
static void inhibit_suspend(void);
static void free_bus(void);
#endif
static void init_thread_helper(const char *temp);
static void copy_selected_files(void);
static void *execute_thread(void *x);
static void check_refresh(void);
static void free_thread_job_list(thread_job_list *h);
static void quit_thread_func(void);
static void sig_handler(int signum);

static pthread_t th;
static thread_job_list *current_th; // current_th: ptr to latest elem in thread_l list
static struct thread_mesg thread_m;
#ifdef SYSTEMD_PRESENT
static sd_bus *bus;
static sd_bus_message *m;
#endif

/*
 * Given a filename, the function checks:
 * 1) if there is a '.' starting substring in it (else it returns 0)
 * 2) for each of the *ext strings, checks if last "strlen(*ext)" chars in filename are equals to *ext. If yes, returns 1.
 */
int is_archive(const char *filename)
{
    const char *ext[] = {".tgz", ".tar.gz", ".zip", ".rar", ".xz", ".ar"};
    int i = 0, len = strlen(filename);

    if (strrchr(filename, '.')) {
        while (i < 6) {
            if (strcmp(filename + len - strlen(ext[i]), ext[i]) == 0) {
                return 1;
            }
            i++;
        }
    }
    return 0;
}

void *safe_malloc(ssize_t size, const char *str)
{
    void *ptr = NULL;

    if (!(ptr = malloc(size))) {
        print_info(str, ERR_LINE);
    }
    return ptr;
}

/*
 * Gived a full path searches the string test in the mimetype, and if found returns 1;
 */
int get_mimetype(const char *path, const char *test)
{
    int ret = 0;
    const char *mimetype;
    magic_t magic_cookie;

    magic_cookie = magic_open(MAGIC_MIME_TYPE);
    magic_load(magic_cookie, NULL);
    mimetype = magic_file(magic_cookie, path);
    if (strstr(mimetype, test)) {
        ret = 1;
    }
    magic_close(magic_cookie);
    return ret;
}

/*
 * Adds a job to the thread_job_list (thread_job_list list).
 * current_th will always point to the newly created job (ie the last job to be executed).
 */
static thread_job_list *add_thread(thread_job_list *h, int type, int (*f)(void))
{
    if (h) {
        h->next = add_thread(h->next, type, f);
    } else {
        if (!(h = safe_malloc(sizeof(struct thread_list), generic_mem_error))) {
            return NULL;
        }
        num_of_jobs++;
        h->selected_files = NULL;
        h->next = NULL;
        h->f = f;
        strcpy(h->full_path, ps[active].my_cwd);
        h->type = type;
        h->num = num_of_jobs;
        current_th = h;
    }
    return h;
}

/*
 * This function will move thread_job_list head to head->next, and will free old head memory and set it to NULL.
 */
void free_running_h(void)
{
    thread_job_list *tmp = thread_h;

    thread_h = thread_h->next;
    if (tmp->selected_files)
        free_copied_list(tmp->selected_files);
    free(tmp);
    tmp = NULL;
}

/*
 * Given a type, a function and a str:
 * - checks if we have write oaccess in cwd.
 * - if type is one of {RENAME_TH, NEW_FILE_TH, CREATE_DIR_TH}, asks user the name of the new file.
 * - adds a new job to the thread_job_list
 * - Initializes the new job
 * - if there's a thread running, prints a message that the job will be queued, else starts the th to execute the job.
 */
void init_thread(int type, int (*f)(void))
{
    char temp[NAME_MAX];

    if ((type >= NEW_FILE_TH) && (type <= RENAME_TH)) {
        ask_user(ask_name, temp, NAME_MAX, 0);
        if (!strlen(temp)) {
            return;
        }
    }
    thread_h = add_thread(thread_h, type, f);
    init_thread_helper(temp);
    if (num_of_jobs > 1) {
        print_info(thread_running, INFO_LINE);
    } else {
#ifdef SYSTEMD_PRESENT
        if (config.inhibit) {
            inhibit_suspend();
        }
#endif
        thread_m.str = NULL;
        thread_m.line = INFO_LINE;
        pthread_create(&th, NULL, execute_thread, NULL);
    }
}

#ifdef SYSTEMD_PRESENT
static void inhibit_suspend(void)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        print_info(bus_error, ERR_LINE);
        return;
    }
    r = sd_bus_call_method(bus,
                           "org.freedesktop.login1",
                           "/org/freedesktop/login1",
                           "org.freedesktop.login1.Manager",
                           "Inhibit",
                            &error,
                            &m,
                           "ssss",
                           "sleep:idle",
                           "ncursesFM",
                           "Job in process...",
                           "block");
    if (r < 0) {
        print_info(error.message, ERR_LINE);
    } else {
        r = sd_bus_message_read(m, "h", NULL);
        if (r < 0) {
            print_info(strerror(-r), ERR_LINE);
        }
    }
    sd_bus_error_free(&error);
}

static void free_bus(void)
{
    if (m) {
        sd_bus_message_unref(m);
        m = NULL;
    }
    if (bus) {
        sd_bus_unref(bus);
        bus = NULL;
    }
}
#endif

/*
 * Just a helper thread for init_thread(); it sets some members of thread_job_list struct depending of current_th->type
 */
static void init_thread_helper(const char *temp)
{
    char name[NAME_MAX];

    if (current_th->type >= RENAME_TH) {
        strcpy(current_th->filename, ps[active].nl[ps[active].curr_pos]);
        if (current_th->type == RENAME_TH) {
            strcpy(name, ps[active].my_cwd);
            sprintf(name + strlen(name), "/%s", temp);
            current_th->selected_files = select_file(current_th->selected_files, name);
        }
    } else {
        if (current_th->type >= ARCHIVER_TH) {
            strcpy(current_th->filename, current_th->full_path);
            if ((current_th->type == NEW_FILE_TH) || (current_th->type == CREATE_DIR_TH)) {
                strcat(current_th->filename, "/");
                strcat(current_th->filename, temp);
            }
        }
        if (current_th->type <= ARCHIVER_TH) {
            copy_selected_files();
            if (current_th->type == ARCHIVER_TH) {
                ask_user(archiving_mesg, name, NAME_MAX, 0);
                if (!strlen(name)) {
                    strcpy(name, strrchr(current_th->selected_files->name, '/') + 1);
                }
                sprintf(current_th->filename + strlen(current_th->filename), "/%s.tgz", name);
            }
        }
    }
}

/*
 * Helper function that will copy file_list *selected to current_th->selected_files, then will free selected.
 */
static void copy_selected_files(void)
{
    file_list *tmp = selected;

    while (tmp) {
        current_th->selected_files = select_file(current_th->selected_files, tmp->name);
        tmp = tmp->next;
    }
    free_copied_list(selected);
    selected = NULL;
}

/*
 * First check if we come from a recursive call, then frees previously finished job and print its completion mesg.
 * If thread_h is not NULL, the th has another job waiting for it, so we run it, else we finished our work: num_of_jobs = 0.
 */
static void *execute_thread(void *x)
{
    signal(SIGINT, sig_handler);
    print_info(thread_m.str, thread_m.line);
    if (thread_h) {
        if (thread_h->f() == -1) {
            thread_m.str = thread_fail_str[thread_h->type];
            thread_m.line = ERR_LINE;
        } else {
            thread_m.str = thread_str[thread_h->type];
            thread_m.line = INFO_LINE;
            check_refresh();
        }
        free_running_h();
        return execute_thread(NULL);
    }
    num_of_jobs = 0;
#ifdef SYSTEMD_PRESENT
    if (config.inhibit) {
        free_bus();
    }
#endif
    pthread_detach(pthread_self()); // to avoid stupid valgrind errors
    return NULL;
}

static void check_refresh(void)
{
    int i;

    for (i = 0; i < cont; i++) {
        if (strcmp(ps[i].my_cwd, thread_h->full_path) == 0) {
            ps[i].needs_refresh = FORCE_REFRESH;
        } else {
            ps[i].needs_refresh = REFRESH;
        }
    }
}

void free_copied_list(file_list *h)
{
    if (h->next)
        free_copied_list(h->next);
    free(h);
}

int remove_from_list(const char *name)
{
    file_list *temp = NULL, *tmp = selected;

    if (strcmp(name, tmp->name) == 0) {
        selected = selected->next;
        free(tmp);
        return 1;
    }
    while(tmp->next) {
        if (strcmp(name, tmp->next->name) == 0) {
            temp = tmp->next;
            tmp->next = tmp->next->next;
            free(temp);
            return 1;
        }
        tmp = tmp->next;
    }
    return 0;
}

file_list *select_file(file_list *h, const char *str)
{
    if (h) {
        h->next = select_file(h->next, str);
    } else {
        if (!(h = safe_malloc(sizeof(struct list), generic_mem_error))) {
            return NULL;
        }
        strcpy(h->name, str);
        h->next = NULL;
    }
    return h;
}

void free_everything(void)
{
    quit_thread_func();
    if (selected) {
        free_copied_list(selected);
    }
}

void quit_thread_func(void)
{
    char c;

    if (thread_h) {
        ask_user(quit_with_running_thread, &c, 1, 'y');
        if (c == 'y') {
            pthread_join(th, NULL);
        } else {
            pthread_kill(th, SIGINT);
        }
    }
}

void free_thread_job_list(thread_job_list *h)
{
    if (h->next) {
        free_thread_job_list(h->next);
    }
    if (h->selected_files)
        free_copied_list(h->selected_files);
    free(h);
}

static void sig_handler(int signum)
{
    free_thread_job_list(thread_h);
    pthread_exit(NULL);
}

#if defined(LIBUDEV_PRESENT) && (SYSTEMD_PRESENT)
void mount_fs(const char *str)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *mess = NULL;
    sd_bus *mount_bus = NULL;
    const char *path;
    char obj_path[80] = "/org/freedesktop/UDisks2/block_devices/";
    char e[80] = "Failed to issue method call: ", success[PATH_MAX] = "Mounted in ";
    char method[10];
    int r, mount;

    mount = is_mounted(str);
    if (!mount) {
        strcpy(method, "Mount");
    } else {
        strcpy(method, "Unmount");
    }
    r = sd_bus_open_system(&mount_bus);
    if (r < 0) {
        print_info(bus_error, ERR_LINE);
        return;
    }
    strcat(obj_path, strrchr(str, '/') + 1);
    r = sd_bus_call_method(mount_bus,
                       "org.freedesktop.UDisks2",
                       obj_path,
                       "org.freedesktop.UDisks2.Filesystem",
                       method,
                       &error,
                       &mess,
                       "a{sv}",
                       NULL);
    if (r < 0) {
        strcat(e, error.message);
        print_info(e, ERR_LINE);
    } else {
        if (!mount) {
            sd_bus_message_read(mess, "s", &path);
            strcat(success, path);
            print_info(success, INFO_LINE);
        } else {
            print_info("Unmounted", INFO_LINE);
        }
    }
    sd_bus_error_free(&error);
    sd_bus_message_unref(mess);
    sd_bus_unref(mount_bus);
}

int is_mounted(const char *dev_path)
{
    FILE *mtab = NULL;
    struct mntent *part = NULL;
    int is_mounted = 0;

    if ((mtab = setmntent ("/etc/mtab", "r"))) {
        while ((part = getmntent(mtab))) {
            if ((part->mnt_fsname) && (!strcmp(part->mnt_fsname, dev_path))) {
                is_mounted = 1;
            }
        }
        endmntent ( mtab);
    }
    return is_mounted;
}
#endif