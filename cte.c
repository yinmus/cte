#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

#define MAX_LINES 1000
#define MAX_LEN 256
#define AUTO_SAVE_INTERVAL 5
#define TAB_SPACE 4

char *clipboard = NULL;
int command_mode = 0;
char command_buffer[MAX_LEN] = {0};
char filename[256] = "untitled.txt";
int cursor_x = 0, cursor_y = 0, scroll_offset = 0;
int num_lines = 0;
char *lines[MAX_LINES];

void load_file(const char *fname) {
    FILE *fp = fopen(fname, "r");
    if (!fp) {
        strncpy(filename, fname, sizeof(filename) - 1);
        lines[0] = strdup("");
        num_lines = 1;
        return;
    }

    char buf[MAX_LEN];
    while (fgets(buf, MAX_LEN, fp) && num_lines < MAX_LINES) {
        buf[strcspn(buf, "\n")] = 0;
        lines[num_lines++] = strdup(buf);
    }
    fclose(fp);
    strncpy(filename, fname, sizeof(filename) - 1);
}

void save_file() {
    FILE *fp = fopen(filename, "w");
    if (!fp) return;
    for (int i = 0; i < num_lines; i++) {
        if (lines[i]) fprintf(fp, "%s\n", lines[i]);
    }
    fclose(fp);
}

void auto_save(int sig) {
    save_file();
    alarm(AUTO_SAVE_INTERVAL);
}

void editor() {
    clear();

    int num_width = snprintf(NULL, 0, "%d", num_lines) + 2;
    int screen_lines = LINES - 3;

    if (cursor_y >= scroll_offset + screen_lines) {
        scroll_offset = cursor_y - screen_lines + 1;
    }
    if (cursor_y < scroll_offset) {
        scroll_offset = cursor_y;
    }

    for (int i = 0; i < screen_lines && i + scroll_offset < num_lines; i++) {
        if (lines[i + scroll_offset]) {
            mvprintw(i, 0, "%*d| %s", num_width - 2, i + scroll_offset + 1, lines[i + scroll_offset]);
        }
    }

    attron(A_BOLD);
    mvhline(LINES - 3, 0, ACS_HLINE, COLS);
    attroff(A_BOLD);

    mvprintw(LINES - 2, 0, " [ %s ] %d/%d lines ", filename, cursor_y + 1, num_lines);

    if (command_mode) {
        attron(A_BOLD);
        mvhline(LINES - 1, 0, ACS_HLINE, COLS);
        attroff(A_BOLD);
        mvprintw(LINES - 1, 0, ":%s ", command_buffer);
    }

    int cursor_pos = cursor_x + num_width;
    if (cursor_pos >= COLS - 1) {
        cursor_pos = COLS - 1;
    }
    move(cursor_y - scroll_offset, cursor_pos);
}

