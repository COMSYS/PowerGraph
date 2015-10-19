#include "common.h"
#include "debug.h"

#include <linux/vmalloc.h>

// print directly to main HMDI screen
void print_on_screen(const char* msg)
{
	struct console *c = console_drivers;
	int msg_len = strlen(msg);
	while(c) {
		if ((c->flags & CON_ENABLED) && c->write)
			c->write(c, msg, msg_len);
		c = c->next;
	}
}

int printf_on_screen(char *format, ...)
{
	va_list  args;
	char *buf;
//	buf = (char *) kmalloc(sizeof(char) * strlen(format) + 32, GFP_KERNEL | __GFP_WAIT | __GFP_REPEAT);
	buf = (char *) vmalloc(sizeof(char) * strlen(format) + 32);
	if(buf == NULL)
	{
		print_on_screen(format);
		return -1;
	}
	va_start(args, format);
	vsprintf(buf, format, args);
	print_on_screen(buf);
	vfree(buf);
	va_end(args);
	return 0;
}

