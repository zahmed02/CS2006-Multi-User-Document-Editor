// File: owner.c
// Main program for document owner/admin



// Forward declarations of owner-specific functions
void display_menu();
void create_shared_doc_if_not_exists();
void initialize_control_file();
void add_user();
void remove_user();
void update_user();
void list_users();

#include "shared.h"

int read_control_file(User users[], int max_users);
void write_control_file(User users[], int user_count);
void view_document(User *user);
void edit_document(User *user);

// Global variable for current owner
User owner_user;
int main() {
    int choice;
    
    // Check if document exists, if not create it
    create_shared_doc_if_not_exists();
    
    // Initialize control file if needed
    initialize_control_file();
    
    // Initialize synchronization mechanisms
    initialize_synchronization(true);  // true means owner
    
    // Create the current user object (owner)
    strcpy(owner_user.name, "admin");
    owner_user.priority = PRIORITY_OWNER;  // Owner has special priority
    owner_user.access_type = ACCESS_BOTH;
    owner_user.pid = getpid();
    
    printf("Owner process started with PID: %d\n", getpid());
    
    // Main menu loop
    while(1) {
        display_menu();
        scanf("%d", &choice);
        getchar(); // Clear newline
        
        switch(choice) {
            case 1:
                view_document(&owner_user);
                break;
            case 2:
                edit_document(&owner_user);
                break;
            case 3:
                add_user();
                break;
            case 4:
                remove_user();
                break;
            case 5:
                update_user();
                break;
            case 6:
                list_users();
                break;
            case 7:
               append_to_history();
               break;
            case 8:
                pop_last_snapshot(); 
                break;
            case 9:
                 print_history(); 
                break;
            case 10:
                printf("Exiting owner program.\n");
                cleanup_synchronization(true);  // true means owner
                exit(0);
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }
    
    return 0;
}

void display_menu() {
    printf("\n=== Document Sharing System (Owner/Admin) ===\n");
    printf("1. View document (read)\n");
    printf("2. Edit document (write)\n");
    printf("3. Add user\n");
    printf("4. Remove user\n");
    printf("5. Update user access\n");
    printf("6. List all users\n");
    printf("7. Push History\n"); 
    printf("8. POP History\n");
    printf("9. View History Log\n");
    printf("10. Exit\n");
    
    printf("Enter your choice: ");
}

void create_shared_doc_if_not_exists() {
    FILE *file = fopen(SHARED_DOC, "a+");
    if (file == NULL) {
        perror("Error creating shared document");
        exit(EXIT_FAILURE);
    }
    fclose(file);
    printf("Shared document verified or created.\n");
}

void initialize_control_file() {
    FILE *file = fopen(CONTROL_FILE, "r");
    
    if (file == NULL) {
        // Control file doesn't exist, create it
        file = fopen(CONTROL_FILE, "w");
        if (file == NULL) {
            perror("Error creating control file");
            exit(EXIT_FAILURE);
        }
        
        // Add admin user
        fprintf(file, "%s\n", SHARED_DOC);
        fprintf(file, "admin %d %d %d\n", PRIORITY_OWNER, ACCESS_BOTH, getpid());
        fprintf(file, "0\n"); // No additional users initially
        
        printf("Control file initialized with admin user.\n");
    } else {
        fclose(file);
        
        // Update admin's PID in the control file
        User users[MAX_USERS];
        int user_count = read_control_file(users, MAX_USERS);
        users[0].pid = getpid();  // Update admin PID
        write_control_file(users, user_count);
    }
}

int read_control_file(User users[], int max_users) {
    FILE *file = fopen(CONTROL_FILE, "r");
    if (file == NULL) {
        perror("Error opening control file");
        exit(EXIT_FAILURE);
    }
    
    char line[MAX_LINE];
    // Read and ignore first line (document path)
    fgets(line, MAX_LINE, file);
    
    // Read admin info
    fgets(line, MAX_LINE, file);
    sscanf(line, "%s %d %d %d", users[0].name, &users[0].priority, &users[0].access_type, &users[0].pid);
    
    // Make sure admin has owner priority
    users[0].priority = PRIORITY_OWNER;
    
    // Read number of additional users
    int user_count;
    fgets(line, MAX_LINE, file);
    sscanf(line, "%d", &user_count);
    
    if (user_count > max_users - 1) {
        user_count = max_users - 1;
    }
    
    // Read remaining users
    for (int i = 0; i < user_count; i++) {
        if (fgets(line, MAX_LINE, file) != NULL) {
            sscanf(line, "%s %d %d %d", users[i+1].name, &users[i+1].priority, &users[i+1].access_type, &users[i+1].pid);
        }
    }
    
    fclose(file);
    return user_count + 1; // Include admin
}

void write_control_file(User users[], int user_count) {
    FILE *file = fopen(CONTROL_FILE, "w");
    if (file == NULL) {
        perror("Error opening control file for writing");
        exit(EXIT_FAILURE);
    }
    
    // Write document path
    fprintf(file, "%s\n", SHARED_DOC);
    
    // Write admin info (always ensure admin has owner priority)
    users[0].priority = PRIORITY_OWNER;  // Ensure owner priority
    fprintf(file, "%s %d %d %d\n", users[0].name, users[0].priority, users[0].access_type, users[0].pid);
    
    // Write number of additional users
    fprintf(file, "%d\n", user_count - 1);
    
    // Write remaining users
    for (int i = 1; i < user_count; i++) {
        fprintf(file, "%s %d %d %d\n", users[i].name, users[i].priority, users[i].access_type, users[i].pid);
    }
    
    fclose(file);
}

// Function to check if a process with given PID exists
bool process_exists(pid_t pid) {
    if (pid <= 0) return false;
    if (kill(pid, 0) == 0) {
        return true;
    }
    return false;
}

// Function to send priority signal to a process
void send_priority_signal(pid_t pid) {
    if (process_exists(pid)) {
        printf("Sending priority signal to process %d\n", pid);
        kill(pid, PRIORITY_SIGNAL);
    }
}

void view_document(User *user) {
    // Tell the system owner is waiting for access
    lock_info->owner_waiting = true;
    
    int fd = open(SHARED_DOC, O_RDONLY);
    if (fd == -1) {
        perror("Error opening document for reading");
        lock_info->owner_waiting = false;
        return;
    }
    
    // Check if another process holds the write lock
    if (lock_info->lock_type == 2 && lock_info->holding_pid > 0) {
        printf("Document is currently locked by process %d, sending priority signal\n", 
               lock_info->holding_pid);
        send_priority_signal(lock_info->holding_pid);
        
        // Short wait to allow the process to release lock
        sleep(1);
    }
    
    if (!acquire_read_lock(fd, user)) {
        close(fd);
        lock_info->owner_waiting = false;
        return;
    }
    
    // Owner no longer waiting once lock is acquired
    lock_info->owner_waiting = false;
    
    printf("\n--- Document Content ---\n");
    char buffer[1024];
    ssize_t bytes_read;
    
    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }
    
    printf("\n--- End of Document ---\n");
    
    release_read_lock(fd, user);
    close(fd);
}

