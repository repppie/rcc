#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "rcc.h"

struct set *
new_set(int n)
{
	struct set *s;

	s = malloc(sizeof(struct set));
	s->bits = malloc(n / 8 + 1);
	memset(s->bits, 0, n / 8 + 1);
	s->n = n;

	return (s);
}

int
set_set(struct set *s, int n)
{
	assert(n < s->n);
	return (s->bits[n / 8] & (1 << n % 8));
}

void
set_add(struct set *s, int n)
{
	assert(n < s->n);
	s->bits[n / 8] |= 1 << n % 8;
}

void
set_del(struct set *s, int n)
{
	assert(n < s->n);
	s->bits[n / 8] &= ~(1 << n % 8);
}
