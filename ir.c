#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <assert.h>

#include "rcc.h"

struct ir *head_ir;
static struct ir *last_ir;

static int labels;

static int gen_ir_op(struct node *n);

static int
_sizeof(struct type *t)
{
	if (t->array)
		return (_sizeof(t->ptr) * t->size);
	else if (t->ptr)
		return (8);
	else
		return (t->size);
}

static int
new_label(void)
{
	return labels++;
}

static struct ir *
new_ir(int op, long o1, long o2, long dst)
{
	struct ir *ir;

	ir = malloc(sizeof(struct ir));
	memset(ir, 0, sizeof(struct ir));
	if (!head_ir)
		head_ir = ir;
	if (last_ir)
		last_ir->next = ir;
	last_ir = ir;
	ir->op = op;
	ir->o1 = o1;
	ir->o2 = o2;
	ir->dst = dst;

	return (ir);
}

/* IR register 0 is pointer to AR. */
#define	RARP 0
static int cur_reg = 1;

static int
alloc_reg(void)
{
	return cur_reg++;
}

static void
ir_load(long o1, long dst, int size)
{
	switch (size) {
	case 8:
		new_ir(IR_LOAD, o1, 0, dst);
		break;
	case 4:
		new_ir(IR_LOAD32, o1, 0, dst);
		break;
	default:
		errx(1, "Invalid load size %d", size);
	}
}

static void
ir_loado(long o1, long o2, long dst, int size)
{
	switch (size) {
	case 8:
		new_ir(IR_LOADO, o1, o2, dst);
		break;
	case 4:
		new_ir(IR_LOADO32, o1, o2, dst);
		break;
	default:
		errx(1, "Invalid load size %d", size);
	}
}

static void
ir_store(long o1, long dst, int size)
{
	switch (size) {
	case 8:
		new_ir(IR_STORE, o1, 0, dst);
		break;
	case 4:
		new_ir(IR_STORE32, o1, 0, dst);
		break;
	default:
		errx(1, "Invalid store size %d", size);
	}
}

static void
ir_storeo(long o1, long o2, long dst, int size)
{
	switch (size) {
	case 8:
		new_ir(IR_STOREO, o1, o2, dst);
		break;
	case 4:
		new_ir(IR_STOREO32, o1, o2, dst);
		break;
	default:
		errx(1, "Invalid store size %d", size);
	}
}

static int
gen_if(struct node *n)
{
	int cond, else_lbl, if_lbl, out_lbl;

	cond = gen_ir_op(n->cond);
	if_lbl = new_label();
	else_lbl = new_label();
	out_lbl = new_label();
	new_ir(IR_CBR, cond, if_lbl, else_lbl);
	new_ir(IR_KILL, cond, 0, 0);
	new_ir(IR_LABEL, if_lbl, 0, 0);
	gen_ir_op(n->l);
	new_ir(IR_JUMP, 0, 0, out_lbl);
	new_ir(IR_LABEL, else_lbl, 0, 0);
	if (n->r)
		gen_ir_op(n->r);
	new_ir(IR_LABEL, out_lbl, 0, 0);

	return (-1);
}

static int
gen_while(struct node *n)
{
	int cond, start, in, out;

	start = new_label();
	in = new_label();
	out = new_label();
	new_ir(IR_LABEL, start, 0, 0);
	cond = gen_ir_op(n->cond);
	new_ir(IR_CBR, cond, in, out);
	new_ir(IR_KILL, cond, 0, 0);
	new_ir(IR_LABEL, in, 0, 0);
	gen_ir_op(n->l);
	new_ir(IR_JUMP, 0, 0, start);
	new_ir(IR_LABEL, out, 0, 0);

	return (-1);
}

