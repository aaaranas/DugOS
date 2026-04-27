#include "vga.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEM    ((volatile uint16_t *) 0xB8000)

static size_t  row;
static size_t  col;
static uint8_t color;

static uint16_t make_entry(char c, uint8_t attr)
{
    return (uint16_t) (unsigned char) c | ((uint16_t) attr << 8);
}

void vga_set_color(uint8_t fg, uint8_t bg)
{
    color = (uint8_t) (fg | (bg << 4));
}

void vga_clear(void)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = make_entry(' ', color);
    row = 0;
    col = 0;
}

void vga_init(void)
{
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

static void scroll_if_needed(void)
{
    if (row < VGA_HEIGHT) return;

    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[(y - 1) * VGA_WIDTH + x] = VGA_MEM[y * VGA_WIDTH + x];

    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEM[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = make_entry(' ', color);

    row = VGA_HEIGHT - 1;
}

void vga_putchar(char c)
{
    if (c == '\n') {
        col = 0;
        row++;
        scroll_if_needed();
        return;
    }
    VGA_MEM[row * VGA_WIDTH + col] = make_entry(c, color);
    col++;
    if (col >= VGA_WIDTH) {
        col = 0;
        row++;
        scroll_if_needed();
    }
}

void vga_write(const char *s)
{
    while (*s) vga_putchar(*s++);
}

void vga_writeln(const char *s)
{
    vga_write(s);
    vga_putchar('\n');
}
