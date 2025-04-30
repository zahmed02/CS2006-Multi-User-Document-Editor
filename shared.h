// File: shared_locks.h
// This will be our shared header file for both owner.c and user.c

#ifndef SHARED_LOCKS_H
#define SHARED_LOCKS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_LINE 256
#define MAX_USERS 20
#define CONTROL_FILE "shared_doc_control.txt"
#define SHARED_DOC "shared_docs.txt"
#define ACCESS_SEMAPHORE "/doc_access_sem"
#define OWNER_SEMAPHORE "/owner_priority_sem"
#define LOCK_INFO_SHM_KEY 9876
#define READER_COUNT_SHM_KEY 9877

// User access types
#define ACCESS_READ_ONLY 1
#define ACCESS_WRITE_ONLY 2
#define ACCESS_BOTH 3

// User priority levels
#define PRIORITY_OWNER -1  // Special priority for owner/admin
#define PRIORITY_HIGH 0
#define PRIORITY_LOW 1

// Signal for priority override
#define PRIORITY_SIGNAL SIGUSR1

void append_to_history();
void pop_last_snapshot();
void print_history();

typedef struct {
    pid_t holding_pid;     // PID of process holding the lock
    int lock_type;         // 0=none, 1=shared/read, 2=exclusive/write
    bool owner_waiting;    // Flag indicating if owner is waiting for access
    int reader_count;      // Number of concurrent readers
    bool countdown_active; // Indicates if countdown is in progress
    int countdown_value;   // Current countdown value (5 to 0)
    bool forced_lock;      // Owner is forcing lock takeover
    pid_t editor_pid;      // PID of the editor process currently editing
    time_t edit_start_time; // When the current editing session started
    int time_allocation;   // Time allocation in seconds for current editor
    bool time_limit_active; // Whether time limiting is active
} LockInfo;

typedef struct {
    char name[50];
    int priority;  // -1 for owner, 0 for high, 1 for low
    int access_type;  // 1: read-only, 2: write-only, 3: both
    pid_t pid;
    bool is_owner;
} User;



// Global variables for synchronization
extern sem_t *access_sem;
extern sem_t *owner_sem;
extern LockInfo *lock_info;
extern int lock_info_shm_id;

// Function prototypes
void initialize_synchronization(bool is_owner);
void cleanup_synchronization(bool is_owner);
bool acquire_read_lock(int fd, User *user);
bool acquire_write_lock(int fd, User *user);
void release_read_lock(int fd, User *user);
void release_write_lock(int fd, User *user);
void wait_for_owner_priority(User *user);
void signal_owner_priority(void);
void handle_priority_signal(int signum);


#endif // SHARED_LOCKS_H