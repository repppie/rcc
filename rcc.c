#include <stdio.h>
#include <fcntl.h>
#include <err.h>

#include "rcc.h"

void
add_special_funcs(void)
{
	struct symbol *s;

	s = add_sym("print", 0);
	s->func = 1;
}

int
main(int argc, char **argv)
{
	FILE *f;

	if (argc != 2)
		errx(1, "Usage: %s <file>", argv[0]);

	if ((f = fopen(argv[1], "r")) == NULL)
		err(1, "fopen");

	lex(f);

	add_special_funcs();
	parse();
	gen_ir();
	emit_x86();

	return (0);
}
