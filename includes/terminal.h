#ifndef TERMINAL_H
#define TERMINAL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// VGA-related definitions
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

typedef struct {
    size_t row;
    size_t column;
    uint8_t color;
    uint16_t* buffer;
} Terminal;

extern Terminal terminal;

void terminal_initialize(void);
void terminal_clear(void);
void terminal_setcolor(uint8_t color);
void terminal_putchar(char c);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);
void terminal_writehex(uint8_t num);
void move_cursor(size_t x, size_t y);
void update_cursor(void);
void switch_screen(uint8_t screen);

#endif // TERMINAL_H

