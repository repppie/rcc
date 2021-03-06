#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "rcc.h"

static int break_lbl = -1;
static int cont_lbl = -1;

static struct type *type(void);
static struct node *expr(void);
static struct node *stmt(void);
static struct node *stmts(void);
static struct node *assign_expr(void);

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

struct type *
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
	case TOK_STRUCT:
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
	case TOK_SHORT:
	case TOK_INT:
		return (4);
	case TOK_CHAR:
		return (1);
	case TOK_VOID:
	default:
		return (0);
	}
}

static struct struct_field *
find_field(struct struct_field *head, char *name)
{
	struct struct_field *f;

	for (f = head; f; f = f->next)
		if (!strcmp(f->name, name))
			return (f);
	return (NULL);
}

static struct struct_field *
struct_field(int *off)
{
	struct struct_field *f, *head, *last;

	head = last = NULL;
	while (!maybe_match('}')) {
		f = malloc(sizeof(struct struct_field));
		memset(f, 0, sizeof(struct struct_field));

		f->type = type();
		if (tok->tok != TOK_ID)
			errx(1, "Syntax error: Expected id, got %d"
			    " at line %d\n", tok->tok, tok->line);
		f->name = tok->str;
		f->off = *off;
		*off += f->type->stacksize;
		if (find_field(head, f->name))
			errx(1, "Duplicate field %s at line %d\n", f->name,
			    tok->line);
		if (!head)
			head = f;
		if (last)
			last->next = f;
		last = f;
		next();
		match(';');
	}

	return (head);
}

static struct type *
_struct(void)
{
	struct _struct *_st;
	char *name;
	int off;

	name = NULL;
	_st = NULL;
	match(TOK_STRUCT);
	if (tok->tok == TOK_ID) {
		name = tok->str;
		next();
		_st = find_struct(name);
	}
	if (maybe_match('{')) {
		if (_st)
			errx(1, "Redefinition of struct %s at line %d", name,
			    tok->line);
		_st = add_struct(name);
		_st->name = name;
		_st->fields = struct_field(&off);
		_st->type = new_type(off);
		_st->type->_struct = _st;
	}
	if (!_st)
		errx(1, "Unknown struct %s at line %d", name, tok->line);
	return (_st->type);
}

