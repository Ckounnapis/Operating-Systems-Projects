#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <lkl.h>
#include <lkl_host.h>
#if !defined(__MSYS__) && !defined(__MINGW32__)
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#else
#include <windows.h>
#endif
#include "test.h"
#include "cla.h" 

#define LOG_FILE "/tmp/lkl_test.log" // Define the path for the log file

//gennikos perigrafeas archiou gia to log file archikopoiontas to se mi egkiri timi
int g_log_file_descriptor = -1;

/**
 * katagrafei ena minima sto logfile.
 * message: to string message tha graftei sto log file
 */
void log_to_file(const char *message) {
    // mono eggrafei ena to log file anixe epitichos
    if (g_log_file_descriptor != -1) {
        write(g_log_file_descriptor, message, strlen(message));
        write(g_log_file_descriptor, "\n", 1); // prosthese nea grammi
    }
}

// domi gia tin diatirisi parsed command-line arguments eidika gai afti tin dokimi
static struct {
    int enable_printk_output; // Flag to enable Linux printk messages
    const char *disk_image_path; // Path to the disk image file
    const char *filesystem_type; // Type of the filesystem (e.g., "ext4", "xfs")
    int partition_number;        // Partition number to mount
} command_line_arguments;

//pinakas orismaton
struct cl_arg args[] = {
    {"disk", 'd', "disk file to use", 1, CL_ARG_STR, &command_line_arguments.disk_image_path},
    {"partition", 'P', "partition to mount", 1, CL_ARG_INT, &command_line_arguments.partition_number},
    {"type", 't', "filesystem type", 1, CL_ARG_STR, &command_line_arguments.filesystem_type},
    {0}, // Sentinel to mark the end of the arguments array
};

// katholiki domi diskou lkl
static struct lkl_disk lkl_virtual_disk;
static int lkl_disk_id = -1;

/**
 * prostheti to disk image sto pirina tou lkl
 * return TEST_SUCCESS se periptosi epitichias, TEST_FAILURE se periptosi sfalmatos
 */