void edit_document(User *user) {
    // Tell the system owner is waiting for access
    lock_info->owner_waiting = true;
    lock_info->forced_lock = true;  // Force lock acquisition
   
    // Open the document with write access
    int fd = open(SHARED_DOC, O_RDWR);
    if (fd == -1) {
        perror("Error opening document for editing");
        lock_info->owner_waiting = false;
        lock_info->forced_lock = false;
        return;
    }
   
    // Check if any process holds the lock and if time limiting is active
    if ((lock_info->lock_type == 1 || lock_info->lock_type == 2) &&
        lock_info->holding_pid > 0) {
        
        printf("Document is currently locked by process %d\n", lock_info->holding_pid);
        
        // If time limiting is active, check remaining time
        if (lock_info->time_limit_active) {
            time_t current_time = time(NULL);
            int elapsed_time = current_time - lock_info->edit_start_time;
            int remaining_time = lock_info->time_allocation - elapsed_time;
            
            if (remaining_time > 5) {
                printf("Current user has %d seconds remaining in their time allocation.\n", remaining_time);
                printf("Starting 5-second countdown for owner priority access...\n");
            } else {
                printf("Current user's time is almost up (%d seconds left). Waiting briefly...\n", remaining_time);
                sleep(remaining_time > 0 ? remaining_time : 1);
                printf("Proceeding to take over document...\n");
            }
        } else {
            printf("Starting 5-second countdown for owner priority access...\n");
        }
        
        // Set countdown flag in shared memory
        lock_info->countdown_active = true;
        lock_info->countdown_value = 5;
        
        // Count down from 5 to 0
        for (int i = 5; i >= 0; i--) {
            lock_info->countdown_value = i;
            printf("Owner taking over in %d seconds...\n", i);
            
            // If there's an active editor process, we can directly signal it
            if (lock_info->editor_pid > 0) {
                // Send forceful termination at countdown = 0
                if (i == 0) {
                    printf("Forcing editor to close and taking control...\n");
                    kill(lock_info->editor_pid, SIGTERM);
                } else if (i <= 2) {
                    // Send save signal when countdown reaches 2
                    printf("Sending save signal to editor...\n");
                    kill(lock_info->editor_pid, SIGUSR1);
                }
            }
            
            sleep(1);
        }
        
        // Reset countdown flag
        lock_info->countdown_active = false;
        
        // Force release of lock by setting holding_pid to 0
        // This is a forceful approach that bypasses normal release procedures
        sem_wait(access_sem);
        lock_info->holding_pid = 0;
        lock_info->lock_type = 0;
        sem_post(access_sem);
        
        // Short wait to ensure cleanup
        sleep(1);
    }
   
    // Acquire exclusive write lock - now it should succeed since we forced release
    if (!acquire_write_lock(fd, user)) {
        close(fd);
        lock_info->owner_waiting = false;
        lock_info->forced_lock = false;
        return;
    }
   
    // Owner no longer waiting once lock is acquired
    lock_info->owner_waiting = false;
   
    // Set time allocation for owner (30 seconds)
    int time_allocation = 30;
    lock_info->edit_start_time = time(NULL);
    lock_info->time_allocation = time_allocation;
    lock_info->time_limit_active = true;
    
    printf("Opening editor for owner (Time allocation: %d seconds)...\n", time_allocation);
   
    // Fork and exec to open nano editor
    pid_t pid = fork();
   
    if (pid == 0) {
        // Child process
        execlp("nano", "nano", "-B", SHARED_DOC, NULL);
        perror("Failed to open editor");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Store editor PID
        lock_info->editor_pid = pid;
        
        // Parent process - wait for editor to close or time expiration
        int status;
        pid_t result;
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
                printf("Saving and closing editor...\n");
                
                // Try to save work (send signal or implement a save mechanism)
                
                // Short grace period
                sleep(1);
                
                // Terminate editor
                kill(pid, SIGTERM);
                waitpid(pid, &status, 0);
                break;
            }
            
            // Display time remaining every second
            static int last_displayed_time = -1;
            // if (time_remaining != last_displayed_time) {
            //     //printf("\rTime remaining: %d seconds  ", time_remaining);
            //     fflush(stdout);
            //     last_displayed_time = time_remaining;
            // }
            
            usleep(100000);  // 100ms sleep
        }
        
        // Clear editor PID
        lock_info->editor_pid = 0;
        lock_info->time_limit_active = false;
        
        if (result > 0 && time_remaining > 0) {
            printf("\nDocument editing completed by owner.\n");
        } else {
            printf("\nEditor closed due to time limit expiration.\n");
        }
    } else {
        perror("Fork failed");
    }
   
    // Release the lock
    release_write_lock(fd, user);
    lock_info->forced_lock = false;  // Reset forced lock flag
    close(fd);
}

