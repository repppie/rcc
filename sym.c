#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "rcc.h"

static struct symtab l0_symtab;
struct symtab *symtab = &l0_symtab;

#define	HASHSTEP(x, c) (((x << 5) + x) + (c))

static unsigned int
hash_str(char *s)
{
	unsigned int hash;

	hash = 0;
	while (*s)
		hash = HASHSTEP(hash, *s++);

	return (hash % SYMTAB_SIZE);
}

static struct symbol *
_find_sym(char *name, struct symtab *tab)
{
	struct symbol *s;
	unsigned int hash;

	hash = hash_str(name);
	while (tab) {
		for (s = tab->tab[hash]; s; s = s->next)
			if (!strcmp(s->name, name))
				return (s);
		tab = tab->prev;
	}
	return (NULL);
}

struct symbol *
find_sym(char *name)
{
	return (_find_sym(name, symtab));
}

struct symbol *
find_global_sym(char *name)
{
	return (_find_sym(name, &l0_symtab));
}

struct symbol *
add_sym(char *name, int size, int stacksize)
{
	struct symbol *s;
	unsigned int hash;

	hash = hash_str(name);
	for (s = symtab->tab[hash]; s; s = s->next)
		if (!strcmp(s->name, name))
			return (s);

	s = malloc(sizeof(struct symbol));
	memset(s, 0, sizeof(struct symbol));
	s->name = name;
	s->val = symtab->ar_offset;
	s->tab = symtab;
	s->typesize = size;
	s->stacksize = stacksize;
	symtab->ar_offset += stacksize;

	s->next = symtab->tab[hash];
	symtab->tab[hash] = s;

	return (s);
}

void
new_symtab(void)
{
	struct symtab *tab;

	tab = malloc(sizeof(struct symtab));
	memset(tab, 0, sizeof(struct symtab));
	tab->prev = symtab;
	tab->level = symtab->level + 1;
	if (tab->level != 1)
		tab->ar_offset = symtab->ar_offset;
	symtab = tab;
}

void
del_symtab(void)
{
	assert(symtab->level > 0);

	symtab = symtab->prev;
}
