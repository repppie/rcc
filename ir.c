#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <assert.h>

#include "rcc.h"

struct ir *head_ir;
static struct ir *last_ir;

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

static int cur_reg;

int
alloc_reg(void)
{
	return cur_reg++;
}

static int
gen_ir_op(struct node *n)
{
	struct symbol *sym;
	int dst, l, op, r;

	switch (n->op) {
	case N_ADD:
	case N_SUB:
	case N_MUL:
	case N_DIV:
		n->killable = 1;
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
		if (n->l->killable)
			new_ir(IR_KILL, l, 0, 0);
		if (n->r->killable)
			new_ir(IR_KILL, r, 0, 0);
		return (dst);
	case N_CONSTANT:
		n->killable = 1;
		dst = alloc_reg();
		new_ir(IR_LOADI, n->val, 0, dst);
		return (dst);
	case N_SYM:
		assert(n->sym->assigned);
		return (n->sym->val);
	case N_ASSIGN:
		sym = n->l->sym;
		if (!sym->assigned) {
			sym->assigned = 1;
			sym->val = dst = alloc_reg();
		}
		r = gen_ir_op(n->r);
		new_ir(IR_MOV, r, 0, dst);
		return (sym->val);
	case N_MULTIPLE:
		for (n = n->l; n; n = n->next)
			gen_ir_op(n);
		return (-1);
	default:
		errx(1, "Unknown node op %d\n", n->op);
		return (-1);
	}
}

void
gen_ir(struct node *n)
{
	int r;

	r = gen_ir_op(n);
}

void
gen_ir_ret(struct node *n)
{
	int r;

	r = gen_ir_op(n);
	new_ir(IR_RET, r, 0, 0);
}

static char *ir_names[NR_IR_OPS] = {
    [IR_ADD] = "ADD",
    [IR_SUB] = "SUB",
    [IR_MUL] = "MUL",
    [IR_DIV] = "DIV",
    [IR_LOADI] = "LOADI",
    [IR_KILL] = "KILL",
    [IR_RET] = "RET",
    [IR_MOV] = "MOV",
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