void add_user() {
    User users[MAX_USERS];
    int user_count = read_control_file(users, MAX_USERS);
    
    if (user_count >= MAX_USERS) {
        printf("Maximum number of users reached.\n");
        return;
    }
    
    User new_user;
    
    printf("Enter new user name: ");
    scanf("%s", new_user.name);
    getchar(); // Clear newline
    
    // Check if user already exists
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].name, new_user.name) == 0) {
            printf("User '%s' already exists.\n", new_user.name);
            return;
        }
    }
    
    printf("Enter priority (0 for high, 1 for low): ");
    scanf("%d", &new_user.priority);
    getchar(); // Clear newline
    
    // Ensure priority is valid (only 0 or 1 for regular users)
    if (new_user.priority != PRIORITY_HIGH && new_user.priority != PRIORITY_LOW) {
        printf("Invalid priority. Setting to default (low priority).\n");
        new_user.priority = PRIORITY_LOW;
    }
    
    printf("Enter access type (1: read-only, 2: write-only, 3: both): ");
    scanf("%d", &new_user.access_type);
    getchar(); // Clear newline
    
    // Verify access type is valid
    if (new_user.access_type < ACCESS_READ_ONLY || new_user.access_type > ACCESS_BOTH) {
        printf("Invalid access type. Setting to default (read-only).\n");
        new_user.access_type = ACCESS_READ_ONLY;
    }
    
    // Generate a placeholder PID since the user isn't running yet
    new_user.pid = 0;
    
    // Add new user to the array
    users[user_count] = new_user;
    
    // Update control file
    write_control_file(users, user_count + 1);
    
    printf("User '%s' added successfully.\n", new_user.name);
}

