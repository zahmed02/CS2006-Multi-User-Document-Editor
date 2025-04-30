#include "shared.h"

// Global variables for synchronization
sem_t *access_sem = NULL;
sem_t *owner_sem = NULL;
LockInfo *lock_info = NULL;
int lock_info_shm_id = -1;

// Signal handler for priority override
void handle_priority_signal(int signum) {
    if (signum == PRIORITY_SIGNAL) {
        printf("Received priority signal. Owner needs access.\n");
        // We don't need to do anything here - the checking for owner_waiting
        // is done in the lock acquisition and holding code
    }
}


void append_to_history() {
    FILE *history_file;
    FILE *doc_file;
    time_t current_time;
    struct tm *time_info;
    char timestamp[30];
    char buffer[1024];  // Buffer to read file content in chunks

    // Get current time
    time(&current_time);
    time_info = localtime(&current_time);

    // Format timestamp
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", time_info);

    // Open history file in append mode
    history_file = fopen("history.txt", "a");
    if (history_file == NULL) {
        fprintf(stderr, "Error: Could not open history.txt for appending.\n");
        return;
    }

    // Write start tag with timestamp
    fprintf(history_file, "<start timestamp=\"%s\">\n", timestamp);

    // Open the actual document file to read its contents
    if (SHARED_DOC != NULL) {
        doc_file = fopen(SHARED_DOC, "r");
        if (doc_file != NULL) {
            // Read from the document file and write into history.txt
            while (fgets(buffer, sizeof(buffer), doc_file) != NULL) {
                fputs(buffer, history_file);
            }
            fclose(doc_file);
        } else {
            fprintf(history_file, "[Error: Could not open document file %s]\n", SHARED_DOC);
        }
    } else {
        fprintf(history_file, "[Empty document]\n");
    }

    // Write end tag
    fprintf(history_file, "</end>\n\n");

    // Close the history file
    fclose(history_file);

    printf("Document successfully appended to history.txt\n");
}
void pop_last_snapshot() {
    FILE *history_file;
    FILE *temp_file;
    FILE *doc_file;
    long last_start_pos = -1;
    long copy_start_pos = -1;
    char line[1024];
    int copying = 0;

    // Open history.txt for reading
    history_file = fopen("history.txt", "r");
    if (history_file == NULL) {
        fprintf(stderr, "Error: Could not open history.txt for reading.\n");
        return;
    }

    // Find the last <start> position
    while (fgets(line, sizeof(line), history_file)) {
        if (strncmp(line, "<start", 6) == 0) {
            last_start_pos = ftell(history_file) - strlen(line);  // Go back to beginning of this <start> line
        }
    }

    if (last_start_pos == -1) {
        fprintf(stderr, "Error: No <start> tag found in history.txt.\n");
        fclose(history_file);
        return;
    }

    // Open document file to write restored content
    if (SHARED_DOC == NULL) {
        fprintf(stderr, "Error: SHARED_DOC filename is NULL.\n");
        fclose(history_file);
        return;
    }

    doc_file = fopen(SHARED_DOC, "w");
    if (doc_file == NULL) {
        fprintf(stderr, "Error: Could not open %s for writing.\n", SHARED_DOC);
        fclose(history_file);
        return;
    }

    // Rewind and start reading again to restore
    rewind(history_file);
    fseek(history_file, last_start_pos, SEEK_SET);

    // Start copying content into document (stop at </end>)
    while (fgets(line, sizeof(line), history_file)) {
        if (strncmp(line, "<start", 6) == 0) {
            copying = 1; // Start copying after <start> tag
            continue;    // Skip writing <start> line
        }
        if (strncmp(line, "</end>", 6) == 0) {
            break;       // Stop copying before writing </end>
        }
        if (copying) {
            fputs(line, doc_file);
        }
    }

    fclose(doc_file);
    fclose(history_file);

    // Now, remove the popped snapshot from history.txt
    // Open history.txt again
    history_file = fopen("history.txt", "r");
    if (history_file == NULL) {
        fprintf(stderr, "Error: Could not re-open history.txt for removal.\n");
        return;
    }

    temp_file = fopen("temp.txt", "w");
    if (temp_file == NULL) {
        fprintf(stderr, "Error: Could not open temp.txt for writing.\n");
        fclose(history_file);
        return;
    }

    int skip = 0;
    rewind(history_file);
    while (fgets(line, sizeof(line), history_file)) {
        if (strncmp(line, "<start", 6) == 0) {
            if (ftell(history_file) - strlen(line) - 1 == last_start_pos) {
                skip = 1;  // Start skipping lines
            }
        }
        if (!skip) {
            fputs(line, temp_file);
        }
        if (skip && strncmp(line, "</end>", 6) == 0) {
            skip = 0;  // Done skipping after </end>
        }
    }

    fclose(history_file);
    fclose(temp_file);

    // Replace history.txt with temp.txt
    remove("history.txt");
    rename("temp.txt", "history.txt");

    printf("Snapshot popped and restored into %s\n", SHARED_DOC);
}
void print_history() {
    FILE *history_file;
    char line[1024];

    history_file = fopen("history.txt", "r");
    if (history_file == NULL) {
        printf("No history found.\n");
        return;
    }

    printf("----- Document History -----\n");

    while (fgets(line, sizeof(line), history_file)) {
        printf("%s", line); // Simply print line by line
    }

    printf("----- End of History -----\n");

    fclose(history_file);
}


