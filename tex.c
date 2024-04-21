#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>

enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN
};

struct editorConfig
{
    int cx, cy;
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

struct abuf
{
    char *b;
    int len;
};

void ab_append(struct abuf *, const char *, int);
void ab_free(struct abuf *);
void disable_raw_mode(void);
void editor_draw_rows(struct abuf *);
void editor_move_cursor(int);
int editor_read_key(void);
void editor_refresh_screen(void);
void editor_process_keypress(void);
void enable_raw_mode(void);
void exit_with_error(const char *);
int get_window_size(int *, int *);
void init_editor(void);
int get_cursor_position(int *, int *);
void refresh_screen(void);

#define TEX_VERSION "0.0.1"
#define ABUF_INIT {NULL, 0}
#define CTRL_KEY(k) ((k) & 0x1f)


int main(void){
    enable_raw_mode();
    init_editor();

    while (1)
    {
        editor_refresh_screen();
        editor_process_keypress();
    }

    return 0;
}

void ab_append(struct abuf *ab, const char *s, int len)
{
    char *new = realloc(ab->b, ab->len + len);

    if(new==NULL)
        return;
    
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void ab_free(struct abuf *ab)
{
    free(ab->b);
}

void disable_raw_mode(void)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        exit_with_error("tcsetattr");
}

int get_cursor_position(int *rows, int *cols)
//Gets the current position of the cursor on terminal screen. Returns -1 for failure.
{
    char buf[32];
    unsigned int i = 0;

    if(write(STDOUT_FILENO, "\x1b[6n", 4)!=4) return -1;

    while(i < sizeof(buf) - 1)
    {
        if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if(buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if(buf[0] != '\x1b' || buf[1] != '[') return -1;
    if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int get_window_size(int *rows, int *cols)
{
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)==-1 || ws.ws_col==0)
    {
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) 
            return -1;

        return get_cursor_position(rows, cols);
    }else
    {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

void init_editor(void)
{
    E.cx = 0;
    E.cy = 0;

    if(get_window_size(&E.screenrows, &E.screencols)==-1) 
        exit_with_error("get_window_size");
}

void editor_draw_rows(struct abuf *ab)
//Draws initial start placing # down the side and a welcome screen with editor's name
{
    for(int y=0; y<E.screenrows; y++)
    {
        if(y==E.screenrows/3)
        {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                "Tex editor -- version %s", TEX_VERSION);
            if(welcomelen>E.screencols)
                welcomelen=E.screencols;
            int padding = (E.screencols - welcomelen) / 2;
            if(padding)
            {
                ab_append(ab, "#", 1);
                padding--;
            }
            while(padding--) ab_append(ab, " ", 1);
            ab_append(ab, welcome, welcomelen);
        }else{
            ab_append(ab, "#", 1);
        }

        ab_append(ab, "\x1b[K", 3);

        if(y<E.screenrows - 1) 
            ab_append(ab, "\r\n", 2);
    }
}

void editor_move_cursor(int const key)
{
    switch(key)
    {
        case ARROW_LEFT:
            if(E.cx != 0)
                E.cx--;
            break;
        case ARROW_RIGHT:
            if(E.cx != E.screencols - 1)
                E.cx++;
            break;
        case ARROW_UP:
            if(E.cy != 0)
                E.cy++;
            break;
        case ARROW_DOWN:
            if(E.cy != E.screenrows - 1)
                E.cy--;
            break;
    }
}

void editor_process_keypress(void)
{
    int c = editor_read_key();

    switch(c)
    {
        case CTRL_KEY('q'):
            refresh_screen();
            exit(0);
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editor_move_cursor(c);
            break;
    }
}

int editor_read_key(void)
//Read key input. If read fails exits with error.
//If an arrow key, returns arrow as wasd to move cursor.
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if(nread == -1 && errno != EAGAIN) exit_with_error("read");
    }

    if(c=='\x1b')
    {
        char seq[3];
        if(read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if(read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';

        if(seq[0] == '[')
        {
            if(seq[1]>='0' && seq[1] <= '9')
            {
                if(read(STDIN_FILENO, & seq[2], 1) != 1)
                    return '\x1b';
                if(seq[2] == '~')
                    switch(seq[1])
                    {
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                    }
            }else{
                switch(seq[1])
                {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                }
            }
        }
        return '\x1b';
    }else{
        return c;
    }
}

void editor_refresh_screen(void)
{
    struct abuf ab = ABUF_INIT;

    ab_append(&ab, "\x1b[?25l", 6);
    ab_append(&ab, "\x1b[H", 3);

    editor_draw_rows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx+1);
    ab_append(&ab, buf, strlen(buf));

    ab_append(&ab, "\x1b[?25h", 6);


    write(STDOUT_FILENO, ab.b, ab.len);
    ab_free(&ab);
}

void enable_raw_mode(void)
{
    if (tcgetattr(STDIN_FILENO, &E.orig_termios)==-1) exit_with_error("tcgetattr");
    atexit(disable_raw_mode);

    struct termios raw = E.orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_oflag &= ~(OPOST);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)==-1) exit_with_error("tcsetattr");

}

void exit_with_error(const char *s)
{
    fflush(stdout);
    refresh_screen();
    perror(s);

    FILE *logFile = fopen("error_log.txt", "a");
    if (logFile){
        fprintf(logFile, "Error: %s\n", s);
        fclose(logFile);
    }

    exit(1);
}

void refresh_screen(void)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}