void remove_user() {
    User users[MAX_USERS];
    int user_count = read_control_file(users, MAX_USERS);
    
    char name[50];
    printf("Enter user name to remove: ");
    scanf("%s", name);
    getchar(); // Clear newline
    
    // Cannot remove admin
    if (strcmp(name, "admin") == 0) {
        printf("Cannot remove admin user.\n");
        return;
    }
    
    int found = -1;
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].name, name) == 0) {
            found = i;
            break;
        }
    }
    
    if (found != -1) {
        // If the user process is currently running, send it a signal
        if (process_exists(users[found].pid)) {
            printf("User '%s' is currently running (PID: %d). Sending termination signal.\n", 
                   name, users[found].pid);
            kill(users[found].pid, SIGTERM);
        }
        
        // Shift users to remove the found user
        for (int i = found; i < user_count - 1; i++) {
            users[i] = users[i + 1];
        }
        
        // Update control file
        write_control_file(users, user_count - 1);
        printf("User '%s' removed successfully.\n", name);
    } else {
        printf("User '%s' not found.\n", name);
    }
}

void update_user() {
    User users[MAX_USERS];
    int user_count = read_control_file(users, MAX_USERS);
    
    char name[50];
    printf("Enter user name to update: ");
    scanf("%s", name);
    getchar(); // Clear newline
    
    // Cannot update admin's properties
    if (strcmp(name, "admin") == 0) {
        printf("Cannot modify admin user's properties.\n");
        return;
    }
    
    int found = -1;
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].name, name) == 0) {
            found = i;
            break;
        }
    }
    
    if (found != -1) {
        printf("Current priority: %d, access type: %d\n", 
               users[found].priority, users[found].access_type);
        
        printf("Enter new priority (0 for high, 1 for low): ");
        scanf("%d", &users[found].priority);
        getchar(); // Clear newline
        
        // Validate priority
        if (users[found].priority != PRIORITY_HIGH && users[found].priority != PRIORITY_LOW) {
            printf("Invalid priority. Setting to default (low priority).\n");
            users[found].priority = PRIORITY_LOW;
        }
        
        printf("Enter new access type (1: read-only, 2: write-only, 3: both): ");
        scanf("%d", &users[found].access_type);
        getchar(); // Clear newline
        
        // Validate access type
        if (users[found].access_type < ACCESS_READ_ONLY || users[found].access_type > ACCESS_BOTH) {
            printf("Invalid access type. Setting to default (read-only).\n");
            users[found].access_type = ACCESS_READ_ONLY;
        }
        
        // If the user is currently running, notify of changes
        if (process_exists(users[found].pid)) {
            printf("User '%s' is currently running (PID: %d). Changes will take effect on next login.\n", 
                   name, users[found].pid);
        }
        
        // Update control file
        write_control_file(users, user_count);
        printf("User '%s' updated successfully.\n", name);
    } else {
        printf("User '%s' not found.\n", name);
    }
}

void list_users() {
    User users[MAX_USERS];
    int user_count = read_control_file(users, MAX_USERS);
    
    printf("\n--- User List ---\n");
    printf("%-20s %-10s %-15s %-10s %-10s\n", "Name", "Priority", "Access Type", "PID", "Status");
    printf("----------------------------------------------------------------\n");
    
    for (int i = 0; i < user_count; i++) {
        const char *priority;
        if (users[i].priority == PRIORITY_OWNER)
            priority = "Owner";
        else if (users[i].priority == PRIORITY_HIGH)
            priority = "High";
        else
            priority = "Low";
            
        const char *access;
        switch (users[i].access_type) {
            case ACCESS_READ_ONLY:
                access = "Read-only";
                break;
            case ACCESS_WRITE_ONLY:
                access = "Write-only";
                break;
            case ACCESS_BOTH:
                access = "Read-Write";
                break;
            default:
                access = "Unknown";
        }
        
        const char *status = (users[i].pid > 0 && process_exists(users[i].pid)) ? "Active" : "Inactive";
        
        printf("%-20s %-10s %-15s %-10d %-10s\n", 
               users[i].name, priority, access, users[i].pid, status);
    }
    
    printf("--- End of User List ---\n");
}