void initialize_synchronization(bool is_owner) {
    // Set up signal handler for priority override
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_priority_signal;
    sigaction(PRIORITY_SIGNAL, &sa, NULL);
    
    if (is_owner) {
        // Create or open semaphores
        access_sem = sem_open(ACCESS_SEMAPHORE, O_CREAT, 0644, 1);
        if (access_sem == SEM_FAILED) {
            perror("Failed to create access semaphore");
            exit(EXIT_FAILURE);
        }
        
        owner_sem = sem_open(OWNER_SEMAPHORE, O_CREAT, 0644, 1);
        if (owner_sem == SEM_FAILED) {
            perror("Failed to create owner semaphore");
            sem_close(access_sem);
            exit(EXIT_FAILURE);
        }
        
        // Set up shared memory for lock info
        
        key_t key = ftok("/tmp", 'R');
        if (key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
        }
        lock_info_shm_id = shmget(key, sizeof(LockInfo), IPC_CREAT | 0666);
        if (lock_info_shm_id < 0) {
            perror("Failed to create lock info shared memory");
            sem_close(access_sem);
            sem_close(owner_sem);
            exit(EXIT_FAILURE);
        }
        
        // Attach to shared memory
        lock_info = (LockInfo*) shmat(lock_info_shm_id, NULL, 0);
        if (lock_info == (LockInfo*) -1) {
            perror("Failed to attach to lock info shared memory");
            sem_close(access_sem);
            sem_close(owner_sem);
            exit(EXIT_FAILURE);
        }
        
        // Initialize lock info
        lock_info->holding_pid = 0;
        lock_info->lock_type = 0;
        lock_info->owner_waiting = false;
        lock_info->reader_count = 0;
        lock_info->countdown_active = false;
        lock_info->countdown_value = 0;
        lock_info->forced_lock = false;
        lock_info->editor_pid = 0;
        lock_info->edit_start_time = 0;
        lock_info->time_allocation = 0;
        lock_info->time_limit_active = false;
        
        printf("Synchronization mechanisms initialized by owner.\n");
    } else {
        // Open existing semaphores (should be created by admin program)
        access_sem = sem_open(ACCESS_SEMAPHORE, 0);
        if (access_sem == SEM_FAILED) {
            perror("Failed to open access semaphore - make sure admin is running first");
            exit(EXIT_FAILURE);
        }
        
        owner_sem = sem_open(OWNER_SEMAPHORE, 0);
        if (owner_sem == SEM_FAILED) {
            perror("Failed to open owner semaphore");
            sem_close(access_sem);
            exit(EXIT_FAILURE);
        }
        
        // Get existing shared memory for lock info
        key_t key = ftok("/tmp", 'R');
        if (key == -1) {
            perror("ftok");
            exit(EXIT_FAILURE);
        }
        lock_info_shm_id = shmget(key, sizeof(LockInfo), IPC_CREAT | 0666);
        if (lock_info_shm_id < 0) {
            perror("Failed to get lock info shared memory segment");
            sem_close(access_sem);
            sem_close(owner_sem);
            exit(EXIT_FAILURE);
        }
        
        // Attach to shared memory
        lock_info = (LockInfo*) shmat(lock_info_shm_id, NULL, 0);
        if (lock_info == (LockInfo*) -1) {
            perror("Failed to attach to lock info shared memory");
            sem_close(access_sem);
            sem_close(owner_sem);
            exit(EXIT_FAILURE);
        }
        
        printf("Synchronization mechanisms initialized by user.\n");
    }
}