static struct type *
type(void)
{
	struct type *_type, *ptr;

	if (!is_type(tok))
		errx(1, "Syntax error at line %d: Expected type got %d\n",
		    tok->line, tok->tok);

	if (tok->tok == TOK_STRUCT)
		_type = _struct();
	else {
		_type = new_type(typesize(tok));
		next();
	}

	while (maybe_match('*')) {
		ptr = new_type(8);
		ptr->ptr = _type;
		_type = ptr;
	}

	return (_type);
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
string(void)
{
	struct symbol *s;

	s = add_string(tok->str);
	match(TOK_STRING);
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
	case TOK_STRING:
		return string();
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
		p->n = assign_expr();
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
	struct struct_field *f;
	int array;

	n = l = primary_expr();
	array = 0;
	while (tok->tok == '(' || tok->tok == '[' || tok->tok == '.' ||
	    tok->tok == TOK_PTR || tok->tok == TOK_INCR || tok->tok ==
	    TOK_DECR) {
		if (maybe_match('(')) {
			n = new_node(N_CALL, n, NULL, NULL, n->type);
			n->params = argument_expr_list();
		} else if (maybe_match('[')) {
			r = expr();
			n = new_node(N_ADD, n, r, NULL, l->type->ptr);
			array = 1;
			match(']');
		} else if (maybe_match('.')) {
			if (!l->type->_struct)
				errx(1, "Invalid operation for member on"
				    " non-struct at line %d", tok->line);
			if (!(f = find_field(l->type->_struct->fields,
			    tok->str)))
				errx(1, "No field '%s' in struct %s at line %d",
				    tok->str, l->type->_struct->name,
				    tok->line);
			next();
			n = new_node(N_FIELD, n, (void *)f, NULL, f->type);
		} else if (maybe_match(TOK_PTR)) {
			if (!l->type->ptr || !l->type->ptr->_struct)
				errx(1, "Invalid operation for member on"
				    " non-struct pointer at line %d",
				    tok->line);
			if (!(f = find_field(l->type->ptr->_struct->fields,
			    tok->str)))
				errx(1, "No field '%s' in struct %s at line %d",
				    tok->str, l->type->_struct->name,
				    tok->line);
			next();
			n = new_node(N_DEREF, n, 0, NULL, n->type);
			n = new_node(N_FIELD, n, (void *)f, NULL, f->type);
		} else if (tok->tok == TOK_INCR || tok->tok == TOK_DECR) {
			r = new_node(N_CONSTANT, NULL, NULL, (void *)1,
			    new_type(4));
			r = new_node(tok->tok == TOK_INCR ? N_ADD : N_SUB, l, r,
			    NULL, l->type);
			r = new_node(N_ASSIGN, l, r, NULL, l->type);
			n = new_node(N_COMMA, l, r, NULL, l->type);
			next();
		}
	}
	if (array)
		n = new_node(N_DEREF, n, 0, NULL, n->type);
	return (n);
}

static struct node *
unary_expr(void)
{
	struct node *n, *r;
	struct type *_type;
	enum tokens t;

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
	} else if (maybe_match('!')) {
		n = unary_expr();
		return (new_node(N_NOT, n, NULL, 0, n->type));
	} else if (maybe_match('~')) {
		r = unary_expr();
		_type = new_type(4);
		n = new_node(N_CONSTANT, NULL, NULL, (void *)0, _type);
		n = new_node(N_SUB, n, r, NULL, r->type);
		r = new_node(N_CONSTANT, NULL, NULL, (void *)1, _type);
		return (new_node(N_SUB, n, r, NULL, r->type));
	} else if (maybe_match('-')) {
		r = unary_expr();
		_type = new_type(4);
		n = new_node(N_CONSTANT, NULL, NULL, (void *)0, _type);
		return (new_node(N_SUB, n, r, NULL, r->type));
	} else if (tok->tok == TOK_INCR || tok->tok == TOK_DECR) {
		t = tok->tok;
		next();
		r = unary_expr();
		n = new_node(N_CONSTANT, NULL, NULL, (void *)1, new_type(4));
		n = new_node(t == TOK_INCR ? N_ADD : N_SUB, r, n, NULL,
		    r->type);
		return (new_node(N_ASSIGN, r, n, NULL, r->type));
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
relational_expr(void)
{
	struct node *l, *r;
	enum tokens t;
	int op;

	l = additive_expr();
	while (tok->tok == TOK_LT || tok->tok == TOK_LE || tok->tok == TOK_GT ||
	    tok->tok == TOK_GE) {
		t = tok->tok;
		next();
		if (t == TOK_LT)
			op = N_LT;
		else if (t == TOK_LE)
			op = N_LE;
		else if (t == TOK_GT)
			op = N_GT;
		else if (t == TOK_GE)
			op = N_GE;
		r = additive_expr();
		l = new_node(op, l, r, 0, l->type);
	}
	return (l);
}

static struct node *
equality_expr(void)
{
	struct node *l, *r;
	enum tokens t;

	l = relational_expr();
	while (tok->tok == TOK_EQ || tok->tok == TOK_NE) { 
		t = tok->tok;
		next();
		r = relational_expr();
		l = new_node(t == TOK_EQ ? N_EQ : N_NE, l, r, 0, l->type);
	}
	return (l);
}

static struct node *
xor_expr(void)
{
	struct node *l, *r;

	l = equality_expr();
	while (maybe_match('^')) {
		r = equality_expr();
		l = new_node(N_XOR, l, r, 0, l->type);
	}
	return (l);
}

static struct node *
and_expr(void)
{
	struct node *l, *r;

	l = xor_expr();
	while (maybe_match('&')) {
		r = xor_expr();
		l = new_node(N_AND, l, r, 0, l->type);
	}
	return (l);
}

static struct node *
or_expr(void)
{
	struct node *l, *r;

	l = and_expr();
	while (maybe_match('|')) {
		r = and_expr();
		l = new_node(N_OR, l, r, 0, l->type);
	}
	return (l);
}

static struct node *
land_expr(void)
{
	struct node *l, *r;

	l = or_expr();
	while (maybe_match(TOK_AND)) {
		r = or_expr();
		l = new_node(N_LAND, l, r, 0, l->type);
	}
	return (l);
}

static struct node *
lor_expr(void)
{
	struct node *l, *r;

	l = land_expr();
	while (maybe_match(TOK_OR)) {
		r = land_expr();
		l = new_node(N_LOR, l, r, 0, l->type);
	}
	return (l);
}

static struct node *
assign_expr(void)
{
	struct node *l, *r;
	enum tokens t;

	l = lor_expr();
	while (tok->tok == '=' || tok->tok == TOK_ASSADD || tok->tok ==
	    TOK_ASSSUB || tok->tok == TOK_ASSMUL || tok->tok == TOK_ASSDIV) {
		t = tok->tok;
		next();
		r = assign_expr();
		if (t == TOK_ASSADD)
			r = new_node(N_ADD, l, r, 0, l->type);
		else if (t == TOK_ASSSUB)
			r = new_node(N_SUB, l, r, 0, l->type);
		else if (t == TOK_ASSMUL)
			r = new_node(N_MUL, l, r, 0, l->type);
		else if (t == TOK_ASSDIV)
			r = new_node(N_DIV, l, r, 0, l->type);
		l = new_node(N_ASSIGN, l, r, 0, l->type);
	}
	return (l);
}

static struct node *
expr(void)
{
	struct node *l, *r;

	l = assign_expr();
	while (maybe_match(',')) {
		r = assign_expr();
		l = new_node(N_COMMA, l, r, 0, l->type);
	}
	return (l);
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
_do(void)
{
	struct node *cond, *l, *n;
	int old_brk, old_cont;

	old_brk = break_lbl;
	old_cont = cont_lbl;
	break_lbl = new_label();
	cont_lbl = new_label();
	match(TOK_DO);
	l = stmts();
	match(TOK_WHILE);
	match('(');
	cond = expr();
	match(')');
	n = new_node(N_DO, l, 0, 0, NULL);
	n->cond = cond;
	n->break_lbl = break_lbl;
	n->cont_lbl = cont_lbl;
	break_lbl = old_brk;
	cont_lbl = old_cont;
	return (n);
}

static struct node *
_for(void)
{
	struct node *cond, *l, *n, *post, *pre;
	int old_brk, old_cont;

	match(TOK_FOR);
	match('(');
	pre = expr();
	match(';');
	cond = expr();
	match(';');
	post = expr();
	match(')');

	old_brk = break_lbl;
	old_cont = cont_lbl;
	break_lbl = new_label();
	cont_lbl = new_label();
	l = stmts();
	n = new_node(N_FOR, l, 0, 0, NULL);
	n->cond = cond;
	n->pre = pre;
	n->post = post;
	n->break_lbl = break_lbl;
	n->cont_lbl = cont_lbl;
	break_lbl = old_brk;
	cont_lbl = old_cont;

	return (n);
}

static struct node *
_while(void)
{
	struct node *cond, *l, *n;
	int old_brk, old_cont;

	match(TOK_WHILE);
	match('(');
	cond = expr();
	match(')');

	old_brk = break_lbl;
	old_cont = cont_lbl;
	break_lbl = new_label();
	cont_lbl = new_label();
	l = stmts();
	n = new_node(N_WHILE, l, 0, 0, NULL);
	n->break_lbl = break_lbl;
	n->cont_lbl = cont_lbl;
	break_lbl = old_brk;
	cont_lbl = old_cont;

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
		else if (tok->tok == TOK_DO)
			n = _do();
		else if (tok->tok == TOK_FOR)
			n = _for();
		else if (tok->tok == TOK_IF)
			n = _if();
		else if (tok->tok == TOK_WHILE)
			n = _while();
		else if (maybe_match(TOK_BREAK)) {
			n = new_node(N_GOTO, (void *)(long)break_lbl, NULL,
			    NULL, NULL);
		} else if (maybe_match(TOK_CONTINUE)) {
			n = new_node(N_GOTO, (void *)(long)cont_lbl, NULL,
			    NULL, NULL);
		} else {
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
		if (maybe_match('}'))
			break;
		n = stmt();
		if (!head)
			head = n;
		if (last)
			last->next = n;
		if (n)
			last = n;
	}
	if (!head)
		return (new_node(N_NOP, NULL, NULL, 0, NULL));
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

	if (tok->tok == ';') {
		next();
		return;
	}
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
