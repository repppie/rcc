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

	while (tok) {
		if (tok->tok < 0x80)
			printf("%d: %c v %ld\n", tok->line, tok->tok, tok->val);
		else if (tok->tok == TOK_ID)
			printf("%d: %s\n", tok->line, tok->str);
		else if (tok->tok == TOK_STRING)
			printf("%d: str %s\n", tok->line, tok->str);
		else
			printf("%d: %d v %ld\n", tok->line, tok->tok, tok->val);
		tok = tok->next;
	}

	return (0);
}