void cleanup_synchronization(bool is_owner) {
    // Detach from shared memory
    if (lock_info != NULL) {
        shmdt(lock_info);
    }
    
    if (is_owner) {
        // Remove shared memory
        shmctl(lock_info_shm_id, IPC_RMID, NULL);
        
        // Close and unlink semaphores
        sem_close(access_sem);
        sem_unlink(ACCESS_SEMAPHORE);
        
        sem_close(owner_sem);
        sem_unlink(OWNER_SEMAPHORE);
        
        printf("Synchronization resources cleaned up by owner.\n");
    } else {
        // Close semaphores (but don't unlink - admin manages them)
        sem_close(access_sem);
        sem_close(owner_sem);
        
        printf("Synchronization resources cleaned up by user.\n");
    }
}

void wait_for_owner_priority(User *user) {
    // Owner never waits - skip if owner
    if (user->priority == PRIORITY_OWNER) {
        return;
    }
    
    // Wait on owner semaphore
    sem_wait(owner_sem);
    
    // Release semaphore for next user
    sem_post(owner_sem);
}

void signal_owner_priority(void) {
    // Set the owner_waiting flag to true
    lock_info->owner_waiting = true;
    
    // If someone holds the lock, send them a signal
    if (lock_info->holding_pid > 0 && lock_info->holding_pid != getpid()) {
        printf("Owner signaling process %d to release lock\n", lock_info->holding_pid);
        kill(lock_info->holding_pid, PRIORITY_SIGNAL);
    }
    
    // Release all semaphores to unblock any waiting users
    sem_post(owner_sem);
    sem_post(access_sem);
}
bool acquire_read_lock(int fd, User *user) {
    struct flock lock;
    
    // If owner, gain immediate access
    if (user->priority == PRIORITY_OWNER) {
        // First check if there's an existing lock
        lock.l_type = F_WRLCK;  // Check for any conflicting locks
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 0;
        
        // Check if there's a conflicting lock without blocking
        if (fcntl(fd, F_GETLK, &lock) != -1 && lock.l_type != F_UNLCK) {
            // Someone holds a lock - send them a signal first
            printf("OWNER detected lock held by PID %d, sending priority signal...\n", 
                   lock.l_pid);
            
            // Store the PID to send signal to
            pid_t holder_pid = lock.l_pid;
            
            // Send signal to the lock holder
            if (holder_pid > 0) {
                handle_priority_signal(holder_pid);
                // Wait a bit for the signal to be processed
                usleep(100000);  // 100ms
            }
        }
        
        // Now try to acquire the read lock
        lock.l_type = F_RDLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 0;
        
        printf("OWNER attempting to acquire read lock...\n");
        
        // Use non-blocking attempt first
        if (fcntl(fd, F_SETLK, &lock) == -1) {
            if (errno == EACCES || errno == EAGAIN) {
                // Lock still held, try one more time with blocking
                printf("OWNER waiting for lock release...\n");
                
                // Set a timeout using alarm
                alarm(5);  // 5 second timeout
                
                if (fcntl(fd, F_SETLKW, &lock) == -1) {
                    if (errno != EINTR) {
                        perror("Error acquiring owner read lock");
                        alarm(0);  // Cancel alarm
                        return false;
                    }
                    printf("OWNER lock acquisition timed out\n");
                    alarm(0);  // Cancel alarm
                    return false;
                }
                
                alarm(0);  // Cancel alarm if successful
            } else {
                perror("Error acquiring owner read lock");
                return false;
            }
        }
        
        // Update lock info
        lock_info->holding_pid = getpid();
        lock_info->lock_type = 1; // read lock
        lock_info->owner_waiting = false;
        
        printf("OWNER read lock acquired successfully.\n");
        return true;
    }
    
    // For non-owner users, follow priority protocol
    wait_for_owner_priority(user);
    
    // Check if owner is waiting before proceeding
    if (lock_info->owner_waiting) {
        printf("Owner is waiting, user %s cannot acquire read lock.\n", user->name);
        return false;
    }
    
    // Acquire access semaphore
    sem_wait(access_sem);
    
    // Check again if owner is waiting
    if (lock_info->owner_waiting) {
        printf("Owner became waiting, user %s cannot acquire read lock.\n", user->name);
        sem_post(access_sem);
        return false;
    }
    
    // Increment reader count
    lock_info->reader_count++;
    
    // If this is the first reader, acquire write lock on file
    // to prevent any writers from accessing
    if (lock_info->reader_count == 1) {
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 0;
        
        if (fcntl(fd, F_SETLKW, &lock) == -1) {
            perror("Error acquiring initial write lock for readers");
            lock_info->reader_count--;
            sem_post(access_sem);
            return false;
        }
        
        // Update lock info
        lock_info->holding_pid = getpid();
        lock_info->lock_type = 1; // read lock
    }
    
    // Release access semaphore
    sem_post(access_sem);
    
    // Set up read lock for this specific reader
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    
    printf("User '%s' (priority %d) acquired read lock.\n", 
           user->name, user->priority);
           
    return true;
}

