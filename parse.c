#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "rcc.h"

static struct node *expr(void);
static struct node *stmt(void);
static struct node *stmts(void);

static struct node *
new_node(enum node_op op, struct node *l, struct node *r, void *v,
    struct type *_type)
{
	struct node *n;

	n = malloc(sizeof(struct node));
	memset(n, 0, sizeof(struct node));
	n->op = op;
	n->l = l;
	n->r = r;
	n->str = v;
	n->type = _type;

	return (n);
}

static void
next(void)
{
	if (tok->tok == TOK_EOF)
		errx(1, "Unexpected EOF at line %d\n", tok->line);
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

static struct type *
new_type(int size)
{
	struct type *t;

	t = malloc(sizeof(struct type));
	memset(t, 0, sizeof(struct type));
	t->size = size;
	t->stacksize = size;

	return (t);
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
typesize(struct token *tok) {
	switch (tok->tok) {
	case TOK_LONG:
		return (8);
	case TOK_CHAR:
	case TOK_SHORT:
	case TOK_INT:
		return (4);
	case TOK_VOID:
	default:
		return (0);
	}
}

static struct type *
type(void) {
	struct type *type, *ptr;

	type = new_type(typesize(tok));
	if (is_type(tok))
		next();
	else
		errx(1, "Syntax error at line %d: Expected type got %d\n",
		    tok->line, tok->tok);
	while (maybe_match('*')) {
		ptr = new_type(8);
		ptr->ptr = type;
		type = ptr;
	}

	return (type);
}

static struct node *
constant(void)
{
	struct type *_type;
	long v;

	v = tok->val;
	match(TOK_CONSTANT);
	_type = new_type(4);
	return (new_node(N_CONSTANT, NULL, NULL, (void *)v, _type));
}

static struct node *
symbol(void)
{
	struct symbol *s;

	if ((s = find_sym(tok->str)) == NULL)
		errx(1, "'%s' undeclared at line %d", tok->str,
		    tok->line);
	match(TOK_ID);
	return (new_node(N_SYM, NULL, NULL, s, s->type));
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

static struct param *
argument_expr_list(void)
{
	struct param *p, *p_head, *p_last;

	p_head = p_last = NULL;
	while (!maybe_match(')')) {
		p = malloc(sizeof(struct param));
		memset(p, 0, sizeof(struct param));
		p->n = expr();
		if (!p_head)
			p_head = p;
		if (p_last)
			p_last->next = p;
		p_last = p;
		if (!maybe_match(',')) {
			match(')');
			break;
		}
	}
	return (p_head);
}

static struct node *
postfix_expr(void)
{
	struct node *l, *n, *r;
	int array;

	n = l = primary_expr();
	array = 0;
	while (tok->tok == '(' || tok->tok == '[') {
		if (maybe_match('(')) {
			n = new_node(N_CALL, n, NULL, NULL, n->type);
			n->params = argument_expr_list();
		} else if (maybe_match('[')) {
			r = expr();
			n = new_node(N_ADD, n, r, NULL, l->type->ptr);
			array = 1;
			match(']');
		}
	}
	if (array)
		n = new_node(N_DEREF, n, 0, NULL, n->type);
	return (n);
}

static struct node *
unary_expr(void)
{
	struct node *n;
	struct type *_type;

	if (maybe_match('*')) {
		n = unary_expr();
		if (!n->type->ptr)
			errx(1, "Deferencing something that is not a pointer "
			    "at line %d\n", tok->line);
		return (new_node(N_DEREF, n, NULL, 0, n->type->ptr));
	} else if (maybe_match('&')) {
		n = unary_expr();
		_type = new_type(8);
		_type->ptr = n->type;
		return (new_node(N_ADDR, n, NULL, 0, _type));
	}
	return (postfix_expr());
}

static struct node *
multiplicative_expr(void)
{
	struct node *l, *r;
	enum tokens t;

	l = unary_expr();
	while (tok->tok == '*' || tok->tok == '/') {
		t = tok->tok;
		next();
		r = unary_expr();
		l = new_node(t == '*' ? N_MUL : N_DIV, l, r, 0, l->type);
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
		if (l->type->ptr && r->type->ptr)
			errx(1, "Can't add two pointers at line %d", tok->line);
		l = new_node(t == '+' ? N_ADD : N_SUB, l, r, 0, l->type);
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
		l = new_node(t == TOK_EQ ? N_EQ : N_NE, l, r, 0, l->type);
	}
	return (l);
}

static struct node *
assign_expr(void)
{
	struct node *l, *r, *n;

	l = equality_expr();
	if (maybe_match('=')) {
		r = expr();
		n = new_node(N_ASSIGN, l, r, 0, l->type);
		return (n);
	} else
		return (l);

}

static struct node *
expr(void)
{
	return (assign_expr());
}

static struct type *
array(struct type *t)
{
	struct node *n, *last;
	struct type *_type;

	n = last = NULL;
	_type = t;
	while (maybe_match('[')) {
		n = primary_expr();
		if (n->op != N_CONSTANT)
			errx(1, "Array size needs to be constant at line %d",
			    tok->line);
		n->next = last;
		last = n;
		match(']');
	}
	while (n) {
		_type = new_type(n->val);
		_type->stacksize =  n->val * t->stacksize;
		_type->ptr = t;
		_type->array = 1;
		t = _type;
		n = n->next;
	}
	return (t);
}

static struct node *
_decl(struct type *__type)
{
	struct node *l, *last, *head, *n, *r;
	struct symbol *s;
	struct type *_type, *ptr;
	char *name;

	last = head = NULL;
	while (1) {
		n = NULL;
		_type = __type;
		while (maybe_match('*')) {
			ptr = new_type(8);
			ptr->ptr = _type;
			_type = ptr;
		}
		if (tok->tok != TOK_ID)
			errx(1, "Syntax error at line %d: Expected identifier,"
			    " got %d\n", tok->line, tok->tok);
		name = tok->str;
		next();

		if (tok->tok == '[')
			_type = array(_type);

		if (find_sym(name) != NULL)
			errx(1, "Redeclaring '%s' at line %d\n", tok->str,
			    tok->line);
		s = add_sym(name, _type);
	
		if (tok->tok == '=') {
			next();
			l = new_node(N_SYM, NULL, NULL, s, _type);
			r = expr();
			n = new_node(N_ASSIGN, l, r, 0, _type);
			if (!head)
				head = n;
			if (last)
				last->next = n;
			if (n)
				last = n;
		}
		if (tok->tok != ',')
			break;
		next();
	}

	match(';');

	n = head;
	if (head && head->next)
		n = new_node(N_MULTIPLE, head, NULL, 0, NULL);

	return (n);
}

static struct node *
ret(void)
{
	struct node *n;

	n = NULL;
	match(TOK_RETURN);
	if (maybe_match(';'))
		goto out;
	n = expr();
	match(';');
out:
	return (new_node(N_RETURN, n, NULL, 0, n->type));
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
	n = new_node(N_IF, l, r, 0, NULL);
	n->cond = cond;
	return (n);
}

static struct node *
_while(void)
{
	struct node *cond, *l, *n;

	match(TOK_WHILE);
	match('(');
	cond = expr();
	match(')');
	l = stmts();
	n = new_node(N_WHILE, l, 0, 0, NULL);
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

	if (is_type(tok))
		n = _decl(type());
	else {
		if (tok->tok == TOK_RETURN)
			n = ret();
		else if (tok->tok == TOK_IF)
			n = _if();
		else if (tok->tok == TOK_WHILE)
			n = _while();
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
		if (n)
			last = n;
		if (maybe_match('}'))
			break;
	}
	if (head->next)
		head = new_node(N_MULTIPLE, head, NULL, 0, NULL);
	return (head);
}

static struct node *
stmts()
{
	if (maybe_match('{'))
		return (compound_stmt());
	else
		return (stmt());
}

static void
func(struct type *_type)
{
	struct symbol *s;
	struct param *p, *head_p, *last_p;
	struct node *n;

	if ((find_sym(tok->str)) != NULL)
		errx(1, "'%s' redeclared at line %d", tok->str,
		    tok->line);
	s = add_sym(tok->str, _type);
	next();
	new_symtab();
	s->func = 1;
	s->tab = symtab;
	match('(');

	head_p = last_p = NULL;
	while (!maybe_match(')')) {
		p = malloc(sizeof(struct param));
		memset(p, 0, sizeof(struct param));
		if (!head_p)
			head_p = p;
		if (last_p)
			last_p->next = p;
		last_p = p;

		_type = type();
		if (tok->tok != TOK_ID)
			errx(1, "Syntax error at line %d, Expected symbol, got"
			    " %d", tok->line, tok->tok);
		if ((find_sym(tok->str)) != NULL)
			errx(1, "'%s' redeclared at line %d", tok->str,
			    tok->line);
		p->sym = add_sym(tok->str, _type);
		next();
		if (!maybe_match(',')) {
			match(')');
			break;
		}
	}

	match('{');
	n = compound_stmt();
	del_symtab();

	s->body = n;
	s->params = head_p;
}

static void
external_decl(void)
{
	struct type *_type;

	_type = type();

	if (tok->tok != TOK_ID)
		errx(1, "Syntax error at line %d, Expected symbol, got %d",
		    tok->line, tok->tok);

	if (tok->next->tok == '(')
		func(_type);
	else
		_decl(_type);
}

void
parse(void)
{
	while (tok->tok != TOK_EOF)
		external_decl();
}
