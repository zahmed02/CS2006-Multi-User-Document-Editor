#include "shared.h"

// Add this at the top of your file with other global variables



// Function prototypes for local functions
bool find_user(const char *name, User *user);
void display_menu(User *user);
void view_document(User *user);
void edit_document(User *user);

// Global to track if we need to exit due to priority
volatile sig_atomic_t priority_exit_flag = 0;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <username>\n", argv[0]);
        return 1;
    }
    
    // Set up signal handler for priority override
    struct sigaction sa;
    sa.sa_handler = handle_priority_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(PRIORITY_SIGNAL, &sa, NULL) == -1) {
        perror("Failed to set up signal handler");
        return 1;
    }
    
    // Initialize synchronization mechanisms
    initialize_synchronization(false);  // false = not owner
    
    User current_user;
    
    // Look up user in control file
    if (!find_user(argv[1], &current_user)) {
        printf("User '%s' not found or doesn't have access.\n", argv[1]);
        cleanup_synchronization(false);
        return 1;
    }
    
    // Update PID in user record
    current_user.pid = getpid();
    
    const char *priority_str;
    if (current_user.priority == PRIORITY_OWNER)
        priority_str = "Owner (Highest)";
    else if (current_user.priority == PRIORITY_HIGH)
        priority_str = "High";
    else
        priority_str = "Low";
        
    const char *access_str;
    switch (current_user.access_type) {
        case ACCESS_READ_ONLY:
            access_str = "Read-only";
            break;
        case ACCESS_WRITE_ONLY:
            access_str = "Write-only";
            break;
        case ACCESS_BOTH:
            access_str = "Read-Write";
            break;
        default:
            access_str = "Unknown";
    }
    
    printf("Welcome, %s!\n", current_user.name);
    printf("Access type: %s\n", access_str);
    printf("Priority: %s\n", priority_str);
    
    int choice;
    while(1) {
        display_menu(&current_user);
        scanf("%d", &choice);
        getchar(); // Clear newline
        
        switch(choice) {
            case 1:
                if (current_user.access_type == ACCESS_READ_ONLY || current_user.access_type == ACCESS_BOTH) {
                    view_document(&current_user);
                } else {
                    printf("You don't have read access to this document.\n");
                }
                break;
            case 2:
                if (current_user.access_type == ACCESS_WRITE_ONLY || current_user.access_type == ACCESS_BOTH) {
                    edit_document(&current_user);
                } else {
                    printf("You don't have write access to this document.\n");
                }
                break;
            case 3:
                printf("Exiting program.\n");
                cleanup_synchronization(false);
                return 0;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }
    
    return 0;
}


bool find_user(const char *name, User *user) {
    FILE *file = fopen(CONTROL_FILE, "r");
    if (file == NULL) {
        perror("Error opening control file");
        exit(EXIT_FAILURE);
    }
    
    char line[MAX_LINE];
    char doc_path[MAX_LINE];
    
    // Read document path
    fgets(doc_path, MAX_LINE, file);
    doc_path[strcspn(doc_path, "\n")] = 0; // Remove newline
    
    // Read admin info
    User admin;
    fgets(line, MAX_LINE, file);
    sscanf(line, "%s %d %d %d", admin.name, &admin.priority, &admin.access_type, &admin.pid);
    admin.priority = PRIORITY_OWNER;  // Ensure owner priority
    
    // Check if user is admin
    if (strcmp(name, admin.name) == 0) {
        *user = admin;
        fclose(file);
        return true;
    }
    
    // Read number of additional users
    int user_count;
    fgets(line, MAX_LINE, file);
    sscanf(line, "%d", &user_count);
    
    // Look for the user
    bool found = false;
    for (int i = 0; i < user_count; i++) {
        if (fgets(line, MAX_LINE, file) != NULL) {
            User current;
            sscanf(line, "%s %d %d %d", current.name, &current.priority, &current.access_type, &current.pid);
            
            if (strcmp(name, current.name) == 0) {
                *user = current;
                found = true;
                break;
            }
        }
    }
    
    fclose(file);
    return found;
}

void display_menu(User *user) {
    printf("\n=== Document Access Menu ===\n");
    if (user->access_type == ACCESS_READ_ONLY || user->access_type == ACCESS_BOTH) {
        printf("1. View document\n");
    }
    if (user->access_type == ACCESS_WRITE_ONLY || user->access_type == ACCESS_BOTH) {
        printf("2. Edit document\n");
    }
    printf("3. Exit\n");
    printf("Enter your choice: ");
}

