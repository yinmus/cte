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
int cmd_mode = 0;
char cmd_buf[MAX_LEN] = {0};
char filename[256] = "untitled.txt";
int cx = 0, cy = 0, scroll_offset = 0;
int num_lines = 0;
char *lines[MAX_LINES];

void load(const char *fname) {
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

void save() {
    FILE *fp = fopen(filename, "w");
    if (!fp) return;
    for (int i = 0; i < num_lines; i++) {
        if (lines[i]) fprintf(fp, "%s\n", lines[i]);
    }
    fclose(fp);
}

void draw() {
    clear();

    int num_width = snprintf(NULL, 0, "%d", num_lines) + 2;
    int screen_lines = LINES - 3;

    if (cy >= scroll_offset + screen_lines) scroll_offset = cy - screen_lines + 1;
    if (cy < scroll_offset) scroll_offset = cy;

    for (int i = 0; i < screen_lines && i + scroll_offset < num_lines; i++) {
        if (lines[i + scroll_offset]) mvprintw(i, 0, "%*d| %s", num_width - 2, i + scroll_offset + 1, lines[i + scroll_offset]);
    }

    attron(A_BOLD);
    mvhline(LINES - 3, 0, ACS_HLINE, COLS);
    attroff(A_BOLD);

    mvprintw(LINES - 2, 0, " [ %s ] %d/%d lines ", filename, cy + 1, num_lines);

    if (cmd_mode) {
        attron(A_BOLD);
        mvhline(LINES - 1, 0, ACS_HLINE, COLS);
        attroff(A_BOLD);
        mvprintw(LINES - 1, 0, ":%s ", cmd_buf);
    }

    int cursor_pos = 0;
    int byte_pos = 0;
    while (byte_pos < cx) {
        int char_len = mblen(&lines[cy][byte_pos], MB_CUR_MAX);
        if (char_len <= 0) break;
        cursor_pos++;
        byte_pos += char_len;
    }

    move(cy - scroll_offset, cursor_pos + num_width);
}

void del_word() {
    if (cx == 0) {
        if (cy > 0) {
            cy--;
            cx = strlen(lines[cy]);
            lines[cy] = realloc(lines[cy], strlen(lines[cy]) + strlen(lines[cy + 1]) + 1);
            strcat(lines[cy], lines[cy + 1]);
            free(lines[cy + 1]);
            for (int i = cy + 1; i < num_lines - 1; i++) lines[i] = lines[i + 1];
            lines[num_lines - 1] = NULL;
            num_lines--;
        }
    } else {
        int i = cx - 1;
        while (i >= 0 && isspace(lines[cy][i])) i--;
        while (i >= 0 && !isspace(lines[cy][i])) i--;
        int new_x = i + 1;
        memmove(&lines[cy][new_x], &lines[cy][cx], strlen(&lines[cy][cx]) + 1);
        cx = new_x;
    }
}

void handle(int ch) {
    static int alt = 0;

    if (alt) {
        alt = 0;
        if (ch == 'c') {
            cmd_mode = 1;
            memset(cmd_buf, 0, MAX_LEN);
        } else if (ch == 'x') {
            if (num_lines > 1) {
                free(lines[cy]);
                for (int i = cy; i < num_lines - 1; i++) lines[i] = lines[i + 1];
                lines[num_lines - 1] = NULL;
                num_lines--;
                if (cy >= num_lines) cy = num_lines - 1;
            }
        } else if (ch == 127 || ch == KEY_BACKSPACE) del_word();
        return;
    }

    if (ch == 27) {
        alt = 1;
        return;
    }

    if (cmd_mode) {
        if (ch == '\n') {
            if (strcmp(cmd_buf, "q") == 0) exit(0);
            if (strcmp(cmd_buf, "w") == 0) save();
            if (strcmp(cmd_buf, "wq") == 0) {
                save();
                exit(0);
            }
            memset(cmd_buf, 0, MAX_LEN);
            cmd_mode = 0;
        } else if (ch == 27) cmd_mode = 0;
        else if ((ch == 127 || ch == KEY_BACKSPACE) && strlen(cmd_buf) > 0) cmd_buf[strlen(cmd_buf) - 1] = '\0';
        else if (isprint(ch) && ch != ' ') {
            int len = strlen(cmd_buf);
            if (len < MAX_LEN - 1) {
                cmd_buf[len] = ch;
                cmd_buf[len + 1] = '\0';
            }
        }
    } else {
        switch (ch) {
            case KEY_BACKSPACE:
            case 127:
                if (cx > 0) {
                    int char_len = mblen(&lines[cy][cx - 1], MB_CUR_MAX);
                    if (char_len <= 0) char_len = 1;
                    memmove(&lines[cy][cx - char_len], &lines[cy][cx], strlen(&lines[cy][cx]) + 1);
                    cx -= char_len;
                } else if (cy > 0) {
                    char *temp = lines[cy];
                    lines[cy - 1] = realloc(lines[cy - 1], strlen(lines[cy - 1]) + strlen(temp) + 1);
                    strcat(lines[cy - 1], temp);
                    free(lines[cy]);
                    for (int i = cy; i < num_lines - 1; i++) lines[i] = lines[i + 1];
                    lines[num_lines - 1] = NULL;
                    num_lines--;
                    cy--;
                    cx = strlen(lines[cy]);
                }
                break;

            case KEY_DC:
                if (cx < strlen(lines[cy])) {
                    int char_len = mblen(&lines[cy][cx], MB_CUR_MAX);
                    if (char_len <= 0) char_len = 1;
                    memmove(&lines[cy][cx], &lines[cy][cx + char_len], strlen(&lines[cy][cx + char_len]) + 1);
                }
                break;

            case '\n':
                if (num_lines < MAX_LINES) {
                    if (cx < strlen(lines[cy])) {
                        memmove(&lines[cy + 1], &lines[cy], (num_lines - cy) * sizeof(char *));
                        lines[cy + 1] = strdup(&lines[cy][cx]);
                        lines[cy][cx] = '\0';
                    } else {
                        memmove(&lines[cy + 1], &lines[cy], (num_lines - cy) * sizeof(char *));
                        lines[cy + 1] = strdup("");
                    }
                    num_lines++;
                    cy++;
                    cx = 0;
                }
                break;

            case KEY_UP:
                if (cy > 0) cy--;
                break;

            case KEY_DOWN:
                if (cy < num_lines - 1) cy++;
                break;

            case KEY_LEFT:
                if (cx > 0) {
                    int char_len = mblen(&lines[cy][cx - 1], MB_CUR_MAX);
                    if (char_len <= 0) char_len = 1;
                    cx -= char_len;
                } else if (cy > 0) {
                    cy--;
                    cx = strlen(lines[cy]);
                }
                break;

            case KEY_RIGHT:
                if (cx < strlen(lines[cy])) {
                    int char_len = mblen(&lines[cy][cx], MB_CUR_MAX);
                    if (char_len <= 0) char_len = 1;
                    cx += char_len;
                } else if (cy < num_lines - 1) {
                    cy++;
                    cx = 0;
                }
                break;

            case '\t':
                int len = strlen(lines[cy]);
                if (len + TAB_SPACE < MAX_LEN) {
                    lines[cy] = realloc(lines[cy], len + TAB_SPACE + 1);
                    memmove(&lines[cy][cx + TAB_SPACE], &lines[cy][cx], len - cx + 1);
                    for (int i = 0; i < TAB_SPACE; i++) lines[cy][cx + i] = ' ';
                    cx += TAB_SPACE;
                }
                break;

            default:
                if (ch >= 32 && ch <= 126 || ch >= 128) {
                    int len = strlen(lines[cy]);
                    if (len + 1 < MAX_LEN) {
                        lines[cy] = realloc(lines[cy], len + 2);
                        memmove(&lines[cy][cx + 1], &lines[cy][cx], len - cx + 1);
                        lines[cy][cx] = ch;
                        cx++;
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

    if (argc > 1) load(argv[1]);
    else load(filename);

    draw();

    while (1) {
        int ch = getch();
        handle(ch);
        draw();
    }

    endwin();
    return 0;
}
