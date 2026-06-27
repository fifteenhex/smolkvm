#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

static volatile uint8_t *rxd = (void *) 0x1000;

static int smolkvm_putc(char c, FILE *file)
{
	(void) file;

	*rxd = c;

	return c;
}

static int smolkvm_getc(FILE *file)
{
	unsigned char c;
	(void) file;		/* Not used in this function */
//	c = __uart_getc();	/* Defined by underlying system */
	return c;
}

static FILE __stdio = FDEV_SETUP_STREAM(smolkvm_putc,
					smolkvm_getc,
					NULL,
					_FDEV_SETUP_RW);

FILE *const stdin = &__stdio;
__strong_reference(stdin, stdout);
__strong_reference(stdin, stderr);