void handle_i(int ch) {
    static int alt_flag = 0;

    if (alt_flag) {
        alt_flag = 0;
        if (ch == 'u') {
            command_mode = 1;
            memset(command_buffer, 0, MAX_LEN);
        } else if (ch == 'x') {
            for (int i = cursor_y; i < num_lines - 1; i++) {
                lines[i] = lines[i + 1];
            }
            lines[num_lines - 1] = NULL;
            num_lines--;
        }
        return;
    }

    if (ch == 27) {
        alt_flag = 1;
        return;
    }

    if (command_mode) {
        if (ch == '\n') {
            if (strcmp(command_buffer, "q") == 0) exit(0);
            if (strcmp(command_buffer, "w") == 0) save_file();
            if (strcmp(command_buffer, "wq") == 0) {
                save_file();
                exit(0);
            }
            memset(command_buffer, 0, MAX_LEN);
            command_mode = 0;
        } else if (ch == 27) {
            command_mode = 0;
        } else if ((ch == 127 || ch == KEY_BACKSPACE) && strlen(command_buffer) > 0) {
            command_buffer[strlen(command_buffer) - 1] = '\0';
        } else if (isprint(ch) && ch != ' ') {
            int len = strlen(command_buffer);
            if (len < MAX_LEN - 1) {
                command_buffer[len] = ch;
                command_buffer[len + 1] = '\0';
            }
        }
    } else {
        if (ch == KEY_BACKSPACE || ch == 127) {
            if (cursor_x > 0) {
                memmove(&lines[cursor_y][cursor_x - 1], &lines[cursor_y][cursor_x], strlen(&lines[cursor_y][cursor_x]) + 1);
                cursor_x--;
            } else if (cursor_y > 0) {
                char *temp = lines[cursor_y];
                lines[cursor_y - 1] = realloc(lines[cursor_y - 1], strlen(lines[cursor_y - 1]) + strlen(temp) + 1);
                strcat(lines[cursor_y - 1], temp);
                free(lines[cursor_y]);

                for (int i = cursor_y; i < num_lines - 1; i++) {
                    lines[i] = lines[i + 1];
                }
                lines[num_lines - 1] = NULL;
                num_lines--;
                cursor_y--;
                cursor_x = strlen(lines[cursor_y]);
            }
        } else if (ch == KEY_DC) {
            if (cursor_x < strlen(lines[cursor_y])) {
                memmove(&lines[cursor_y][cursor_x], &lines[cursor_y][cursor_x + 1], strlen(&lines[cursor_y][cursor_x]));
            }
        } else if (ch == '\n') {
            if (cursor_x < strlen(lines[cursor_y])) {
                memmove(&lines[cursor_y + 1], &lines[cursor_y], (num_lines - cursor_y) * sizeof(char *));
                lines[cursor_y + 1] = strdup(&lines[cursor_y][cursor_x]);
                lines[cursor_y][cursor_x] = '\0';
            } else {
                memmove(&lines[cursor_y + 1], &lines[cursor_y], (num_lines - cursor_y) * sizeof(char *));
                lines[cursor_y + 1] = strdup("");
            }
            num_lines++;
            cursor_y++;
            cursor_x = 0;
        } else if (ch == KEY_UP) {
            if (cursor_y > 0) cursor_y--;
        } else if (ch == KEY_DOWN) {
            if (cursor_y < num_lines - 1) cursor_y++;
        } else if (ch == KEY_LEFT) {
            if (cursor_x > 0) cursor_x--;
        } else if (ch == KEY_RIGHT) {
            if (cursor_x < strlen(lines[cursor_y])) cursor_x++;
        } else if (ch == '\t') {
            int len = strlen(lines[cursor_y]);
            if (len + TAB_SPACE < MAX_LEN) {
                lines[cursor_y] = realloc(lines[cursor_y], len + TAB_SPACE + 1);
                memmove(&lines[cursor_y][cursor_x + TAB_SPACE], &lines[cursor_y][cursor_x], len - cursor_x + 1);
                for (int i = 0; i < TAB_SPACE; i++) {
                    lines[cursor_y][cursor_x + i] = ' ';
                }
                cursor_x += TAB_SPACE;
            }
        } else if (isprint(ch)) {
            int len = strlen(lines[cursor_y]);
            if (len + 1 < MAX_LEN) {
                lines[cursor_y] = realloc(lines[cursor_y], len + 2);
                memmove(&lines[cursor_y][cursor_x + 1], &lines[cursor_y][cursor_x], len - cursor_x + 1);
                lines[cursor_y][cursor_x] = ch;
                cursor_x++;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    timeout(100);
    signal(SIGALRM, auto_save);
    alarm(AUTO_SAVE_INTERVAL);

    if (argc > 1) {
        load_file(argv[1]);
    } else {
        load_file(filename);
    }

    editor();

    while (1) {
        int ch = getch();
        handle_i(ch);
        editor();
    }

    endwin();
    return 0;
}
