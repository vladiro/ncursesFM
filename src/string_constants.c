const char *editor_missing = "You have to specify a valid editor in config file.";

const char *generic_error = "A generic error occurred. Check log.";

const char *info_win_str[] = {"?: ", "I: ", "E: "};

const char *arch_ext[] = {".tgz", ".tar.gz", ".zip", ".rar", ".xz", ".ar"};

const char *sorting_str[] = {"Files will be sorted alphabetically now.",
                             "Files will be sorted by size now.",
                             "Files will be sorted by last access now.",
                             "Files will be sorted by type now."};

const char *too_many_bookmarks = "Too many bookmarks. Max 30.";
const char *bookmarks_add_quest = "Adding current file to bookmarks. Proceed? Y/n:> ";
const char *bookmarks_rm_quest = "Removing current file from bookmarks. Proceed? Y/n:> ";
const char *bookmarks_xdg_err = "You cannot remove xdg defined user dirs.";
const char *bookmarks_file_err = "Could not open bookmarks file.";
const char *bookmark_added = "Added to bookmarks!";
const char *bookmarks_rm = "Removed fron bookmarks!";
const char *no_bookmarks = "No bookmarks found.";
const char *inexistent_bookmark = "It seems current bookmark no longer exists. Remove it? Y/n:>";

const char *file_selected = "This file is already selected. Cancel its selection before.";
const char *no_selected_files = "There are no selected files.";
const char *file_sel[] = {"File selected.", "File deleted from selected list.", "File deleted from selected list. Selected list empty."};

const char *sure = "Are you serious? y/N:> ";

const char *big_file = "This file is quite big. Do you really want to open it? y/N:> ";

const char *already_searching = "There's already a search in progress. Wait for it.";
const char *search_mem_fail = "Stopping search as no more memory can be allocated.";
const char *search_insert_name = "Insert filename to be found, at least 5 chars, max 20 chars.:> ";
const char *search_archives = "Do you want to search in archives too? y/N:> ";
const char *lazy_search = "Do you want a lazy search (less precise)? y/N:>";
const char *searched_string_minimum = "At least 5 chars...";
const char *too_many_found = "Too many files found; try with a larger string.";
const char *no_found = "No files found.";
const char searching_mess[2][80] = {"Searching...", "Search finished. Press f anytime to view the results."};

#ifdef LIBCUPS_PRESENT
const char *print_question = "Do you really want to print this file? Y/n:> ";
const char *print_ok = "Print job added.";
const char *print_fail = "No printers available.";
#endif

const char *archiving_mesg = "Insert new file name (defaults to first entry name):> ";

const char *ask_name = "Insert new name:> ";

const char *extr_question = "Do you really want to extract this archive? Y/n:> ";

const char *thread_job_mesg[] = {"Moving...", "Pasting...", "Removing...", "Archiving...", "Extracting..."};
const char *thread_str[] = {"Every file has been moved.", "Every files has been copied.", "File/dir removed.", "The archive is ready.", "Succesfully extracted."};
const char *thread_fail_str[] = {"Could not move", "Could not paste.", "Could not remove every file.", "Could not archive.", "Could not extract every file."};
const char *short_msg[] = {"File created.", "Dir created.", "File renamed."};

const char *selected_mess = "There are selected files.";

const char *thread_running = "There's already a thread working. This thread will be queued.";
const char *quit_with_running_thread = "Queued jobs still running. Waiting...";

#ifdef SYSTEMD_PRESENT
const char *pkg_quest = "Do you really want to install this package? y/N:> ";
const char *install_th_wait = "Waiting for package installation to finish...";
const char *package_warn = "Currently there is no check against wrong package arch: it will crash packagekit and ncursesfm.";
const char *device_mode_str =  "Choose your desired device to (un)mount it";
#endif

const char *bookmarks_mode_str = "Bookmarks:";

#ifdef SYSTEMD_PRESENT
const int HELPER_HEIGHT[] = {16, 7, 8, 8, 8};
#else
const int HELPER_HEIGHT[] = {14, 6, 8, 8, 8};
#endif

const char helper_string[][16][150] =
{
    {
        {"Remember: every shortcut in ncursesFM is case insensitive."},
#ifdef SYSTEMD_PRESENT
        {"Enter -> surf between folders or to open files."},
        {"It will eventually (un)mount your ISO files or install your distro downloaded packages."},
#else
        {"Enter -> surf between folders or to open files."},
#endif
        {", -> enable fast browse mode: it lets you jump between files by just typing their name."},
        {"PG_UP/DOWN -> jump straight to first/last file. i -> check current file fullname."},
        {"h -> trigger the showing of hidden files; s -> see stat about files in current folder."},
        {". -> change files/dirs sorting function: alphabetically (default), by size, by last modified or by type."},
        {"Space -> select files. Once more to remove the file from selected files."},
#ifdef LIBCUPS_PRESENT
        {"v/x -> paste/cut, b -> compress, r -> remove, z -> extract, p -> print a file."},
#else
        {"v/x -> paste/cut, b -> compress, r -> remove, z -> extract."},
#endif
        {"g -> switch to bookmarks window. j -> trigger image previewer, e -> add current file to bookmarks"},
        {"o -> rename current file/dir; n/d -> create new file/dir. f -> search for a file."},
        {"t -> create second tab. w -> close second tab. Arrow keys -> switch between tabs."},
#ifdef SYSTEMD_PRESENT
        {"m -> switch to devices tab."},
#endif
        {"ESC -> quit."}
    }, {
        {"Remember: every shortcut in ncursesFM is case insensitive."},
#ifdef SYSTEMD_PRESENT
        {"Enter -> surf between folders or to open files."},
        {"It will eventually (un)mount your ISO files or install your distro downloaded packages."},
#else
        {"Enter -> surf between folders or to open files."},
#endif
        {"Just start typing your desired filename, to move right to its position."},
        {"ESC -> leave fast browse mode."}
    }, {
        {"Remember: every shortcut in ncursesFM is case insensitive."},
        {"s -> see stat about files in current folder. i -> check current file fullname."},
        {"t -> create second tab. w -> close tab. Arrow keys -> switch between tabs."},
        {"e -> remove selected file from bookmarks."},
        {"enter on a bookmarks will move to the folder/file selected."},
        {"ESC -> leave bookmarks mode."}
    }, {
        {"Remember: every shortcut in ncursesFM is case insensitive."},
        {"s -> see stat about files in current folder. i -> check current file fullname."},
        {"t -> create second tab. w -> close tab. Arrow keys -> switch between tabs."},
        {"enter on a file to move to its folder and highight it."},
        {"enter on a folder to move inside it."},
        {"ESC -> leave search mode."}
    }, {
        {"Remember: every shortcut in ncursesFM is case insensitive."},
        {"s -> see stat about files in current folder. i -> check current file fullname."},
        {"t -> create second tab. w -> close tab. Arrow keys -> switch between tabs."},
        {"m -> (un)mount current device"},
        {"enter -> move to current device mountpoint, mounting it if necessary."},
        {"ESC -> leave device mode."}
    }
};
