#ifdef __FreeBSD__
#include <sys/param.h>
#endif

#include <sys/param.h>
#include <stdio.h>
#include <time.h>
#include <argp.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h> // gia to onoma katalogou & vasiko onoma
#include <sys/stat.h>
#include <fcntl.h>
#include <fnmatch.h> // gia antistichisi motivou onomatos
#include <dirent.h>  // gia tis leitourgies eisagogeis
#include <lkl.h>     // pirinas tis vivliothikis Linux Kernel
#include <lkl_host.h> //Leitourgies ipodochis LKL
#include <stdarg.h>  // gia metavlites sinartisis(logging)

// kentriki leitourgia gia log minimaton
static FILE *g_application_log_file = NULL;

/**
 * archikopoiei to sistima katagrafis anoigontas ena archio katagrafis
 * prospathei na anixi prota to /tmp/lkl_cptofs.log kaia stin sinechia to /tmp/lkl_test_alternate.log
 * termatizei se periptosi apotichias anigmatos opoioudipote archiou katagrafis
 */
void log_init(void) {
    g_application_log_file = fopen("/tmp/lkl_cptofs.log", "a");
    if (!g_application_log_file) {
        g_application_log_file = fopen("/tmp/lkl_test_alternate.log", "a");
        if (!g_application_log_file) {
            fprintf(stderr, "Fatal Error: Unable to open or create any log file.\n");
            exit(EXIT_FAILURE);
        } else {
            fprintf(stderr, "Warning: Using alternate log file: /tmp/lkl_test_alternate.log\n");
        }
    } else {
        fprintf(stderr, "Info: Log file opened successfully: /tmp/lkl_cptofs.log\n");
    }
}

/**
 * morfopoiri kai grafei ena minima sto archio katagrafis me chroniko orio(timestamp) kai epipedo(level) 
 * level: to epipedo katagrafis (p.x,"INFO", "WARN", "ERROR")
 * format: i simvoloseira morfopoihshs gia to minima
 * args: ta metavlita orismata gia tin simvoloseira morfopoihshs 
 */
void log_message(const char *level, const char *format, va_list args) {
    if (!g_application_log_file) return; // Do nothing if log file isn't open

    time_t current_time;
    time(&current_time);
    char time_string_buffer[20];
    strftime(time_string_buffer, sizeof(time_string_buffer), "%Y-%m-%d %H:%M:%S", localtime(&current_time));

    // ektiposi chronikou oriou(timestamp) kai epipedou katagrafeis(log level)
    fprintf(g_application_log_file, "[%s] [%s] ", time_string_buffer, level);

    // ektiposi tou pragmatikou onomatos chrisimpoiontas tin morfi kai ta orismata
    va_list args_copy;
    va_copy(args_copy, args); // Copy args because vfprintf consumes it
    vfprintf(g_application_log_file, format, args_copy);
    va_end(args_copy);

    fprintf(g_application_log_file, "\n"); // Add a newline
    fflush(g_application_log_file); // Flush the buffer to ensure immediate write
}

/**
 * katagrafei ena enimerotiko minima 
 * format: i simvoloseira morfopoihshs gia to minima
 * Variadic: orismata
 */
void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message("INFO", format, args);
    va_end(args);
}

/**
 * grafei ena proidopoihtiko minima 
 */
void log_warn(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message("WARN", format, args);
    va_end(args);
}

/**
 * grafei ena proidopoihtiko minima 
 */
void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message("ERROR", format, args);
    va_end(args);
}

//simvolosires tekmiriosis gia argp
static const char doc_copy_to_fs[] = "Copy files to a filesystem image (host to LKL FS)";
static const char doc_copy_from_fs[] = "Copy files from a filesystem image (LKL FS to host)";
static const char args_doc_copy_to_fs[] = "-t fstype -i fsimage path... fs_path";
static const char args_doc_copy_from_fs[] = "-t fstype -i fsimage fs_path... path";

// epilogi orismon gia tis grammes entolon argp
static struct argp_option options[] = {
    {"enable-printk", 'p', 0, 0, "show Linux printks"},
    {"partition", 'P', "int", 0, "partition number"},
    {"filesystem-type", 't', "string", 0, "select filesystem type - mandatory"},
    {"filesystem-image", 'i', "string", 0, "path to the filesystem image - mandatory"},
    {"owner", 'o', "int", 0, "owner of the destination files"},
    {"group", 'g', "int", 0, "group of the destination files"},
    {"selinux", 's', "string", 0, "selinux attributes for destination"},
    {0}, // Sentinel to mark the end of the options array
};

// domi gia tin kratisi enotlon gia to ergalio antigrafis
static struct cl_args {
    int enable_printk;           // gia energopoiisi minimaton sto linux
    int partition_number;        // arithmos gia to mount
    const char *filesystem_type; // tipos archion(p.x, "ext4", "xfs")
    const char *filesystem_image_path; // diadormi tou sistimatos archio pou vriksete i eikona
    int num_paths;               // arithmos diadromon
    char **file_paths;           // pinakas diadromon
    const char *selinux_context; // simvolosira perivallontos asfaleias SELinux 
    uid_t owner_uid;             // User ID gia prosopika archia proorismou 
    gid_t group_gid;             // Group ID gia omadika archia proorismou
} parsed_command_line_arguments;