static int
gen_ir_op(struct node *n)
{
	struct symbol *sym;
	struct param *p;
	int dst, l, op, r, tmp;

	switch (n->op) {
	case N_ADD:
	case N_SUB:
		if (n->op == N_ADD)
			op = IR_ADD;
		else if (n->op == N_SUB)
			op = IR_SUB;

		if (n->r->type->ptr) {
			struct node *_t;

			_t = n->l;
			n->l = n->r;
			n->r = _t;
		}
		l = gen_ir_op(n->l);
		r = gen_ir_op(n->r);
		if (n->l->type->ptr) {
			tmp = alloc_reg();
			new_ir(IR_LOADI, _sizeof(n->l->type->ptr), 0, tmp);
			new_ir(IR_MUL, tmp, r, r);
			new_ir(IR_KILL, tmp, 0, 0);
		}
		dst = alloc_reg();
		new_ir(op, l, r, dst);
		new_ir(IR_KILL, l, 0, 0);
		new_ir(IR_KILL, r, 0, 0);
		return (dst);
	case N_MUL:
	case N_DIV:
		if (n->op == N_MUL)
			op = IR_MUL;
		else if (n->op == N_DIV)
			op = IR_DIV;
		l = gen_ir_op(n->l);
		r = gen_ir_op(n->r);
		dst = alloc_reg();
		new_ir(op, l, r, dst);
		new_ir(IR_KILL, l, 0, 0);
		new_ir(IR_KILL, r, 0, 0);
		return (dst);
	case N_ADDR:
		dst = alloc_reg();
		if (n->l->sym->global)
			new_ir(IR_LOADG, (long)n->l->sym->name, 0, dst);
		else {
			tmp = alloc_reg();
			new_ir(IR_LOADI, n->l->sym->loc, 0, tmp);
			new_ir(IR_ADD, tmp, RARP, dst);
			new_ir(IR_KILL, tmp, 0, 0);
		}
		return (dst);
	case N_DEREF:
		l = gen_ir_op(n->l);
		dst = alloc_reg();
		ir_load(l, dst, _sizeof(n->type));
		new_ir(IR_KILL, l, 0, 0);
		return (dst);
	case N_CONSTANT:
		dst = alloc_reg();
		new_ir(IR_LOADI, n->val, 0, dst);
		return (dst);
	case N_SYM:
		dst = alloc_reg();
		if (n->type->array) {
			if (n->sym->global)
				new_ir(IR_LOADG, (long)n->sym->name, 0, dst);
			else {
				tmp = alloc_reg();
				new_ir(IR_LOADI, n->sym->loc, 0, tmp);
				new_ir(IR_ADD, tmp, RARP, dst);
				new_ir(IR_KILL, tmp, 0, 0);
			}
			return (dst);
		}
		tmp = alloc_reg();
		if (n->sym->global) {
			new_ir(IR_LOADG, (long)n->sym->name, 0, tmp);
			ir_load(tmp, dst, _sizeof(n->type));
		} else {
			new_ir(IR_LOADI, n->sym->loc, 0, tmp);
			ir_loado(RARP, tmp, dst, _sizeof(n->type));
			new_ir(IR_KILL, tmp, 0, 0);
		}
		return (dst);
	case N_ASSIGN:
		r = gen_ir_op(n->r);
		if (n->l->op == N_DEREF) {
			l = gen_ir_op(n->l->l);
			ir_store(r, l, _sizeof(n->l->type));
			new_ir(IR_KILL, r, 0, 0);
			return (l);
		}
		sym = n->l->sym;
		tmp = alloc_reg();
		if (sym->global) {
			new_ir(IR_LOADG, (long)sym->name, 0, tmp);
			ir_store(r, tmp, _sizeof(n->type));
		} else {
			new_ir(IR_LOADI, sym->loc, 0, tmp);
			ir_storeo(r, RARP, tmp, _sizeof(n->type));
		}
		new_ir(IR_KILL, tmp, 0, 0);
		return (r);
	case N_MULTIPLE:
		for (n = n->l; n; n = n->next) {
			l = gen_ir_op(n);
			if (l != -1)
				new_ir(IR_KILL, l, 0, 0);
		}
		return (-1);
	case N_CALL:
		dst = alloc_reg();
		assert(n->l->op == N_SYM);
		for (p = n->params; p; p = p->next)
			 p->val = gen_ir_op(p->n);
		new_ir(IR_CALL, (long)n->l->sym, (long)n->params, dst);
		for (p = n->params; p; p = p->next)
			new_ir(IR_KILL, p->val, 0, 0);
		return (dst);
	case N_RETURN:
		l = -1;
		if (n->l)
			l = gen_ir_op(n->l);
		new_ir(IR_RET, l, 0, 0);
		return (-1);
	case N_NE:
	case N_EQ:
		dst = alloc_reg();
		l = gen_ir_op(n->l);
		r = gen_ir_op(n->r);
		new_ir(n->op == N_EQ ? IR_EQ : IR_NE, l, r, dst);
		new_ir(IR_KILL, l, 0, 0);
		new_ir(IR_KILL, r, 0, 0);
		return (dst);
	case N_IF:
		return (gen_if(n));
	case N_WHILE:
		return (gen_while(n));
	default:
		errx(1, "Unknown node op %d", n->op);
		return (-1);
	}
}

void
gen_ir(void)
{
	struct symbol *s;
	int i;

	for (i = 0; i < SYMTAB_SIZE; i++) {
		s = symtab->tab[i];
		if (s && s->body) {
			head_ir = NULL;
			last_ir = NULL;
			new_ir(IR_ENTER, s->tab->ar_offset, (long)s->params, 0);
			gen_ir_op(s->body);
			s->ir = head_ir;
		}
	}
}

static char *ir_names[NR_IR_OPS] = {
    [IR_ADD] = "ADD",
    [IR_SUB] = "SUB",
    [IR_MUL] = "MUL",
    [IR_DIV] = "DIV",
    [IR_LOADI] = "LOADI",
    [IR_LOADG] = "LOADG",
    [IR_LOAD] = "LOAD",
    [IR_LOAD32] = "LOAD32",
    [IR_LOADO] = "LOADO",
    [IR_LOADO32] = "LOADO32",
    [IR_STORE] = "STORE",
    [IR_STORE32] = "STORE32",
    [IR_STOREO] = "STOREO",
    [IR_STOREO32] = "STOREO32",
    [IR_KILL] = "KILL",
    [IR_RET] = "RET",
    [IR_MOV] = "MOV",
    [IR_ENTER] = "ENTER",
    [IR_RET] = "RET",
    [IR_EQ] = "EQ",
    [IR_NE] = "NE",
    [IR_CBR] = "CBR",
    [IR_JUMP] = "JUMP",
    [IR_LABEL] = "LABEL",
    [IR_CALL] = "CALL",
};

void
dump_ir_op(FILE *f, struct ir *ir)
{
	fprintf(f, "%s %ld, %ld, %ld\n", ir_names[ir->op], ir->o1, ir->o2,
	    ir->dst);
}

void
dump_ir(void)
{
	struct ir *ir;

	for (ir = head_ir; ir; ir = ir->next)
		dump_ir_op(stdout, ir);
}
