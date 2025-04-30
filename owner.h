// owner.h

#ifndef OWNER_H
#define OWNER_H

#include "shared.h"


// Function to signal owner priority
// Forward declarations of owner-specific functions
void display_menu();
void create_shared_doc_if_not_exists();
void initialize_control_file();
int read_control_file(User users[], int max_users);
void write_control_file(User users[], int user_count);
void add_user();
void remove_user();
void update_user();
void list_users();
void view_document(User *user);
void edit_document(User *user);
void signal_owner_priority();

#endif // OWNER_H
