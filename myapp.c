#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_USERS 10
#define MAX_USERNAME 20
#define MAX_TEXT 10000
#define MAX_LINE 100
#define MAX_FILENAME 50

typedef struct {
    char username[MAX_USERNAME];
    int isOwner;
    int isWriter;
} User;

typedef struct {
    char text[MAX_TEXT];
    int cursor_pos;
    int current_color;
    int is_bold;
    int is_italic;
    int is_underline;
    int text_size; // 1-small, 2-normal, 3-large
    char filename[MAX_FILENAME];
} Document;

// Global variables
User users[MAX_USERS];
int user_count = 0;
int current_user = -1;
Document doc;

// Function prototypes
void init_colors();
void draw_status_bar(WINDOW *win);
void draw_text_area(WINDOW *win);
void draw_format_bar(WINDOW *win);
void process_key(int ch);
void save_document();
void add_user();
void remove_user();
void select_user();
int is_owner();
int is_writer();
void init_document();
void apply_formatting(int ch);

int main() {
    // Initialize ncurses
    initscr();
    start_color();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(1);
    
    if (!has_colors()) {
        endwin();
        printf("Your terminal does not support color\n");
        return 1;
    }
    
    init_colors();
    init_document();
    
    // Create default owner user
    strcpy(users[0].username, "admin");
    users[0].isOwner = 1;
    users[0].isWriter = 1;
    user_count = 1;
    current_user = 0;
    
    // Create windows
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    WINDOW *status_bar = newwin(1, max_x, 0, 0);
    WINDOW *format_bar = newwin(1, max_x, 1, 0);
    WINDOW *text_area = newwin(max_y-3, max_x, 2, 0);
    WINDOW *command_bar = newwin(1, max_x, max_y-1, 0);
    
    keypad(text_area, TRUE);
    
    // Main loop
    int ch;
    int running = 1;
    
    while (running) {
        // Draw windows
        werase(status_bar);
        werase(format_bar);
        werase(text_area);
        werase(command_bar);
        
        draw_status_bar(status_bar);
        if (is_writer()) {
            draw_format_bar(format_bar);
        }
        draw_text_area(text_area);
        
        // Command bar instructions
        mvwprintw(command_bar, 0, 0, "Commands: s-Save | x-Exit | a-Add User | r-Remove User | u-Select User");
        
        // Refresh windows
        wrefresh(status_bar);
        wrefresh(format_bar);
        wrefresh(text_area);
        wrefresh(command_bar);
        
        // Get input
        ch = getch();
        
        // Process control keys
        if (ch == 's' && is_writer()) { // Save
            save_document();
        } else if (ch == 'x') { // Exit
            running = 0;
        } else if (ch == 'a' && is_owner()) { // Add user
            add_user();
        } else if (ch == 'r' && is_owner()) { // Remove user
            remove_user();
        } else if (ch == 'u') { // Select user
            select_user();
        } else if (is_writer()) {
            // Process text formatting and editing
            if (ch == KEY_F(1)) { // Bold
                doc.is_bold = !doc.is_bold;
            } else if (ch == KEY_F(2)) { // Italic
                doc.is_italic = !doc.is_italic;
            } else if (ch == KEY_F(3)) { // Underline
                doc.is_underline = !doc.is_underline;
            } else if (ch == KEY_F(4)) { // Cycle text size
                doc.text_size = (doc.text_size % 3) + 1;
            } else if (ch == KEY_F(5)) { // Cycle colors
                doc.current_color = (doc.current_color % 7) + 1;
            } else {
                process_key(ch);
            }
        }
    }
    
    // Clean up
    delwin(status_bar);
    delwin(format_bar);
    delwin(text_area);
    delwin(command_bar);
    endwin();
    
    return 0;
}

void init_colors() {
    // Initialize color pairs
    init_pair(1, COLOR_WHITE, COLOR_BLACK);  // Default
    init_pair(2, COLOR_RED, COLOR_BLACK);    // Red
    init_pair(3, COLOR_GREEN, COLOR_BLACK);  // Green
    init_pair(4, COLOR_BLUE, COLOR_BLACK);   // Blue
    init_pair(5, COLOR_YELLOW, COLOR_BLACK); // Yellow
    init_pair(6, COLOR_CYAN, COLOR_BLACK);   // Cyan
    init_pair(7, COLOR_MAGENTA, COLOR_BLACK); // Magenta
}