int lkl_test_disk_add(void) {
    char log_message_buffer[256];
    snprintf(log_message_buffer, sizeof(log_message_buffer), "Attempting to add disk: %s", command_line_arguments.disk_image_path);
    log_to_file(log_message_buffer);

    // anixe to disk image me vasi to logismiko
#if defined(__MSYS__) || defined(__MINGW32__)
    lkl_virtual_disk.handle = CreateFile(command_line_arguments.disk_image_path, GENERIC_READ | GENERIC_WRITE,
                                        0, NULL, OPEN_EXISTING, 0, NULL);
    if (!lkl_virtual_disk.handle) {
#else
    lkl_virtual_disk.fd = open(command_line_arguments.disk_image_path, O_RDWR);
    if (lkl_virtual_disk.fd < 0) {
#endif
        // an apotichi kane unlink
        goto out_unlink_file;
    }

    lkl_virtual_disk.ops = NULL; 

    // vale to disk sto lkl kernel
    lkl_disk_id = lkl_disk_add(&lkl_virtual_disk);
    if (lkl_disk_id < 0) {
        snprintf(log_message_buffer, sizeof(log_message_buffer), "Failed to add disk to LKL: %s", lkl_strerror(lkl_disk_id));
        log_to_file(log_message_buffer);
        goto out_close_handle; // If adding fails, close the file handle
    }

    snprintf(log_message_buffer, sizeof(log_message_buffer), "Disk added successfully with ID: %d", lkl_disk_id);
    log_to_file(log_message_buffer);
    goto out; // Disk added, skip error cleanup

out_close_handle:
// klise to archio 
#if defined(__MSYS__) || defined(__MINGW32__)
    CloseHandle(lkl_virtual_disk.handle);
#else
    close(lkl_virtual_disk.fd);
#endif

out_unlink_file:
// unlink/ delete to archio 
#if defined(__MSYS__) || defined(__MINGW32__)
    DeleteFile(command_line_arguments.disk_image_path);
#else
    unlink(command_line_arguments.disk_image_path);
#endif

out:
    lkl_test_logf("Disk file descriptor/handle: %x, Disk ID: %d", lkl_virtual_disk.fd, lkl_disk_id);

    if (lkl_disk_id >= 0) {
        return TEST_SUCCESS;
    }

    log_to_file("Failed to add disk due to an earlier error.");
    return TEST_FAILURE;
}

/**
 * afairei to disk apo ton pirina tou lkl kai kleieni to file handle
 * return TEST_SUCCESS an einai epitichis, TEST_FAILURE an echi sfalma
 */
int lkl_test_disk_remove(void) {
    char log_message_buffer[256];
    snprintf(log_message_buffer, sizeof(log_message_buffer), "Attempting to remove disk with ID: %d", lkl_disk_id);
    log_to_file(log_message_buffer);

    // svise to disk apo to lkl kernel
    int return_code = lkl_disk_remove(lkl_virtual_disk);

// klise to disk image file handle
#if defined(__MSYS__) || defined(__MINGW32__)
    CloseHandle(lkl_virtual_disk.handle);
#else
    close(lkl_virtual_disk.fd);
#endif

    if (return_code == 0) {
        log_to_file("Disk removed successfully.");
        return TEST_SUCCESS;
    }

    snprintf(log_message_buffer, sizeof(log_message_buffer), "Failed to remove disk: %s", lkl_strerror(return_code));
    log_to_file(log_message_buffer);
    return TEST_FAILURE;
}

static char mount_point_buffer[32]; 
LKL_TEST_CALL(mount_device, lkl_mount_dev, 0, lkl_disk_id, command_line_arguments.partition_number,
              command_line_arguments.filesystem_type, 0, NULL, mount_point_buffer, sizeof(mount_point_buffer))

/**
 * aposindei tin porigoumeni siskevi apo ton pirina tou lkl
 * return TEST_SUCCESS an einai epitichis, TEST_FAILURE an echi sfalma
 */
static int lkl_test_umount_dev(void) {
    char log_message_buffer[256]; // Declare buffer here
    log_to_file("Attempting to unmount device...");

    // allazei to directory to opio vriskese mesa sto lkl se root gia na pofigi "device busy" errors
    long chdir_result = lkl_sys_chdir("/");
    // Unmount the device
    long umount_result = lkl_umount_dev(lkl_disk_id, command_line_arguments.partition_number, 0, 1000);

    if (chdir_result == 0 && umount_result == 0) {
        log_to_file("Device unmounted successfully.");
        return TEST_SUCCESS;
    } else {
        snprintf(log_message_buffer, sizeof(log_message_buffer),
                 "Failed to unmount device. Chdir result: %ld, Umount result: %ld (%s)",
                 chdir_result, umount_result, lkl_strerror(umount_result));
        log_to_file(log_message_buffer);
        return TEST_FAILURE;
    }
}

struct lkl_dir *g_directory_handle;

/**
 * anigi to directory mesa sto LKL filesystem.
 * return TEST_SUCCESS an einai epitichis, TEST_FAILURE an parousisasti sfalma
 */
static int lkl_test_opendir(void) {
    int error_code;
    char log_message_buffer[256]; //dilonoume ena buffer
    snprintf(log_message_buffer, sizeof(log_message_buffer), "Opening directory: %s", mount_point_buffer);
    log_to_file(log_message_buffer);

    // anigei to directory chrisimopiontas tin entoli tou LKL's opendir
    g_directory_handle = lkl_opendir(mount_point_buffer, &error_code);
    lkl_test_logf("lkl_opendir(%s) = %d %s\n", mount_point_buffer, error_code,
                  lkl_strerror(error_code));

    if (error_code == 0) {
        log_to_file("Directory opened successfully.");
        return TEST_SUCCESS;
    }

    snprintf(log_message_buffer, sizeof(log_message_buffer), "Failed to open directory: %s", lkl_strerror(error_code));
    log_to_file(log_message_buffer);
    return TEST_FAILURE;
}

/**
 * diavazei ta entries apo to opened directory.
 * TEST_SUCCESS an einai epitichis, TEST_FAILURE an parousiasti sfalma
 */
static int lkl_test_readdir(void) {
    struct lkl_linux_dirent64 *directory_entry;
    int characters_written = 0;
    char log_message_buffer[256]; // dilonoume ena buffer

    log_to_file("Reading directory entries...");
    
    // kanei Loop anamesa sta directory entries mechri na epistrafei NULL
    while ((directory_entry = lkl_readdir(g_directory_handle))) {
        //  dilosi sto directory to entry name
        characters_written += lkl_test_logf("%s ", directory_entry->d_name);
        snprintf(log_message_buffer, sizeof(log_message_buffer), "Read directory entry: %s", directory_entry->d_name);
        log_to_file(log_message_buffer);

        //prosthetei nea grammi an afti pou einai gini poli megali
        if (characters_written >= 70) {
            lkl_test_logf("\n");
            characters_written = 0;
            // break gia na apofigi tin sichisi an ta entries einai poli megala
            break; 
        }
    }

    // elechos gai sfalmata mesa sto directory entries
    if (lkl_errdir(g_directory_handle) == 0) {
        log_to_file("Directory read successfully.");
        return TEST_SUCCESS;
    }

    snprintf(log_message_buffer, sizeof(log_message_buffer), "Failed to read directory: %s", lkl_strerror(lkl_errdir(g_directory_handle)));
    log_to_file(log_message_buffer);
    return TEST_FAILURE;
}

LKL_TEST_CALL(closedir_handle, lkl_closedir, 0, g_directory_handle);
LKL_TEST_CALL(change_to_mount_point, lkl_sys_chdir, 0, mount_point_buffer);
LKL_TEST_CALL(start_lkl_kernel, lkl_start_kernel, 0, "mem=128M loglevel=8");
LKL_TEST_CALL(stop_lkl_kernel, lkl_sys_halt, 0);

// pinakas ton tests pou tha ektelestoun apo to LKL test framework
struct lkl_test tests[] = {
    LKL_TEST(disk_add),      
    LKL_TEST(start_lkl_kernel),       
    LKL_TEST(mount_device),           
    LKL_TEST(change_to_mount_point),  
    LKL_TEST(opendir),       
    LKL_TEST(readdir),       
    LKL_TEST(closedir_handle),        
    LKL_TEST(umount_dev),    
    LKL_TEST(stop_lkl_kernel),        
    LKL_TEST(disk_remove),   
};
/**
 * sinartisi main tou disk lkl
 * return 0 se periptosi epitchias, return -1 se periptosi sfalmatos
 */
int main(int argc, const char **argv) {
    int test_run_result;
    // anigma tou log file gia eggrafi
    g_log_file_descriptor = open(LOG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (g_log_file_descriptor < 0) {
        perror("Error: Failed to open log file"); 
        return -1;
    }
    if (parse_args(argc, argv, args) < 0) {
        log_to_file("Error: Failed to parse command-line arguments.");
        close(g_log_file_descriptor);
        return -1;
    }
    if (!command_line_arguments.disk_image_path || !command_line_arguments.filesystem_type) {
        fprintf(stderr, "Error: 'disk' and 'type' parameters are required.\n");
        log_to_file("Error: Missing required 'disk' or 'type' command-line parameters.");
        close(g_log_file_descriptor);
        return -1;
    }
    // prosarmozoume to LKL host print function sto prosarmosmeno logger
    lkl_host_ops.print = lkl_test_log;
    //archikopoiisi tou LKL perivallon
    lkl_init(&lkl_host_ops);
    // trexe ta apetoumena tests
    test_run_result = lkl_test_run(tests, sizeof(tests) / sizeof(struct lkl_test),
                                   "disk %s", command_line_arguments.filesystem_type);
    // katharise to lkl perivallon
    lkl_cleanup();
    // klise to log file
    close(g_log_file_descriptor)
    return test_run_result;
}