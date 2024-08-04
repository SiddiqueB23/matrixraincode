/*** includes ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#define TINYOBJ_LOADER_C_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/
#define _OPEN_SYS_ITOA_EXT
#define CTRL_KEY(k) ((k) & 0x1f)

enum Key {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN
};

struct Vec3f {
    float x, y, z;
};
struct Vec2i {
    int x, y;
};
struct Vec3f white = {1.0, 1.0, 1.0};
struct Vec3f black = {0.0, 0.0, 0.0};

/*** data ***/
struct Config {
    int screenrows;
    int screencols;
    float aspectRatio;
    struct termios orig_termios;
};
struct Config E;

struct Vec3f displayColorBuffer[1080][1920];
char displayTextBuffer[1920 * 1080 * 41];
float depthBuffer[1080 * 1920];
char *tb;
int tblen;

char digitStrings[256][3];

/*** utils ***/
void swap(int *a, int *b) {
    int t = *a;
    *a = *b;
    *b = t;
}

int max(int a, int b) {
    return (a > b) ? a : b;
}
int min(int a, int b) {
    return (a < b) ? a : b;
}

struct Vec3f cross(struct Vec3f a, struct Vec3f b) {
    struct Vec3f result = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    return result;
}

float dot(struct Vec3f a, struct Vec3f b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

void setPixel(int x, int y, struct Vec3f *color) {
    if (x >= 0 && x < E.screencols && y >= 0 && y < E.screenrows * 2)
        displayColorBuffer[y][x] = *color;
}

void perspectiveVec3f(struct Vec3f *p, float camDistance) {
    float factor = 1.0 / (1.0 - p->z / camDistance);
    p->x *= factor;
    p->y *= factor;
    p->z *= factor;
}
void perspective3f(float *x, float *y, float *z, float camDistance) {
    float factor = 1.0 / (1.0 - *z / camDistance);
    *x *= factor;
    *y *= factor;
    *z *= factor;
}

/*** terminal ***/
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[0m", 4);
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    write(STDOUT_FILENO, "\x1b[?25h", 6);
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");
    atexit(disableRawMode);
    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

int readKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';
        if (seq[0] == '[') {
            switch (seq[1]) {
            case 'A':
                return ARROW_UP;
            case 'B':
                return ARROW_DOWN;
            case 'C':
                return ARROW_RIGHT;
            case 'D':
                return ARROW_LEFT;
            }
        }
        return '\x1b';
    } else {
        return c;
    }
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** output ***/
void writeTextBuffer(void *src, size_t count) {
    memcpy(tb, src, count);
    tblen += count;
    tb += count;
}

void writeNum(__uint8_t x) {
    writeTextBuffer(digitStrings[x], x >= (__uint8_t)100 ? 3 : (x >= (__uint8_t)10 ? 2 : 1));
}

void render() {
    for (int i = 0; i < 2 * E.screenrows; i++) {
        for (int j = 0; j < E.screencols; j++) {
            float x = (float)j / (float)E.screencols;
            float y = (float)i / (float)(2 * E.screenrows);
            displayColorBuffer[i][j].x = x;
            displayColorBuffer[i][j].y = y;
        }
    }
}

void clearColor(struct Vec3f color) {
    for (int i = 0; i < 2 * E.screenrows; i++) {
        for (int j = 0; j < E.screencols; j++) {
            displayColorBuffer[i][j] = color;
        }
    }
}

void refreshScreen() {
    tb = displayTextBuffer;
    tblen = 0;
    writeTextBuffer("\x1b[?25l", 6);
    writeTextBuffer("\x1b[2J", 4);
    writeTextBuffer("\x1b[H", 3);
    for (int i = 0; i < E.screenrows; i++) {
        for (int j = 0; j < E.screencols; j++) {
            writeTextBuffer("\x1b[38;2;", 7);
            writeNum((__uint8_t)(displayColorBuffer[2 * i][j].x * 255.0f));
            writeTextBuffer(";", 1);
            writeNum((__uint8_t)(displayColorBuffer[2 * i][j].y * 255.0f));
            writeTextBuffer(";", 1);
            writeNum((__uint8_t)(displayColorBuffer[2 * i][j].z * 255.0f));
            writeTextBuffer("m", 1);
            writeTextBuffer("\x1b[48;2;", 7);
            writeNum((__uint8_t)(displayColorBuffer[2 * i + 1][j].x * 255.0f));
            writeTextBuffer(";", 1);
            writeNum((__uint8_t)(displayColorBuffer[2 * i + 1][j].y * 255.0f));
            writeTextBuffer(";", 1);
            writeNum((__uint8_t)(displayColorBuffer[2 * i + 1][j].z * 255.0f));
            writeTextBuffer("m", 1);
            writeTextBuffer("\u2580", 3);
        }
    }
    write(STDOUT_FILENO, displayTextBuffer, tblen);
}

/*** input ***/
void processKeypress() {
    char c = readKey();
    switch (c) {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[0m", 4);
        write(STDOUT_FILENO, "\x1b[?25h", 6);
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    }
}

/*** init ***/
void init() {
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
    E.aspectRatio = (float)E.screencols / (float)E.screenrows;
}

char *itoa10(int num, char *buffer) {
    if (num == 0) {
        buffer[0] = '0';
        return buffer;
    }
    int i = num >= 100 ? 2 : (num >= 10 ? 1 : 0);
    while (i >= 0) {
        buffer[i] = (num % 10) + '0';
        num /= 10;
        i--;
    }
    return buffer;
}

void initDigitStrings() {
    for (int i = 0; i < 256; i++) {
        itoa10(i, digitStrings[i]);
    }
}

static char *mmap_file(size_t *len, const char *filename) {

    struct stat sb;
    char *p;
    int fd;

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return NULL;
    }

    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        return NULL;
    }

    if (!S_ISREG(sb.st_mode)) {
        fprintf(stderr, "%s is not a file\n", filename);
        return NULL;
    }

    p = (char *)mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);

    if (p == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    if (close(fd) == -1) {
        perror("close");
        return NULL;
    }

    (*len) = sb.st_size;

    return p;
}

int main() {
    srandom(0);
    initDigitStrings();
    enableRawMode();
    init();
    clearColor(black);

    while (1) {
        
        refreshScreen();
        processKeypress();
    }

    return 0;
}