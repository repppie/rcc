#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rcc.h"

#define	SYMTAB_SIZE 1021
static struct symbol *symtab[SYMTAB_SIZE];

#define	HASHSTEP(x, c) (((x << 5) + x) + (c))

static int
hash_str(char *s)
{
	int hash;

	hash = 0;
	while (*s)
		hash = HASHSTEP(hash, *s++);

	return (hash);
}

struct symbol *
find_sym(char *name)
{
	struct symbol *s;
	int hash;

	hash = hash_str(name);
	for (s = symtab[hash]; s; s = s->next)
		if (!strcmp(s->name, name))
			return (s);
	return (NULL);
}

struct symbol *
add_sym(char *name)
{
	struct symbol *s;
	int hash;

	hash = hash_str(name);
	for (s = symtab[hash]; s; s = s->next)
		if (!strcmp(s->name, name))
			return (s);

	s = malloc(sizeof(struct symbol));
	memset(s, 0, sizeof(struct symbol));
	s->name = name;

	s->next = symtab[hash];
	symtab[hash] = s;

	return (s);
}
