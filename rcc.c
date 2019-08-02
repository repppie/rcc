#include <stdio.h>
#include <fcntl.h>
#include <err.h>

#include "rcc.h"

int
main(int argc, char **argv)
{
	FILE *f;

	if (argc != 2)
		errx(1, "Usage: %s <file>", argv[0]);

	if ((f = fopen(argv[1], "r")) == NULL)
		err(1, "fopen");

	lex(f);

	parse();

	return (0);
}