void init_document() {
    memset(doc.text, 0, MAX_TEXT);
    doc.cursor_pos = 0;
    doc.current_color = 1;
    doc.is_bold = 0;
    doc.is_italic = 0;
    doc.is_underline = 0;
    doc.text_size = 2; // Normal by default
    strcpy(doc.filename, "document.txt");
}

void draw_status_bar(WINDOW *win) {
    wattron(win, A_REVERSE);
    mvwprintw(win, 0, 0, "Docs-like App | User: %s | %s | %s", 
              users[current_user].username,
              is_owner() ? "Owner" : "Viewer",
              is_writer() ? "Writer" : "Reader");
    
    int max_x = getmaxx(win);
    for (int i = getcurx(win); i < max_x; i++) {
        waddch(win, ' ');
    }
    
    wattroff(win, A_REVERSE);
}

void draw_format_bar(WINDOW *win) {
    wattron(win, A_REVERSE);
    
    // Show formatting options if the user is a writer
    mvwprintw(win, 0, 0, "Format: ");
    mvwprintw(win, 0, 8, "F1:Bold[%s] ", doc.is_bold ? "ON" : "OFF");
    mvwprintw(win, 0, 20, "F2:Italic[%s] ", doc.is_italic ? "ON" : "OFF");
    mvwprintw(win, 0, 34, "F3:Underline[%s] ", doc.is_underline ? "ON" : "OFF");
    
    // Size indicator
    const char *size_str;
    switch(doc.text_size) {
        case 1: size_str = "Small"; break;
        case 2: size_str = "Normal"; break;
        case 3: size_str = "Large"; break;
        default: size_str = "Normal";
    }
    mvwprintw(win, 0, 52, "F4:Size[%s] ", size_str);
    
    // Color indicator
    wattron(win, COLOR_PAIR(doc.current_color));
    mvwprintw(win, 0, 66, "F5:Color ");
    wattroff(win, COLOR_PAIR(doc.current_color));
    
    int max_x = getmaxx(win);
    for (int i = getcurx(win); i < max_x; i++) {
        waddch(win, ' ');
    }
    
    wattroff(win, A_REVERSE);
}

