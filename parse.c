#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "rcc.h"

static struct node *expr(void);

static struct node *
new_node(enum node_op op, struct node *l, struct node *r, void *v)
{
	struct node *n;

	n = malloc(sizeof(struct node));
	memset(n, 0, sizeof(struct node));
	n->op = op;
	n->l = l;
	n->r = r;
	n->str = v;

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

static int
is_type(struct token *tok) {
	switch (tok->tok) {
	case TOK_CHAR:
	case TOK_SHORT:
	case TOK_INT:
	case TOK_LONG:
		return (1);
	default:
		return (0);
	}
}

static struct node *
constant(void)
{
	long v;

	v = tok->val;
	match(TOK_CONSTANT);
	return (new_node(N_CONSTANT, NULL, NULL, (void *)v));
}

static struct node *
symbol(void)
{
	struct symbol *s;

	if ((s = find_sym(tok->str)) == NULL)
		errx(1, "'%s' undeclared at line %d", tok->str,
		    tok->line);
	match(TOK_ID);
	return (new_node(N_SYM, NULL, NULL, s));
}

static struct node *
factor(void)
{
	struct node *n;

	switch (tok->tok) {
	case TOK_CONSTANT:
		return constant();
	case TOK_ID:
		return symbol();
	case '(':
		next();
		n = expr();
		match(')');
		return n;
	default:
		errx(1, "Syntax error at line %d", tok->line);
	}
}

static struct node *
term(void)
{
	struct node *l, *r;
	enum tokens t;

	l = factor();

	while (tok->tok == '*' || tok->tok == '/') {
		t = tok->tok;
		next();
		r = factor();
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

static struct node *
decl(void)
{
	struct node *l, *last, *head, *n, *r;
	struct symbol *s;

	next();

	last = head = NULL;
	while (1) {
		n = NULL;
		if (tok->tok != TOK_ID)
			errx(1, "Syntax error at line %d: Expected identifier,"
			    " got %d\n", tok->line, tok->tok);
		if (find_sym(tok->str) != NULL)
			errx(1, "Redeclaring '%s' at line %d\n", tok->str,
			    tok->line);
		s = add_sym(tok->str);
	
		next();
		if (tok->tok == '=') {
			next();
			l = new_node(N_SYM, NULL, NULL, s);
			r = expr();
			n = new_node(N_ASSIGN, l, r, 0);
			if (!head)
				head = n;
			if (last)
				last->next = n;
			last = n;
		}
		if (tok->tok != ',')
			break;
		next();
	}

	if (head->next)
		n = new_node(N_MULTIPLE, head, NULL, 0);

	return (n);
}

static void
stmt(void)
{
	struct node *n;

	if (tok->tok == ';') {
		next();
		return;
	}

	if (is_type(tok)) {
		n = decl();
		gen_ir(n);
	} else {
		n = expr();
		gen_ir_ret(n);
	}
}

void
parse(void)
{

	while (tok->tok != TOK_EOF)
		stmt();

	emit_x86();
}