static int is_copy_to_filesystem = 0; // Flag: 1 if copying to FS, 0 if copying from FS

/**
 * Parser: sinartisi gai epiloges grammes entolon
 * key: to kleidi epilogis charaktiron
 * arg: ta orismata pou sindeonte me to kleidi epologis
 * state: diktis tis domis argp_state
 * return 0 se periptosi epitichia kai ARGP_ERR_UNKNOWN se periptosi pou to key einai agnosto
 */
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct cl_args *cla = state->input; // Get pointer to our arguments structure

    switch (key) {
        case 'p':
            cla->enable_printk = 1;
            break;
        case 'P':
            cla->partition_number = atoi(arg);
            break;
        case 't':
            cla->filesystem_type = arg;
            break;
        case 'i':
            cla->filesystem_image_path = arg;
            break;
        case 's':
            cla->selinux_context = arg;
            break;
        case 'o':
            cla->owner_uid = atoi(arg);
            break;
        case 'g':
            cla->group_gid = atoi(arg);
            break;
        case ARGP_KEY_ARG:
            // Capture all remaining arguments. The last one is the destination,
            // and everything before it are sources, similar to 'cp'.
            cla->file_paths = &state->argv[state->next - 1];
            cla->num_paths = state->argc - state->next + 1;
            state->next = state->argc; // Stop further argument parsing
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

// argp domi gia 'cptofs' (antigrafi apo filesystem)
static struct argp argp_cptofs_config = {
    .options = options,
    .parser = parse_opt,
    .args_doc = args_doc_copy_to_fs,
    .doc = doc_copy_to_fs,
};

// argp domi gia 'cpfromfs' (anigrafi apo filesystem)
static struct argp argp_cpfromfs_config = {
    .options = options,
    .parser = parse_opt,
    .args_doc = args_doc_copy_from_fs,
    .doc = doc_copy_from_fs,
};

static int search_directory_for_files(const char *source_dir_path, const char *destination_dir_path,
                                      const char *filename_match_pattern, uid_t owner_id, gid_t group_id);

/**
 * anigi ena archio pigiaou kofika gai anagnvsi eite ston kentriko ipologisti eite entos tou lkl 
 * path: i diadromi pros to archio
 * return File descriptor se periptosi epitichias, se periptosi sfalmatos tipose arnitiki tim
 */
static int open_source_file(const char *path) {
    int file_descriptor;

    if (is_copy_to_filesystem) { // If copying to FS (source is host)
        file_descriptor = open(path, O_RDONLY, 0);
    } else { // If copying from FS (source is LKL)
        file_descriptor = lkl_sys_open(path, LKL_O_RDONLY, 0);
    }

    if (file_descriptor < 0) {
        fprintf(stderr, "Error: Unable to open source file '%s' for reading: %s\n", path,
                is_copy_to_filesystem ? strerror(errno) : lkl_strerror(file_descriptor));
        log_error("Unable to open source file '%s' for reading: %s", path,
                  is_copy_to_filesystem ? strerror(errno) : lkl_strerror(file_descriptor));
    }

    return file_descriptor;
}

/**
 * anigei ena archio me proorismo na grapsei eite ston kentriko ipologisti eite sto sto lkl 
 * path: i diadromi tou archiou
 * mode: ta dikaiomata tou archiou
 * owner_id:to UID tou katochou pou tha oristei
 * group_id: to GID tis omadas pou tha oristei
 * File descriptor se periptosi epitchias,ektiposi arnitkis timis se periptosi lathous
 */
static int open_destination_file(const char *path, int mode, uid_t owner_id, gid_t group_id) {
    int file_descriptor;
    int return_code;

    if (is_copy_to_filesystem) { // If copying to FS (destination is LKL)
        file_descriptor = lkl_sys_open(path, LKL_O_RDWR | LKL_O_TRUNC | LKL_O_CREAT, mode);
    } else { // If copying from FS (destination is host)
        file_descriptor = open(path, O_RDWR | O_TRUNC | O_CREAT, mode);
    }

    if (file_descriptor < 0) {
        fprintf(stderr, "Error: Unable to open destination file '%s' for writing: %s\n", path,
                is_copy_to_filesystem ? lkl_strerror(file_descriptor) : strerror(errno));
        log_error("Unable to open destination file '%s' for writing: %s", path,
                  is_copy_to_filesystem ? lkl_strerror(file_descriptor) : strerror(errno));
        return file_descriptor; // Return the error code directly
    }

    if (owner_id != (uid_t)-1 && group_id != (gid_t)-1) {
        if (is_copy_to_filesystem) { // Change owner/group in LKL
            return_code = lkl_sys_fchown(file_descriptor, owner_id, group_id);
        } else { // Change owner/group on host
            return_code = fchown(file_descriptor, owner_id, group_id);
        }
        if (return_code) {
            fprintf(stderr, "Error: Unable to set owner/group on '%s': %s\n", path,
                    is_copy_to_filesystem ? lkl_strerror(return_code) : strerror(errno));
            log_error("Unable to set owner/group on '%s': %s", path,
                      is_copy_to_filesystem ? lkl_strerror(return_code) : strerror(errno));
            // Close the partially opened file descriptor before returning error
            if (is_copy_to_filesystem) lkl_sys_close(file_descriptor); else close(file_descriptor);
            return -1;
        }
    }
    if (parsed_command_line_arguments.selinux_context && is_copy_to_filesystem) {
        return_code = lkl_sys_fsetxattr(file_descriptor, "security.selinux",
                                      parsed_command_line_arguments.selinux_context,
                                      strlen(parsed_command_line_arguments.selinux_context), 0);
        if (return_code) {
            fprintf(stderr, "Error: Unable to set selinux attribute on '%s': %s\n", path, lkl_strerror(return_code));
            log_error("Unable to set selinux attribute on '%s': %s", path, lkl_strerror(return_code));
            // This is often a non-fatal error, so we might just log and continue
        }
    }

    return file_descriptor;
}

/**
 *diavazei dedomena apo ena archio proorismou
 *fd: perigrafei ena archio
 *buffer: to buffer sto opio tha gini i anagnosi
 *length: o megistos arithmos byte pros anagnosi
 *return ton  arithmo byte pou echoun diavastei, tipose minima lathous se error 
 */
static int read_from_source(int fd, char *buffer, int length) {
    int bytes_read;

    if (is_copy_to_filesystem) { // Read from host file
        bytes_read = read(fd, buffer, length);
    } else { // Read from LKL file
        bytes_read = lkl_sys_read(fd, buffer, length);
    }

    if (bytes_read < 0) {
        fprintf(stderr, "Error reading from source file: %s\n",
                is_copy_to_filesystem ? strerror(errno) : lkl_strerror(bytes_read));
        log_error("Error reading from source file: %s",
                  is_copy_to_filesystem ? strerror(errno) : lkl_strerror(bytes_read));
    }
    return bytes_read;
}

/**
 *grafei ta dedomena apo ena archio proorismou
 *fd: perigrafei ena archio
 *buffer: to buffer sto opio tha gini i anagnosi
 *length: o megistos arithmos byte pros anagnosi
 *return ton arithmo byte pou echoun diavastei, tipose minima lathous se error 
 */
static int write_to_destination(int fd, char *buffer, int length) {
    int bytes_written;

    if (is_copy_to_filesystem) { // Write to LKL file
        bytes_written = lkl_sys_write(fd, buffer, length);
    } else { // Write to host file
        bytes_written = write(fd, buffer, length);
    }

    if (bytes_written < 0) {
        fprintf(stderr, "Error writing to destination file: %s\n",
                is_copy_to_filesystem ? lkl_strerror(bytes_written) : strerror(errno));
        log_error("Error writing to destination file: %s",
                  is_copy_to_filesystem ? lkl_strerror(bytes_written) : strerror(errno));
    }
    return bytes_written;
}

/**
 *kleini ena perigrafea archiou pigieou kodika
 *fd: o perigrafeas archiou pou tha kleisei
 */
static void close_source_file(int fd) {
    if (is_copy_to_filesystem) {
        close(fd);
    } else {
        lkl_sys_close(fd);
    }
}

/**
 *kleini ena perigrafea archiou pigieou kodika
 *fd: o perigrafeas archiou pou tha kleisei
 */
static void close_destination_file(int fd) {
    if (is_copy_to_filesystem) {
        lkl_sys_close(fd);
    } else {
        close(fd);
    }
}

/**
 *antigrafei to periechomeno enos archiou proelefsis se ena archio proorismou
 * source_path: i diadromi apo to archio proelefsis
 * destination_path: i diadromi tou archiou proorismou 
 * file_mode: ta dikaiomata tou neou archiou
 * owner_id: to UID gia to neo archio
 * group_id: to GID gia to neo archio
 * return 0 se periptosi apitichias,arnitiko se periptosi sfalamtos
 */
static int copy_regular_file(const char *source_path, const char *destination_path,
                             int file_mode, uid_t owner_id, gid_t group_id) {
    long bytes_read_count, bytes_to_write, bytes_written_count;
    char data_buffer[4096], *buffer_ptr;
    int overall_return_code = 0;
    int source_fd, destination_fd;

    source_fd = open_source_file(source_path);
    if (source_fd < 0) {
        return source_fd;
    }

    destination_fd = open_destination_file(destination_path, file_mode, owner_id, group_id);
    if (destination_fd < 0) {
        close_source_file(source_fd); // klini to archio porelefsis
        return destination_fd;
    }

    do {
        bytes_read_count = read_from_source(source_fd, data_buffer, sizeof(data_buffer));

        if (bytes_read_count > 0) {
            buffer_ptr = data_buffer;
            bytes_to_write = bytes_read_count;
            do {
                bytes_written_count = write_to_destination(destination_fd, buffer_ptr, bytes_to_write);

                if (bytes_written_count < 0) {
                    overall_return_code = bytes_written_count;
                    goto cleanup_files; 
                }

                bytes_to_write -= bytes_written_count;
                buffer_ptr += bytes_written_count;

            } while (bytes_to_write > 0);
        }

        if (bytes_read_count < 0) {
            overall_return_code = bytes_read_count; 
        }

    } while (bytes_read_count > 0); // na sinechisi mechri to telos i mechri na vgalei error

cleanup_files:
    close_source_file(source_fd);
    close_destination_file(destination_fd);

    return overall_return_code;
}

/**
 * anakta plirofories statistikon gia mia diadromi proelefsis
 * path: i diadromi pros to archio
 * file_type: tipos archiou gai tin apothikefsi
 * file_mode_out: tipos dikaiomaton tou archiou
 * file_size_out: to megethos tou archiouy
 * modification_time_out: pote tropopoiithike to archio
 * access_time_out: dichni ton chrono pote epitrapike prosvasi sto archio
 * return 0 se periptosi epitichias alios tipose minima lathous
 */
static int stat_source_path(const char *path, unsigned int *file_type_out,
                            unsigned int *file_mode_out, long long *file_size_out,
                            struct lkl_timespec *modification_time_out,
                            struct lkl_timespec *access_time_out) {
    struct stat host_stat;
    struct lkl_stat lkl_stat_info;
    int return_code;

    if (is_copy_to_filesystem) { // pare statistika apo to filesystem
        return_code = lstat(path, &host_stat);
        if (file_type_out)
            *file_type_out = host_stat.st_mode & S_IFMT;
        if (file_mode_out)
            *file_mode_out = host_stat.st_mode & ~S_IFMT;
        if (file_size_out)
            *file_size_out = host_stat.st_size;
        if (modification_time_out) {
            modification_time_out->tv_sec = host_stat.st_mtim.tv_sec;
            modification_time_out->tv_nsec = host_stat.st_mtim.tv_nsec;
        }
        if (access_time_out) {
            access_time_out->tv_sec = host_stat.st_atim.tv_sec;
            access_time_out->tv_nsec = host_stat.st_atim.tv_nsec;
        }
    } else { // pare statistika apo to LKL filesystem
        return_code = lkl_sys_lstat(path, &lkl_stat_info); 
        if (file_type_out)
            *file_type_out = lkl_stat_info.st_mode & S_IFMT;
        if (file_mode_out)
            *file_mode_out = lkl_stat_info.st_mode & ~S_IFMT;
        if (file_size_out)
            *file_size_out = lkl_stat_info.st_size;
        if (modification_time_out) {
            modification_time_out->tv_sec = lkl_stat_info.lkl_st_mtime;
            modification_time_out->tv_nsec = lkl_stat_info.st_mtime_nsec;
        }
        if (access_time_out) {
            access_time_out->tv_sec = lkl_stat_info.lkl_st_atime;
            access_time_out->tv_nsec = lkl_stat_info.st_atime_nsec;
        }
    }

    if (return_code) {
        fprintf(stderr, "Error: stat_source_path('%s') failed: %s\n",
                path, is_copy_to_filesystem ? strerror(errno) : lkl_strerror(return_code));
        log_error("stat_source_path('%s') failed: %s",
                  path, is_copy_to_filesystem ? strerror(errno) : lkl_strerror(return_code));
    }

    return return_code;
}

/**
 * dimiourgei ena fakelo (dierectory) sto epithimito proorismo eite ston kentriko ipologisti(host) eite ston lkl
 * path: i diadromi pros to archio
 * mode: ta dikeomata tou neou fakelou 
 * owner_id: to UID gia to neo archio
 * group_id: to GID gia to neo archio
 * return 0 se periptosi epitichias i ena o katalogos eiparchei idi, tipose minima lathous se periptosi lathous
 */
static int create_destination_directory(const char *path, unsigned int mode, uid_t owner_id, gid_t group_id) {
    int return_code;

    if (is_copy_to_filesystem) { // dimiourgise to directory ston LKL
        return_code = lkl_sys_mkdir(path, mode);
        if (return_code == -LKL_EEXIST) { // If directory already exists, it's not an error
            return_code = 0;
        }
    } else { // dimiourgise to directory ston host
        return_code = mkdir(path, mode);
        if (return_code < 0 && errno == EEXIST) { // If directory already exists, it's not an error
            return_code = 0;
        }
    }
    if (owner_id != (uid_t)-1 || group_id != (gid_t)-1) {
        if (is_copy_to_filesystem) { // allaxe ton/to owner/group ston LKL
            return_code = lkl_sys_chown(path, owner_id, group_id);
        } else { // allaxe ton/to owner/group ston host
            return_code = chown(path, owner_id, group_id);
        }
        if (return_code) {
            fprintf(stderr, "Error: Unable to chown directory '%s': %s\n",
                    path, is_copy_to_filesystem ? strerror(errno) : lkl_strerror(return_code));
            log_error("Unable to chown directory '%s': %s",
                      path, is_copy_to_filesystem ? strerror(errno) : lkl_strerror(return_code));
            return return_code;
        }
    }

    if (return_code) {
        fprintf(stderr, "Error: Unable to create directory '%s': %s\n",
                path, is_copy_to_filesystem ? strerror(errno) : lkl_strerror(return_code));
        log_error("Unable to create directory '%s': %s",
                  path, is_copy_to_filesystem ? strerror(errno) : lkl_strerror(return_code));
    }

    return return_code;
}

/**
 * diavazei ton stocho enos simvolikou sindesmou apo tin archiki topothesia
 * source_symlink_path: i diadromi pros ton simvoliko proorismo
 * output_buffer: endiamesi mnimi gia tin apothikefsi tou stochou sindesis
 * buffer_size: megethos tis endiamesis mnimis exodou
 * return ton arithmo ton byte ston teliko proorismo
 */
static int read_source_symlink(const char *source_symlink_path, char *output_buffer, int buffer_size) {
    int bytes_read;

    if (is_copy_to_filesystem) { // Read symlink on host
        bytes_read = readlink(source_symlink_path, output_buffer, buffer_size);
    } else { // Read symlink in LKL
        bytes_read = lkl_sys_readlink(source_symlink_path, output_buffer, buffer_size);
    }

    if (bytes_read < 0) {
        fprintf(stderr, "Error: Unable to read symlink target for '%s': %s\n", source_symlink_path,
                is_copy_to_filesystem ? strerror(errno) : lkl_strerror(bytes_read));
        log_error("Unable to read symlink target for '%s': %s", source_symlink_path,
                  is_copy_to_filesystem ? strerror(errno) : lkl_strerror(bytes_read));
    }

    return bytes_read;
}

/**
 * dimiorourgei ena simvoliko sindesmo ston proorismo 
 * destination_path: i diadromi tou neou simvolikou sindesmou 
 * link_target: o telikos stochous tou sindesmou 
 * owner_id: to UID gia to neo archio
 * group_id: to GID gia to neo archio
 * return 0 se periptosi epitichias 
 */
static int create_destination_symlink(const char *destination_path, const char *link_target,
                                      uid_t owner_id, gid_t group_id) {
    int return_code;

    if (is_copy_to_filesystem) { // dimiourgise ton simvoliko sindesmo ston LKL
        return_code = lkl_sys_symlink(link_target, destination_path);
    } else { // dimiourgise ton simvoliko sindesmo ston host
        return_code = symlink(link_target, destination_path);
    }

    // Set owner and group if specified (using lchown for symlinks on host)
    if (owner_id != (uid_t)-1 || group_id != (gid_t)-1) {
        if (is_copy_to_filesystem) { // Change owner/group in LKL (AT_SYMLINK_NOFOLLOW is important)
            return_code = lkl_sys_fchownat(LKL_AT_FDCWD, destination_path, owner_id, group_id, LKL_AT_SYMLINK_NOFOLLOW);
        } else { 
            return_code = lchown(destination_path, owner_id, group_id);
        }
        if (return_code) {
            fprintf(stderr, "Error: Unable to chown symbolic link '%s': %s\n",
                    destination_path, is_copy_to_filesystem ? strerror(errno) : lkl_strerror(return_code));
            log_error("Unable to chown symbolic link '%s': %s",
                      destination_path, is_copy_to_filesystem ? strerror(errno) : lkl_strerror(return_code));
            return return_code;
        }
    }

    if (return_code) {
        fprintf(stderr, "Error: Unable to create symbolic link '%s' pointing to '%s': %s\n",
                destination_path, link_target, is_copy_to_filesystem ? lkl_strerror(return_code) : strerror(errno));
        log_error("Unable to create symbolic link '%s' pointing to '%s': %s",
                  destination_path, link_target, is_copy_to_filesystem ? lkl_strerror(return_code) : strerror(errno));
    }

    return return_code;
}

/**
 *antigrafei ena simvoliko sindesmo 
 *source_symlink_path: i diadromi pros ton simvoliko sindesmo 
 * destination_path: i teliki diadromi 
 * owner_id: to UID gia to neo archio
 * group_id: to GID gia to neo archio
 * return 0 se periptosi epitichias 
 */
static int copy_symbolic_link(const char *source_symlink_path, const char *destination_path,
                              uid_t owner_id, gid_t group_id) {
    int return_code;
    long long symlink_target_size;
    char *target_buffer = NULL;

    // pare to megethos tou symlink target
    return_code = stat_source_path(source_symlink_path, NULL, NULL, &symlink_target_size, NULL, NULL);
    if (return_code) {
        return -1; // Error already logged by stat_source_path
    }

    target_buffer = malloc(symlink_target_size + 1); // +1 for null terminator
    if (!target_buffer) {
        fprintf(stderr, "Error: Unable to allocate memory (%lld bytes) for symlink target.\n", symlink_target_size + 1);
        log_error("Unable to allocate memory (%lld bytes) for symlink target.", symlink_target_size + 1);
        return -1;
    }

    // diavase to symlink target
    long long actual_size = read_source_symlink(source_symlink_path, target_buffer, symlink_target_size);
    if (actual_size != symlink_target_size) {
        fprintf(stderr, "Error: read_source_symlink('%s') returned incorrect size: got %lld, expected %lld\n",
                source_symlink_path, actual_size, symlink_target_size);
        log_error("read_source_symlink('%s') returned incorrect size: got %lld, expected %lld",
                  source_symlink_path, actual_size, symlink_target_size);
        return_code = -1;
        goto cleanup_buffer;
    }
    target_buffer[symlink_target_size] = '\0'; // Null-terminate the string

    // dimiorgise ena symlink ston proorismo
    return_code = create_destination_symlink(destination_path, target_buffer, owner_id, group_id);
    if (return_code) {
        return_code = -1; // Propagate error
    }

cleanup_buffer:
    if (target_buffer) {
        free(target_buffer);
    }

    return return_code;
}

/**
 * epexergazete mia memonomeni katachorisi sistimatos archion
 * source_parent_dir: o goneas gia ton katalogo tis katachorisis proelefsis
 * destination_parent_dir: o goneas gia ton katalogo tis katachorisis tou proorismou
 * entry_name: to onoma tis katachorisis
 * owner_id: to UID gia to neo archio
 * group_id: to GID gia to neo archio
 * return 0 se periptosi epitichias 
 */
static int process_file_system_entry(const char *source_parent_dir, const char *destination_parent_dir,
                                      const char *entry_name, uid_t owner_id, gid_t group_id) {
    char full_source_path[PATH_MAX];
    char full_destination_path[PATH_MAX];
    struct lkl_timespec access_time, modification_time;
    unsigned int entry_type, entry_mode;
    int return_code;
    snprintf(full_source_path, sizeof(full_source_path), "%s/%s", source_parent_dir, entry_name);
    snprintf(full_destination_path, sizeof(full_destination_path), "%s/%s", destination_parent_dir, entry_name);

    return_code = stat_source_path(full_source_path, &entry_type, &entry_mode, NULL, &modification_time, &access_time);
    if (return_code) {
        // error oti eisai idi sindedemenos ston fakelo
        return return_code;
    }

    // diachirsi diaforon file types
    switch (entry_type) {
        case S_IFREG: // Regular file
            return_code = copy_regular_file(full_source_path, full_destination_path, entry_mode, owner_id, group_id);
            break;
        case S_IFDIR: // Directory
            return_code = create_destination_directory(full_destination_path, entry_mode, owner_id, group_id);
            if (return_code) {
                break; // If directory creation fails, stop
            }
            return_code = search_directory_for_files(full_source_path, full_destination_path, NULL, owner_id, group_id);
            break;
        case S_IFLNK: // Symbolic link
            return_code = copy_symbolic_link(full_source_path, full_destination_path, owner_id, group_id);
            break;
        case S_IFSOCK: // Socket
        case S_IFBLK:  // Block device
        case S_IFCHR:  // Character device
        case S_IFIFO:  // FIFO (named pipe)
        default:
            printf("Info: Skipping '%s': unsupported entry type (0x%x).\n", full_source_path, entry_type);
            log_info("Skipping '%s': unsupported entry type (0x%x).", full_source_path, entry_type);
            break; // No error for unsupported types, just skip
    }
    if (!return_code) {
        if (is_copy_to_filesystem) { // Set timestamps in LKL
            struct lkl_timespec lkl_timestamps[] = { access_time, modification_time };
            return_code = lkl_sys_utimensat(LKL_AT_FDCWD, full_destination_path,
                                            (struct __lkl__kernel_timespec *)lkl_timestamps,
                                            LKL_AT_SYMLINK_NOFOLLOW);
        } else { // Set timestamps on host
            struct timespec host_timestamps[] = {
                { .tv_sec = access_time.tv_sec, .tv_nsec = access_time.tv_nsec, },
                { .tv_sec = modification_time.tv_sec, .tv_nsec = modification_time.tv_nsec, },
            };
            return_code = utimensat(AT_FDCWD, full_destination_path, host_timestamps, AT_SYMLINK_NOFOLLOW);
        }

        if (return_code) {
            fprintf(stderr, "Warning: Failed to set timestamps for '%s': %s\n",
                    full_destination_path, is_copy_to_filesystem ? lkl_strerror(return_code) : strerror(errno));
            log_warn("Failed to set timestamps for '%s': %s",
                     full_destination_path, is_copy_to_filesystem ? lkl_strerror(return_code) : strerror(errno));
            return_code = 0; // Treat as non-fatal for overall function return
        }
    }

    if (return_code) {
        fprintf(stderr, "Error: Failed to process entry '%s', aborting operation.\n", full_source_path);
        log_error("Failed to process entry '%s', aborting operation.", full_source_path);
    }

    return return_code;
}

/**
 * anigei ena katalogo eite ston ketnriko ipologisti eite entos tou lkl
 * path: i diadromi pros ton katalogo
 * return DIR se periptosi epitichias, null se periptosi lathous
 */
static DIR *open_directory(const char *path) {
    DIR *directory_handle;
    int error_code;

    if (is_copy_to_filesystem) {
        directory_handle = opendir(path);
    } else {
        directory_handle = (DIR *)lkl_opendir(path, &error_code);
    }

    if (!directory_handle) {
        fprintf(stderr, "Error: Unable to open directory '%s': %s\n",
                path, is_copy_to_filesystem ? strerror(errno) : lkl_strerror(error_code));
        log_error("Unable to open directory '%s': %s",
                  path, is_copy_to_filesystem ? strerror(errno) : lkl_strerror(error_code));
    }
    return directory_handle;
}

/**
 * anigei ena fakelo(directory) eite ston host eite ston lkl
 * path: i diadromi pros ton fakelo
 * return DIR se periptosi epitichias, null se periptosi sfalmatos
 */
static const char *read_directory_entry(DIR *dir_handle, const char *path) {
    struct lkl_dir *lkl_dir_handle = (struct lkl_dir *)dir_handle; // Cast for LKL specific functions
    const char *entry_name = NULL;
    const char *error_message = NULL;

    if (is_copy_to_filesystem) {
        struct dirent *directory_entry = readdir(dir_handle);
        if (directory_entry) {
            entry_name = directory_entry->d_name;
        }
    } else {
        struct lkl_linux_dirent64 *lkl_directory_entry = lkl_readdir(lkl_dir_handle);
        if (lkl_directory_entry) {
            entry_name = lkl_directory_entry->d_name;
        }
    }
    if (!entry_name) {
        if (is_copy_to_filesystem) {
            if (errno) {
                error_message = strerror(errno);
            }
        } else {
            if (lkl_errdir(lkl_dir_handle)) {
                error_message = lkl_strerror(lkl_errdir(lkl_dir_handle));
            }
        }
    }

    if (error_message) {
        fprintf(stderr, "Error while reading directory '%s': %s\n", path, error_message);
        log_error("Error while reading directory '%s': %s", path, error_message);
    }
    return entry_name;
}

/**
 * klini to directory handler
 * dir_handle: gia na klisi to directory
 */
static void close_directory_handle(DIR *dir_handle) {
    if (is_copy_to_filesystem) {
        closedir(dir_handle);
    } else {
        lkl_closedir((struct lkl_dir *)dir_handle);
    }
}

/**
 * anazite anadromika ena directory kai antigrafei ta stoichia pou antistichoun metaxei tous
 * source_dir_path: i diadromi apo ton katalogo proelefsis
 * destination_dir_path: i diadromi pros to teliko katalogo
 * filename_match_pattern: motivo gia tin antistichisi ton stichion metaxei tous
 * owner_id: to UID gia to neo archio
 * group_id: to GID gia to neo archio
 * return 0 se periptosi epitichias
 */
static int search_directory_for_files(const char *source_dir_path, const char *destination_dir_path,
                                      const char *filename_match_pattern, uid_t owner_id, gid_t group_id) {
    DIR *directory_handle;
    const char *entry_name;
    int return_code = 0;

    directory_handle = open_directory(source_dir_path);
    if (!directory_handle) {
        return -1; // Error already logged by open_directory
    }
    while ((entry_name = read_directory_entry(directory_handle, source_dir_path))) {
        if (!strcmp(entry_name, ".") || !strcmp(entry_name, "..")) {
            continue;
        }
        if (filename_match_pattern && fnmatch(filename_match_pattern, entry_name, 0) != 0) {
            continue;
        }
        return_code = process_file_system_entry(source_dir_path, destination_dir_path, entry_name, owner_id, group_id);
        if (return_code) {
            goto cleanup_dir; // Abort on first error
        }
    }

cleanup_dir:
    close_directory_handle(directory_handle);

    return return_code;
}

/**
 * elechi an mia diadromi anaferete ston root directory(p.x, ".", "..", "/")
 * afto chrisimopoiite gia na diefkrinisoume an theloume na antigrapsoume ena olokliro directory i ena sigkekrimeno fakelo
 * path: i diadromi tis simvolosiras pou tha elechtei
 * return 1 ena anaferomaste se root alios retun 0
 */
static int is_root_path(const char *path) {
    const char *current_char = path;

    while (*current_char) {
        switch (*current_char) {
            case '.':
                if (current_char > path && current_char[-1] == '.') {
                    return 0; // Not a root path (e.g., "../something")
                }
                break;
            case '/':
                break;
            default:
                return 0; // Contains other characters, not a simple root path
        }
        current_char++;
    }
    return 1; // Only contains '.', '/', or empty string (treated as current dir)
}

int copy_single_item(const char *source_item_path, const char *mount_point,
                     const char *destination_path, uid_t owner_id, gid_t group_id) {
    char source_full_path[PATH_MAX];
    char destination_full_path[PATH_MAX];
    char *source_directory_component, *source_base_name_component;

    if (is_copy_to_filesystem) { // Source is on host, destination is in LKL
        snprintf(source_full_path, sizeof(source_full_path), "%s", source_item_path);
        snprintf(destination_full_path, sizeof(destination_full_path), "%s/%s", mount_point, destination_path);
    } else { // Source is in LKL, destination is on host
        snprintf(source_full_path, sizeof(source_full_path), "%s/%s", mount_point, source_item_path);
        snprintf(destination_full_path, sizeof(destination_full_path), "%s", destination_path);
    }

    if (is_root_path(source_item_path)) {
        log_info("Copying all contents from source directory '%s' to destination '%s'.",
                 source_full_path, destination_full_path);
        return search_directory_for_files(source_full_path, destination_full_path, NULL, owner_id, group_id);
    } else {
        source_directory_component = dirname(strdup(source_full_path));
        source_base_name_component = basename(strdup(source_full_path));

        log_info("Copying single item '%s' from directory '%s' to destination '%s'.",
                 source_base_name_component, source_directory_component, destination_full_path);

        int result = search_directory_for_files(source_directory_component, destination_full_path,
                                                source_base_name_component, owner_id, group_id);
        free(source_directory_component); // Free memory allocated by strdup
        free(source_base_name_component);

        return result;
    }
}

/**
 * Main gia ta cptofs/cpfromfs 
 * chirizete tin analisi orismaton, tin archikopoihsh lkl, tis leitourgies tou diskou kai tin antigrafei archion
 * @return 0 ena einai epitichis
 */
int main(int argc, char **argv) {
    log_init(); 
    log_info("Program started.");

    struct lkl_disk lkl_disk_object;
    long return_code, unmount_return_code;
    int path_index;
    char mount_point_buffer[32];
    unsigned int assigned_disk_id;
    parsed_command_line_arguments.owner_uid = (uid_t)-1;
    parsed_command_line_arguments.group_gid = (gid_t)-1;
 
    if (strstr(argv[0], "cptofs")) {
        is_copy_to_filesystem = 1;
        return_code = argp_parse(&argp_cptofs_config, argc, argv, 0, 0, &parsed_command_line_arguments);
    } else {
        is_copy_to_filesystem = 0;
        return_code = argp_parse(&argp_cpfromfs_config, argc, argv, 0, 0, &parsed_command_line_arguments);
    }

    if (return_code < 0) {
        log_error("Argument parsing failed.");
        return -1;
    }

    log_info("Arguments parsed successfully.");
    log_info("Filesystem type: %s", parsed_command_line_arguments.filesystem_type);
    log_info("Filesystem image path: %s", parsed_command_line_arguments.filesystem_image_path);
    if (!parsed_command_line_arguments.enable_printk) {
        lkl_host_ops.print = NULL;
    }
    lkl_disk_object.fd = open(parsed_command_line_arguments.filesystem_image_path,
                              is_copy_to_filesystem ? O_RDWR : O_RDONLY);
    if (lkl_disk_object.fd < 0) {
        fprintf(stderr, "Error: Can't open filesystem image '%s': %s\n",
                parsed_command_line_arguments.filesystem_image_path, strerror(errno));
        log_error("Can't open filesystem image '%s': %s",
                  parsed_command_line_arguments.filesystem_image_path, strerror(errno));
        return_code = 1;
        goto cleanup_exit;
    }

    lkl_disk_object.ops = NULL;

    return_code = lkl_init(&lkl_host_ops);
    if (return_code < 0) {
        fprintf(stderr, "Error: LKL initialization failed: %s\n", lkl_strerror(return_code));
        log_error("LKL initialization failed: %s", lkl_strerror(return_code));
        goto cleanup_lkl_context;
    }
    return_code = lkl_disk_add(&lkl_disk_object);
    if (return_code < 0) {
        fprintf(stderr, "Error: Can't add disk to LKL: %s\n", lkl_strerror(return_code));
        log_error("Can't add disk to LKL: %s", lkl_strerror(return_code));
        goto cleanup_lkl_context;
    }
    assigned_disk_id = return_code; 
    return_code = lkl_start_kernel("mem=100M"); 
    if (return_code < 0) {
        fprintf(stderr, "Error: Failed to start LKL kernel: %s\n", lkl_strerror(return_code));
        log_error("Failed to start LKL kernel: %s", lkl_strerror(return_code));
        goto cleanup_lkl_halt;
    }
    return_code = lkl_mount_dev(assigned_disk_id, parsed_command_line_arguments.partition_number,
                                parsed_command_line_arguments.filesystem_type,
                                is_copy_to_filesystem ? 0 : LKL_MS_RDONLY, // Mount read-only if cpfromfs
                                NULL, mount_point_buffer, sizeof(mount_point_buffer));
    if (return_code) {
        fprintf(stderr, "Error: Can't mount disk: %s\n", lkl_strerror(return_code));
        log_error("Can't mount disk: %s", lkl_strerror(return_code));
        goto cleanup_lkl_halt;
    }
    lkl_sys_umask(0);
    for (path_index = 0; path_index < parsed_command_line_arguments.num_paths - 1; path_index++) {
        return_code = copy_single_item(parsed_command_line_arguments.file_paths[path_index],
                                       mount_point_buffer,
                                       parsed_command_line_arguments.file_paths[parsed_command_line_arguments.num_paths - 1],
                                       parsed_command_line_arguments.owner_uid,
                                       parsed_command_line_arguments.group_gid);
        if (return_code) {
            log_error("Error occurred during file copying.");
            break; // Stop on first copy error
        }
    }
    char device_string[13]; 
    snprintf(device_string, sizeof(device_string), "/dev/%08x", assigned_disk_id);
    for (;;) {
        return_code = lkl_sys_mount(device_string, mount_point_buffer, (char *)parsed_command_line_arguments.filesystem_type,
                                    LKL_MS_RDONLY | LKL_MS_REMOUNT, NULL);
        if (return_code == 0) { 
            break;
        }
        if (return_code == -LKL_EBUSY) { // Device is busy, wait and retry
            struct lkl_timespec sleep_time = {
                .tv_sec = 1,
                .tv_nsec = 0,
            };
            lkl_sys_nanosleep((struct __lkl__kernel_timespec *)&sleep_time, NULL);
            continue;
        } else if (return_code < 0) { // Other error during remount
            fprintf(stderr, "Error: Cannot remount disk read-only: %s\n", lkl_strerror(return_code));
            log_error("Cannot remount disk read-only: %s", lkl_strerror(return_code));
            break; // Exit loop on unrecoverable error
        }
    }
    unmount_return_code = lkl_umount_dev(assigned_disk_id, parsed_command_line_arguments.partition_number, 0, 1000);
    if (return_code == 0) {
        return_code = unmount_return_code;
        if (return_code) {
             fprintf(stderr, "Warning: Unmount failed with code: %ld (%s)\n", unmount_return_code, lkl_strerror(unmount_return_code));
             log_warn("Unmount failed with code: %ld (%s)", unmount_return_code, lkl_strerror(unmount_return_code));
        }
    }

cleanup_lkl_halt:
    lkl_sys_halt(); 
    log_info("Program completed.");

cleanup_lkl_context:
    if (lkl_disk_object.fd >= 0) {
        close(lkl_disk_object.fd); 
    }

cleanup_exit:
    if (g_application_log_file) {
        fclose(g_application_log_file);
    }
    return return_code;
}