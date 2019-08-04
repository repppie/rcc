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
new_label(void)
{
	return labels++;
}

static struct ir *
new_ir(int op, int o1, int o2, int dst)
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

static int
gen_if(struct node *n)
{
	int cond, else_lbl, if_lbl;

	cond = gen_ir_op(n->cond);
	if_lbl = new_label();
	else_lbl = new_label();
	new_ir(IR_CBR, cond, if_lbl, else_lbl);
	new_ir(IR_KILL, cond, 0, 0);
	new_ir(IR_LABEL, if_lbl, 0, 0);
	gen_ir_op(n->l);
	new_ir(IR_LABEL, else_lbl, 0, 0);
	if (n->r)
		gen_ir_op(n->r);

	return (-1);
}

static int
gen_ir_op(struct node *n)
{
	struct symbol *sym;
	int dst, l, op, r, tmp;

	switch (n->op) {
	case N_ADD:
	case N_SUB:
	case N_MUL:
	case N_DIV:
		if (n->op == N_ADD)
			op = IR_ADD;
		else if (n->op == N_SUB)
			op = IR_SUB;
		else if (n->op == N_MUL)
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
	case N_CONSTANT:
		dst = alloc_reg();
		new_ir(IR_LOADI, n->val, 0, dst);
		return (dst);
	case N_SYM:
		dst = alloc_reg();
		tmp = alloc_reg();
		new_ir(IR_LOADI, n->sym->val, 0, tmp);
		new_ir(IR_LOADO, RARP, tmp, dst);
		new_ir(IR_KILL, tmp, 0, 0);
		return (dst);
	case N_ASSIGN:
		assert(n->l->op == N_SYM);
		sym = n->l->sym;
		r = gen_ir_op(n->r);
		tmp = alloc_reg();
		new_ir(IR_LOADI, sym->val, 0, tmp);
		new_ir(IR_STOREO, r, RARP, tmp);
		new_ir(IR_KILL, tmp, 0, 0);
		return (sym->val);
	case N_MULTIPLE:
		for (n = n->l; n; n = n->next)
			gen_ir_op(n);
		return (-1);
	case N_RETURN:
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
	default:
		errx(1, "Unknown node op %d", n->op);
		return (-1);
	}
}

void
gen_ir(struct node *n)
{
	new_ir(IR_ENTER, ar_offset, 0, 0);
	gen_ir_op(n);
}

static char *ir_names[NR_IR_OPS] = {
    [IR_ADD] = "ADD",
    [IR_SUB] = "SUB",
    [IR_MUL] = "MUL",
    [IR_DIV] = "DIV",
    [IR_LOADI] = "LOADI",
    [IR_LOADO] = "LOADO",
    [IR_STOREO] = "STOREO",
    [IR_KILL] = "KILL",
    [IR_RET] = "RET",
    [IR_MOV] = "MOV",
    [IR_ENTER] = "ENTER",
    [IR_RET] = "RET",
    [IR_EQ] = "EQ",
    [IR_NE] = "NE",
    [IR_CBR] = "CBR",
    [IR_LABEL] = "LABEL",
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
