#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>

struct termios orig_termios;

void enable_raw_mode(void);

void disable_raw_mode(void);

void exit_with_error(const char *, int);

int main(void){
    enable_raw_mode();

    while (1){
        char c = '\0';
        if (read(STDIN_FILENO, &c, 1)==-1 && errno != EAGAIN) exit_with_error('read', 1);
        if (iscntrl(c)) {
            printf("%d\n", c);
        } else {
            printf("%d ('%c)\n", c, c);
        }
        if (c == 'q') break;
    }

    return 0;
}

void disable_raw_mode(void){
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        exit_with_error("tcsetattr", 1);
}

void enable_raw_mode(void){
    if (tcgetattr(STDIN_FILENO, &orig_termios)==-1) exit_with_error("tcgetattr", 1);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_oflag &= ~(OPOST);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)==-1) exit_with_error("tcsetattr", 1);

}

void exit_with_error(const char *s, int exitStatus){
    fflush(stdout);

    perror(s);

    FILE *logFile = fopen("error_log.txt", "a");
    if (logFile){
        fprintf(logFile, "Error: %s\n", s);
        fclose(logFile);
    }

    exit(exitStatus);
}