#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "rcc.h"

static struct node *expr(void);
static struct node *stmt(void);
static struct node *stmts(void);

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
maybe_match(enum tokens t)
{
	if (tok->tok == t) {
		next();
		return (1);
	}
	return (0);
}
		

static int
is_type(struct token *tok) {
	switch (tok->tok) {
	case TOK_CHAR:
	case TOK_SHORT:
	case TOK_INT:
	case TOK_LONG:
	case TOK_VOID:
		return (1);
	default:
		return (0);
	}
}

static int
type(void) {
	int t;

	t = tok->tok;
	if (is_type(tok))
		next();
	else
		errx(1, "Syntax error at line %d: Expected type got %d\n",
		    tok->line, tok->tok);
	return (t);
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
	if (s->func) {
		match('(');
		match(')');
		return (new_node(N_CALL, NULL, NULL, s));
	} else
		return (new_node(N_SYM, NULL, NULL, s));
}

static struct node *
primary_expr(void)
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
		return (n);
	default:
		errx(1, "Syntax error at line %d", tok->line);
	}
}

static struct node *
multiplicative_expr(void)
{
	struct node *l, *r;
	enum tokens t;

	l = primary_expr();

	while (tok->tok == '*' || tok->tok == '/') {
		t = tok->tok;
		next();
		r = primary_expr();
		l = new_node(t == '*' ? N_MUL : N_DIV, l, r, 0);
	}

	return (l);
}

static struct node *
additive_expr(void)
{
	struct node *l, *r;
	enum tokens t;

	l = multiplicative_expr();
	while (tok->tok == '+' || tok->tok == '-') {
		t = tok->tok;
		next();
		r = multiplicative_expr();
		l = new_node(t == '+' ? N_ADD : N_SUB, l, r, 0);
	}
	return (l);
}

static struct node *
equality_expr(void)
{
	struct node *l, *r;
	enum tokens t;

	l = additive_expr();
	while (tok->tok == TOK_EQ || tok->tok == TOK_NE) {
		t = tok->tok;
		next();
		r = additive_expr();
		l = new_node(t == TOK_EQ ? N_EQ : N_NE, l, r, 0);
	}
	return (l);
}

static struct node *
expr(void)
{
	return (equality_expr());
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

	match(';');

	n = head;
	if (head->next)
		n = new_node(N_MULTIPLE, head, NULL, 0);

	return (n);
}

static struct node *
assign(void)
{
	struct symbol *s;
	struct node *l, *r, *n;

	if (tok->tok != TOK_ID)
		errx(1, "Syntax error at line %d: Expected identifier,"
		    " got %d\n", tok->line, tok->tok);
	if ((s = find_sym(tok->str)) == NULL)
		errx(1, "'%s' undeclared at line %d\n", tok->str,
		    tok->line);
	next();
	match('=');
	l = new_node(N_SYM, NULL, NULL, s);
	r = expr();
	n = new_node(N_ASSIGN, l, r, 0);
	match(';');

	return (n);
}

static struct node *
ret(void)
{
	struct node *n;

	match(TOK_RETURN);
	n = expr();
	match(';');
	return (new_node(N_RETURN, n, NULL, 0));
}

static struct node *
_if(void)
{
	struct node *cond, *l, *r, *n;

	r = NULL;
	match(TOK_IF);
	match('(');
	cond = expr();
	match(')');
	l = stmts();
	if (tok->tok == TOK_ELSE) {
		match(TOK_ELSE);
		r = stmts();
	}
	n = new_node(N_IF, l, r, 0);
	n->cond = cond;
	return (n);
}

static struct node *
stmt(void)
{
	struct node *n;

	if (tok->tok == ';') {
		next();
		return NULL;
	}

	if (is_type(tok)) {
		n = decl();
	} else {
		if (tok->next->tok == '=')
			n = assign();
		else if (tok->tok == TOK_RETURN)
			n = ret();
		else if (tok->tok == TOK_IF)
			n = _if();
		else {
			n = expr();
			match(';');
		}
	}
	return (n);
}

static struct node *
compound_stmt(void)
{
	struct node *head, *last, *n;

	last = head = NULL;
	while (1) {
		n = stmt();
		if (!head)
			head = n;
		if (last)
			last->next = n;
		last = n;
		if (maybe_match('}'))
			break;
	}
	if (head->next)
		n = new_node(N_MULTIPLE, head, NULL, 0);
	return (n);
}

static struct node *
stmts()
{
	if (maybe_match('{'))
		return (compound_stmt());
	else
		return (stmt());
}

void
func(void) {
	struct symbol *s;
	struct node *n;
	int _type;

	_type = type();

	if (tok->tok != TOK_ID)
		errx(1, "Syntax error at line %d, Expected symbol, got %d",
		    tok->line, tok->tok);
	if ((find_sym(tok->str)) != NULL)
		errx(1, "'%s' redeclared at line %d", tok->str,
		    tok->line);
	s = add_sym(tok->str);
	s->func = 1;
	next();

	new_symtab();
	match('(');
	match(')');

	match('{');
	n = compound_stmt();
	del_symtab();

	s->body = n;
}

void
parse(void)
{
	while (tok->tok != TOK_EOF)
		func();
}
