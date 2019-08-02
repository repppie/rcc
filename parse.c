#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "rcc.h"

static struct node *
new_node(enum node_op op, struct node *l, struct node *r, int v)
{
	struct node *n;

	n = malloc(sizeof(struct node));
	memset(n, 0, sizeof(struct node));
	n->op = op;
	n->l = l;
	n->r = r;
	n->val = v;

	return (n);
}

static void
next(void)
{
	tok = tok->next;
}

static void
match(enum tokens t)
{
	if (tok->tok != t)
		errx(1, "Syntax error at line %d: Expected '%d' got '%d'\n",
		    tok->line, t, tok->tok);
	next();
}

static struct node *
constant(void)
{
	long v;

	v = tok->val;
	match(TOK_CONSTANT);
	return (new_node(N_CONSTANT, NULL, NULL, v));
}

static struct node *
term(void)
{
	struct node *l, *r;
	enum tokens t;

	l = constant();

	while (tok->tok == '*' || tok->tok == '/') {
		t = tok->tok;
		next();
		r = constant();
		l = new_node(t == '*' ? N_MUL : N_DIV, l, r, 0);
	}

	return (l);
}

static struct node *
expr(void)
{
	struct node *l, *r;
	enum tokens t;

	l = term();

	while (tok->tok == '+' || tok->tok == '-') {
		t = tok->tok;
		next();
		r = term();
		l = new_node(t == '+' ? N_ADD : N_SUB, l, r, 0);
	}

	return (l);
}

static void
stmt(void)
{
	struct node *n;

	if (tok->tok == ';') {
		next();
		return;
	}

	n = expr();
	gen_ir(n);
	dump_ir();
}

void
parse(void)
{

	while (tok->tok != TOK_EOF)
		stmt();
}