void draw_text_area(WINDOW *win) {
    box(win, 0, 0);
    
    // Parse and display the document with formatting
    int y = 1;
    int x = 1;
    int max_y = getmaxy(win) - 2;
    int max_x = getmaxx(win) - 2;
    
    // Track current formatting state
    int current_attr = 0;
    int current_color_pair = 1;
    
    wattron(win, current_attr | COLOR_PAIR(current_color_pair));
    
    for (int i = 0; i < strlen(doc.text); i++) {
        // Special formatting tokens (simplified for this example)
        if (doc.text[i] == '\n') {
            y++;
            x = 1;
            if (y >= max_y) break;
            continue;
        }
        
        // Check if we need to parse formatting codes
        if (doc.text[i] == '\\' && i+1 < strlen(doc.text)) {
            switch (doc.text[i+1]) {
                case 'b': // Bold toggle
                    i++;
                    if (current_attr & A_BOLD)
                        current_attr &= ~A_BOLD;
                    else
                        current_attr |= A_BOLD;
                    
                    wattrset(win, current_attr | COLOR_PAIR(current_color_pair));
                    continue;
                case 'i': // Italic toggle
                    i++;
                    if (current_attr & A_ITALIC)
                        current_attr &= ~A_ITALIC;
                    else
                        current_attr |= A_ITALIC;
                    
                    wattrset(win, current_attr | COLOR_PAIR(current_color_pair));
                    continue;
                case 'u': // Underline toggle
                    i++;
                    if (current_attr & A_UNDERLINE)
                        current_attr &= ~A_UNDERLINE;
                    else
                        current_attr |= A_UNDERLINE;
                    
                    wattrset(win, current_attr | COLOR_PAIR(current_color_pair));
                    continue;
                case 'c': // Color change
                    if (i+2 < strlen(doc.text) && isdigit(doc.text[i+2])) {
                        current_color_pair = doc.text[i+2] - '0';
                        if (current_color_pair < 1 || current_color_pair > 7)
                            current_color_pair = 1;
                        i += 2;
                        wattrset(win, current_attr | COLOR_PAIR(current_color_pair));
                        continue;
                    }
                    break;
                case 's': // Size change
                    if (i+2 < strlen(doc.text) && isdigit(doc.text[i+2])) {
                        // Size is just a marker, actual rendering would depend on terminal
                        i += 2;
                        continue;
                    }
                    break;
            }
        }
        
        // Display regular character
        if (x <= max_x) {
            mvwaddch(win, y, x, doc.text[i]);
            x++;
        } else {
            y++;
            x = 1;
            if (y >= max_y) break;
            mvwaddch(win, y, x, doc.text[i]);
            x++;
        }
    }
    
    // Position cursor at insertion point
    int cursor_y = 1;
    int cursor_x = 1;
    int count = 0;
    
    for (int i = 0; i < doc.cursor_pos && i < strlen(doc.text); i++) {
        if (doc.text[i] == '\n') {
            cursor_y++;
            cursor_x = 1;
        } else if (doc.text[i] == '\\' && i+1 < strlen(doc.text)) {
            // Skip formatting codes
            if (strchr("biucs", doc.text[i+1])) {
                i++;
                if (strchr("cs", doc.text[i]) && i+1 < strlen(doc.text))
                    i++;
            } else {
                cursor_x++;
            }
        } else {
            cursor_x++;
        }
        
        if (cursor_x > max_x) {
            cursor_y++;
            cursor_x = 1;
        }
    }
    
    // Move cursor to edit position if writer
    if (is_writer()) {
        wmove(win, cursor_y, cursor_x);
    }
}

void process_key(int ch) {
    if (!is_writer()) return;
    
    if (ch == KEY_BACKSPACE || ch == 127) {
        if (doc.cursor_pos > 0) {
            memmove(&doc.text[doc.cursor_pos-1], &doc.text[doc.cursor_pos], 
                    strlen(doc.text) - doc.cursor_pos + 1);
            doc.cursor_pos--;
        }
    } else if (ch == KEY_DC) { // Delete key
        if (doc.cursor_pos < strlen(doc.text)) {
            memmove(&doc.text[doc.cursor_pos], &doc.text[doc.cursor_pos+1], 
                    strlen(doc.text) - doc.cursor_pos);
        }
    } else if (ch == KEY_LEFT) {
        if (doc.cursor_pos > 0) doc.cursor_pos--;
    } else if (ch == KEY_RIGHT) {
        if (doc.cursor_pos < strlen(doc.text)) doc.cursor_pos++;
    } else if (ch == KEY_UP || ch == KEY_DOWN) {
        // Simplified cursor movement for this example
        // Would need more complex handling for proper line navigation
    } else if (ch == '\n' || ch == KEY_ENTER || ch == 10 || ch == 13) {
        // Insert newline
        memmove(&doc.text[doc.cursor_pos+1], &doc.text[doc.cursor_pos], 
                strlen(doc.text) - doc.cursor_pos + 1);
        doc.text[doc.cursor_pos] = '\n';
        doc.cursor_pos++;
    } else if (ch == KEY_F(1) || ch == KEY_F(2) || ch == KEY_F(3) || 
               ch == KEY_F(4) || ch == KEY_F(5)) {
        // Formatting handled elsewhere
    } else if (isprint(ch)) {
        // Insert regular character
        if (strlen(doc.text) < MAX_TEXT - 1) {
            memmove(&doc.text[doc.cursor_pos+1], &doc.text[doc.cursor_pos], 
                    strlen(doc.text) - doc.cursor_pos + 1);
            doc.text[doc.cursor_pos] = ch;
            doc.cursor_pos++;
        }
    }
}

