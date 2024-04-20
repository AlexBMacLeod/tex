#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>

struct editorConfig
{
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

#define ABUF_INIT {NULL, 0}

#define CTRL_KEY(k) ((k) & 0x1f)

void ab_append(struct abuf *, const char *, int);

void ab_free(struct abuf *);

void disable_raw_mode(void);

void editor_draw_rows(struct abuf *);
//Draws # down the side of the terminal upon program start

char editor_read_key(void);

void editor_refresh_screen(void);

void editor_process_keypress(void);

void enable_raw_mode(void);

void exit_with_error(const char *);

int get_window_size(int *, int *);

void init_editor(void);

int get_cursor_position(int *, int *);
//Gets the current position of the cursor on terminal screen. Returns -1 for failure.

void refresh_screen(void);




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

char editor_read_key(void)
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if(nread == -1 && errno != EAGAIN) exit_with_error("read");
    }
    return c;
}

void editor_process_keypress(void)
{
    char c = editor_read_key();

    switch(c)
    {
        case CTRL_KEY('q'):
            refresh_screen();
            exit(0);
            break;
    }
}

void refresh_screen(void)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void editor_refresh_screen(void)
{
    struct abuf ab = ABUF_INIT;

    ab_append(&ab, "\x1b[?25l", 6);
    ab_append(&ab, "\x1b[2J", 4);
    ab_append(&ab, "\x1b[H", 3);

    editor_draw_rows(&ab);

    ab_append(&ab, "\x1b[H", 3);
    ab_append(&ab, "\x1b[?25h", 6);


    write(STDOUT_FILENO, ab.b, ab.len);
    ab_free(&ab);
}

void editor_draw_rows(struct abuf *ab)
{
    for(int y=0; y<E.screenrows; y++)
    {
        ab_append(ab, "#", 1);

        if(y<E.screenrows - 1) 
            ab_append(ab, "\r\n", 2);
    }
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
    if(get_window_size(&E.screenrows, &E.screencols)==-1) 
        exit_with_error("get_window_size");
}

int get_cursor_position(int *rows, int *cols)
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