bool acquire_write_lock(int fd, User *user) {
    struct flock lock;
    
    // If owner, gain immediate access
    if (user->priority == PRIORITY_OWNER) {
        signal_owner_priority();  // Signal to give owner priority
        
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 0; // Lock the entire file
        
        printf("OWNER attempting to acquire write lock...\n");
        
        // Wait for lock to be available
        while (fcntl(fd, F_SETLKW, &lock) == -1) {
            if (errno != EINTR) {
                perror("Error acquiring owner write lock");
                return false;
            }
            // If interrupted by signal, retry
            printf("OWNER write lock acquisition interrupted, retrying...\n");
            lock.l_type = F_WRLCK;
        }
        
        // Update lock info
        lock_info->holding_pid = getpid();
        lock_info->lock_type = 2; // write lock
        lock_info->owner_waiting = false;
        
        printf("OWNER write lock acquired successfully.\n");
        return true;
    }
    
    // For non-owner users, follow priority protocol
    wait_for_owner_priority(user);
    
    // Check if owner is waiting before proceeding
    if (lock_info->owner_waiting) {
        printf("Owner is waiting, user %s cannot acquire write lock.\n", user->name);
        return false;
    }
    
    // Acquire access semaphore (exclusive access for writers)
    sem_wait(access_sem);
    
    // Check again if owner is waiting
    if (lock_info->owner_waiting) {
        printf("Owner became waiting, user %s cannot acquire write lock.\n", user->name);
        sem_post(access_sem);
        return false;
    }
    
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    
    printf("User '%s' (priority %d) attempting to acquire write lock...\n", 
           user->name, user->priority);
    
    // Try to acquire the lock but be responsive to signals
    while (fcntl(fd, F_SETLKW, &lock) == -1) {
        if (errno != EINTR) {
            perror("Error acquiring write lock");
            sem_post(access_sem);
            return false;
        }
        
        // If interrupted by signal, check if owner is waiting
        if (lock_info->owner_waiting) {
            printf("Owner is now waiting, write lock acquisition aborted.\n");
            sem_post(access_sem);
            return false;
        }
        
        // Try again
        lock.l_type = F_WRLCK;
    }
    
    // Update lock info
    lock_info->holding_pid = getpid();
    lock_info->lock_type = 2; // write lock
    
    printf("User '%s' (priority %d) acquired write lock.\n", 
           user->name, user->priority);
    
    // Note: We keep the access_sem held during the entire write operation
    // It will be released when releasing the lock
    
    return true;
}

void release_read_lock(int fd, User *user) {
    struct flock lock;
    
    // If owner, simply release the lock
    if (user->priority == PRIORITY_OWNER) {
        lock.l_type = F_UNLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 0;
        
        if (fcntl(fd, F_SETLK, &lock) == -1) {
            perror("Error releasing owner read lock");
        } else {
            // Update lock info
            if (lock_info->holding_pid == getpid()) {
                lock_info->holding_pid = 0;
                lock_info->lock_type = 0;
            }
            printf("OWNER read lock released.\n");
        }
        
        return;
    }
    
    // For regular users, decrement reader count
    sem_wait(access_sem);
    
    lock_info->reader_count--;
    
    // If this is the last reader, release the write lock
    if (lock_info->reader_count == 0) {
        lock.l_type = F_UNLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 0;
        
        if (fcntl(fd, F_SETLK, &lock) == -1) {
            perror("Error releasing last reader lock");
        } else {
            // Update lock info
            if (lock_info->holding_pid == getpid()) {
                lock_info->holding_pid = 0;
                lock_info->lock_type = 0;
            }
            printf("Last reader lock released.\n");
        }
    }
    
    sem_post(access_sem);
    
    printf("User '%s' released read lock.\n", user->name);
}

void release_write_lock(int fd, User *user) {
    struct flock lock;
    
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    
    if (fcntl(fd, F_SETLK, &lock) == -1) {
        perror("Error releasing write lock");
        return;
    }
    
    // Update lock info
    if (lock_info->holding_pid == getpid()) {
        lock_info->holding_pid = 0;
        lock_info->lock_type = 0;
    }
    
    printf("User '%s' released write lock.\n", user->name);
    
    // Release access semaphore for non-owner users
    if (user->priority != PRIORITY_OWNER) {
        sem_post(access_sem);
    }
}
