#ifndef __DEBUG__
#define __DEBUG__

// print directly to the main HMDI screen
void print_on_screen(const char* msg);

// print directly to the main HMDI screen, allows formatting
int printf_on_screen(char *format, ...);

#endif