void view_document(User *user) {
        // First, check if the file exists
        if (access(SHARED_DOC, F_OK) == -1) {
            printf("Error: Shared document doesn't exist. Ask owner to create it.\n");
            return;
        }
        
    int fd = open(SHARED_DOC, O_RDWR);
    if (fd == -1) {
        perror("Error opening document for reading");
        return;
    }
    
    if (!acquire_read_lock(fd, user)) {
        close(fd);
        return;
    }
    
    printf("\n--- Document Content ---\n");
    char buffer[1024];
    ssize_t bytes_read;
    
    // Simulate reading delay
    printf("User '%s' is reading the document...\n", user->name);
    
    // Read in chunks, checking for priority exit flag between chunks
    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        if (priority_exit_flag) {
            printf("\n[!] Owner requested priority access. Releasing read lock.\n");
            break;
        }
        
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
        
        // Small delay to allow for priority interruption
        usleep(10000);  // 10ms delay
    }
    
    printf("\n--- End of Document ---\n");
    release_read_lock(fd, user);
    close(fd);
    
    // Reset exit flag after handling it
    priority_exit_flag = 0;
}
void edit_document(User *user) {
    // Check if owner is forcing a lock - if so, wait
    if (lock_info->forced_lock) {
        printf("Owner is currently taking over the document. Please wait.\n");
        return;
    }

    // Open the document with write access
    int fd = open(SHARED_DOC, O_RDWR);
    if (fd == -1) {
        perror("Error opening document for editing");
        return;
    }
   
    // Acquire exclusive write lock
    if (!acquire_write_lock(fd, user)) {
        close(fd);
        return;
    }
   
    // Set time allocation based on priority
    int time_allocation;
    if (user->priority == 2) {  // Assuming priority 2 is for owner
        time_allocation = 30;   // Owner gets 30 seconds
    } else if (user->priority == 1) {
        time_allocation = 10;   // High priority users get 10 seconds
    } else {
        time_allocation = 15;   // Regular users get 15 seconds
    }
    
    // Record start time and allocation
    lock_info->edit_start_time = time(NULL);
    lock_info->time_allocation = time_allocation;
    lock_info->time_limit_active = true;
    
    printf("Opening editor for user '%s' (Time allocation: %d seconds)...\n", 
           user->name, time_allocation);
   
   
    // Fork and exec to open nano editor
    pid_t pid = fork();
   
    if (pid == 0) {
        // Child process - make sure it ignores priority signal
        signal(PRIORITY_SIGNAL, SIG_IGN);
       
        // Redirect stdin, stdout to terminal for nano
        execlp("nano", "nano", "-B", SHARED_DOC, NULL);
        perror("Failed to open editor");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Store editor PID in shared memory so owner can interact with it directly
        lock_info->editor_pid = pid;
        
        // Parent process - wait for editor to close or priority signal
        int status;
        pid_t result;
        bool save_triggered = false;
        time_t start_time = time(NULL);
        int elapsed_time = 0;
        int time_remaining = time_allocation;
       
        while ((result = waitpid(pid, &status, WNOHANG)) == 0) {
            // Update elapsed time
            elapsed_time = time(NULL) - start_time;
            time_remaining = time_allocation - elapsed_time;
            
            // Check if time allocation is exceeded
            if (time_remaining <= 0) {
                printf("\n[!] Time allocation (%d seconds) has expired.\n", time_allocation);
                printf("Attempting to save your work...\n");
                
                
                
                // Give a short grace period for auto-saving (if implemented)
                sleep(1);
                
                // Then terminate the editor
                kill(pid, SIGTERM);
                waitpid(pid, &status, 0);
                break;
            }
            
            // Display time remaining every second
            static int last_displayed_time = -1;
            
            
            // Check if owner is forcing lock takeover
            if (lock_info->forced_lock) {
                printf("\n[!] Owner is forcing document takeover.\n");
                if (!save_triggered) {
                    printf("Attempting to save your work...\n");
                    save_triggered = true;
                }
                
                // Only terminate if countdown has reached zero
                if (lock_info->countdown_active && lock_info->countdown_value <= 0) {
                    printf("Editor will now close.\n");
                    kill(pid, SIGTERM);
                    waitpid(pid, &status, 0);
                    break;
                }
            }
            
            // Check for countdown
            if (lock_info->countdown_active) {
                if (!save_triggered && lock_info->countdown_value <= 2) {
                    printf("\n[!] Owner requesting priority access in %d seconds. Preparing to save...\n", 
                           lock_info->countdown_value);
                    // Only try to save when countdown gets to 2 or less
                    save_triggered = true;
                }
                
            }
            
            // Check priority flag for immediate exit
            if (priority_exit_flag) {
                printf("\n[!] Owner has priority access. Saving and closing editor.\n");
                
                // Try to save the file before closing
                if (!save_triggered) {
                    save_triggered = true;
                    sleep(1);
                }
                
                // Then terminate the editor
                kill(pid, SIGTERM);
                waitpid(pid, &status, 0);  // Wait for the child to terminate
                break;
            }
            usleep(100000);  // 100ms sleep
        }
       
        // Clear editor PID from shared memory
        lock_info->editor_pid = 0;
        lock_info->time_limit_active = false;
        
        if (result > 0 && !priority_exit_flag && !lock_info->forced_lock && time_remaining > 0) {
            printf("\nDocument editing completed by '%s'.\n", user->name);
        } else if (time_remaining <= 0) {
            printf("\nEditor closed due to time limit expiration.\n");
        } else {
            printf("\nEditor closed due to owner priority request.\n");
        }
    } else {
        perror("Fork failed");
    }
   
    // Release the lock
    release_write_lock(fd, user);
    close(fd);
   
    // Reset exit flag after handling it
    priority_exit_flag = 0;
    
    // If owner is waiting, print message about waiting in queue
    if (lock_info->owner_waiting) {
        printf("Owner has priority access. You are now in the queue.\n");
        printf("You may edit the document after the owner completes their edits.\n");
    }
}