void apply_formatting(int ch) {
    // Create formatting marker
    char format_code[4] = {0};
    
    switch(ch) {
        case KEY_F(1): // Bold
            strcpy(format_code, "\\b");
            break;
        case KEY_F(2): // Italic
            strcpy(format_code, "\\i");
            break;
        case KEY_F(3): // Underline
            strcpy(format_code, "\\u");
            break;
        case KEY_F(4): // Size
            sprintf(format_code, "\\s%d", doc.text_size);
            break;
        case KEY_F(5): // Color
            sprintf(format_code, "\\c%d", doc.current_color);
            break;
    }
    
    // Insert the format code at cursor position
    if (format_code[0] && strlen(doc.text) + strlen(format_code) < MAX_TEXT) {
        memmove(&doc.text[doc.cursor_pos + strlen(format_code)], 
                &doc.text[doc.cursor_pos], 
                strlen(doc.text) - doc.cursor_pos + 1);
        
        for (int i = 0; i < strlen(format_code); i++) {
            doc.text[doc.cursor_pos + i] = format_code[i];
        }
        
        doc.cursor_pos += strlen(format_code);
    }
}

void save_document() {
    FILE *fp = fopen(doc.filename, "w");
    if (fp) {
        fprintf(fp, "%s", doc.text);
        fclose(fp);
    }
}

void add_user() {
    if (!is_owner() || user_count >= MAX_USERS) return;
    
    // Simple implementation - in a real app, you'd use a form
    echo();
    curs_set(1);
    
    char username[MAX_USERNAME] = {0};
    int is_writer = 0;
    
    clear();
    mvprintw(1, 1, "Add New User");
    mvprintw(3, 1, "Username: ");
    refresh();
    getnstr(username, MAX_USERNAME-1);
    
    mvprintw(4, 1, "Writer access? (y/n): ");
    refresh();
    char writer_choice = getch();
    is_writer = (writer_choice == 'y' || writer_choice == 'Y');
    
    // Add the user
    strcpy(users[user_count].username, username);
    users[user_count].isOwner = 0;
    users[user_count].isWriter = is_writer;
    user_count++;
    
    noecho();
    curs_set(1);
    clear();
}

void remove_user() {
    if (!is_owner() || user_count <= 1) return;
    
    // Simple implementation
    echo();
    curs_set(1);
    
    int user_to_remove = -1;
    
    clear();
    mvprintw(1, 1, "Remove User");
    mvprintw(2, 1, "Available users:");
    
    for (int i = 0; i < user_count; i++) {
        if (i != 0) { // Don't show admin in the list
            mvprintw(3+i, 1, "%d. %s", i, users[i].username);
        }
    }
    
    mvprintw(4+user_count, 1, "Enter user number to remove: ");
    refresh();
    
    char input[10];
    getnstr(input, 9);
    user_to_remove = atoi(input);
    
    if (user_to_remove > 0 && user_to_remove < user_count) {
        // Remove the user (shift array)
        for (int i = user_to_remove; i < user_count - 1; i++) {
            strcpy(users[i].username, users[i+1].username);
            users[i].isOwner = users[i+1].isOwner;
            users[i].isWriter = users[i+1].isWriter;
        }
        user_count--;
        
        // Adjust current user if needed
        if (current_user == user_to_remove) {
            current_user = 0; // Switch to admin
        } else if (current_user > user_to_remove) {
            current_user--; // Adjust index after removal
        }
    }
    
    noecho();
    curs_set(1);
    clear();
}

void select_user() {
    echo();
    curs_set(1);
    
    clear();
    mvprintw(1, 1, "Select User");
    mvprintw(2, 1, "Available users:");
    
    for (int i = 0; i < user_count; i++) {
        mvprintw(3+i, 1, "%d. %s", i, users[i].username);
    }
    
    mvprintw(4+user_count, 1, "Enter user number: ");
    refresh();
    
    char input[10];
    getnstr(input, 9);
    int selected_user = atoi(input);
    
    if (selected_user >= 0 && selected_user < user_count) {
        current_user = selected_user;
    }
    
    noecho();
    curs_set(1);
    clear();
}

int is_owner() {
    if (current_user >= 0 && current_user < user_count) {
        return users[current_user].isOwner;
    }
    return 0;
}

int is_writer() {
    if (current_user >= 0 && current_user < user_count) {
        return users[current_user].isWriter;
    }
    return 0;
}