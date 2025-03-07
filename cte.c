#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <locale.h>
#include <wchar.h>
#include <ctype.h>
#include <wctype.h>

#define MAX_LINES 1000
#define MAX_LEN 256
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

    int cursor_pos = 0;
    int byte_pos = 0;
    while (byte_pos < cursor_x) {
        int char_len = mblen(&lines[cursor_y][byte_pos], MB_CUR_MAX);
        if (char_len <= 0) break;
        cursor_pos++;
        byte_pos += char_len;
    }

    move(cursor_y - scroll_offset, cursor_pos + num_width);
}

void dlt_word() {
    if (cursor_x == 0) {
        if (cursor_y > 0) {
            cursor_y--;
            cursor_x = strlen(lines[cursor_y]);
            lines[cursor_y] = realloc(lines[cursor_y], strlen(lines[cursor_y]) + strlen(lines[cursor_y + 1]) + 1);
            strcat(lines[cursor_y], lines[cursor_y + 1]);
            for (int i = cursor_y + 1; i < num_lines - 1; i++) {
                lines[i] = lines[i + 1];
            }
            lines[num_lines - 1] = NULL;
            num_lines--;
        }
    } else {
        int i = cursor_x - 1;
        while (i >= 0 && isspace(lines[cursor_y][i])) i--;
        while (i >= 0 && !isspace(lines[cursor_y][i])) i--;
        int new_x = i + 1;
        memmove(&lines[cursor_y][new_x], &lines[cursor_y][cursor_x], strlen(&lines[cursor_y][cursor_x]) + 1);
        cursor_x = new_x;
    }
}

void handle_i(int ch) {
    static int alt_flag = 0;

    if (alt_flag) {
        alt_flag = 0;
        if (ch == 'c') {
            command_mode = 1;
            memset(command_buffer, 0, MAX_LEN);
        } else if (ch == 'x') {
            for (int i = cursor_y; i < num_lines - 1; i++) {
                lines[i] = lines[i + 1];
            }
            lines[num_lines - 1] = NULL;
            num_lines--;
        } else if (ch == 127 || ch == KEY_BACKSPACE) { 
            dlt_word();
        }
        return;
    }

    if (ch == 27) { // Alt 
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
        switch (ch) {
            case KEY_BACKSPACE:
            case 127:
                if (cursor_x > 0) {
                    int char_len = mblen(&lines[cursor_y][cursor_x - 1], MB_CUR_MAX);
                    if (char_len <= 0) char_len = 1;
                    memmove(&lines[cursor_y][cursor_x - char_len], &lines[cursor_y][cursor_x], strlen(&lines[cursor_y][cursor_x]) + 1);
                    cursor_x -= char_len;
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
                break;

            case KEY_DC:
                if (cursor_x < strlen(lines[cursor_y])) {
                    int char_len = mblen(&lines[cursor_y][cursor_x], MB_CUR_MAX);
                    if (char_len <= 0) char_len = 1;
                    memmove(&lines[cursor_y][cursor_x], &lines[cursor_y][cursor_x + char_len], strlen(&lines[cursor_y][cursor_x + char_len]) + 1);
                }
                break;

            case '\n':
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
                break;

            case KEY_UP:
                if (cursor_y > 0) cursor_y--;
                break;

            case KEY_DOWN:
                if (cursor_y < num_lines - 1) cursor_y++;
                break;

            case KEY_LEFT:
                if (cursor_x > 0) {
                    int char_len = mblen(&lines[cursor_y][cursor_x - 1], MB_CUR_MAX);
                    if (char_len <= 0) char_len = 1;
                    cursor_x -= char_len;
                } else if (cursor_y > 0) {
                    cursor_y--;
                    cursor_x = strlen(lines[cursor_y]);
                }
                break;

            case KEY_RIGHT:
                if (cursor_x < strlen(lines[cursor_y])) {
                    int char_len = mblen(&lines[cursor_y][cursor_x], MB_CUR_MAX);
                    if (char_len <= 0) char_len = 1;
                    cursor_x += char_len;
                } else if (cursor_y < num_lines - 1) {
                    cursor_y++;
                    cursor_x = 0;
                }
                break;

            case '\t':
                int len = strlen(lines[cursor_y]);
                if (len + TAB_SPACE < MAX_LEN) {
                    lines[cursor_y] = realloc(lines[cursor_y], len + TAB_SPACE + 1);
                    memmove(&lines[cursor_y][cursor_x + TAB_SPACE], &lines[cursor_y][cursor_x], len - cursor_x + 1);
                    for (int i = 0; i < TAB_SPACE; i++) {
                        lines[cursor_y][cursor_x + i] = ' ';
                    }
                    cursor_x += TAB_SPACE;
                }
                break;

            default:
                if (ch >= 32 && ch <= 126 || ch >= 128) { 
                    int len = strlen(lines[cursor_y]);
                    if (len + 1 < MAX_LEN) {
                        lines[cursor_y] = realloc(lines[cursor_y], len + 2);
                        memmove(&lines[cursor_y][cursor_x + 1], &lines[cursor_y][cursor_x], len - cursor_x + 1);
                        lines[cursor_y][cursor_x] = ch;
                        cursor_x++;
                    }
                }
                break;
        }
    }
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, ""); 
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    timeout(100);

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
