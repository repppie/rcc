#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <assert.h>

#include "rcc.h"

struct ir *head_ir;
struct ir *last_ir;

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

struct ir *
new_ir(int op, struct ir_oprnd *o1, struct ir_oprnd *o2, struct ir_oprnd *dst)
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
	if (o1)
		ir->o1 = *o1;
	if (o2)
		ir->o2 = *o2;
	if (dst)
		ir->dst = *dst;

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
ir_enter(int off, struct param *p)
{
	struct ir_oprnd o1 = { off, IRO_IMM }, o2 = { (long)p, IRO_MISC };

	new_ir(IR_ENTER, &o1, &o2, NULL);
}

static void
ir_kill(int _o1)
{
	struct ir_oprnd o1 = { _o1, IRO_TEMP };

	new_ir(IR_KILL, &o1, NULL, NULL);
}

static void
ir_load(int _o1, int _dst, int size)
{
	struct ir_oprnd o1 = { _o1, IRO_TEMP }, dst = { _dst, IRO_TEMP };

	switch (size) {
	case 8:
		new_ir(IR_LOAD, &o1, NULL, &dst);
		break;
	case 4:
		new_ir(IR_LOAD32, &o1, NULL, &dst);
		break;
	case 1:
		new_ir(IR_LOAD8, &o1, NULL, &dst);
		break;
	default:
		errx(1, "Invalid load size %d", size);
	}
}

static void
ir_loado(int _o1, int _o2, int _dst, int size)
{
	struct ir_oprnd o1 = { _o1, IRO_TEMP }, o2 = { _o2, IRO_TEMP }, dst =
	    { _dst, IRO_TEMP };

	switch (size) {
	case 8:
		new_ir(IR_LOADO, &o1, &o2, &dst);
		break;
	case 4:
		new_ir(IR_LOADO32, &o1, &o2, &dst);
		break;
	case 1:
		new_ir(IR_LOADO8, &o1, &o2, &dst);
		break;
	default:
		errx(1, "Invalid load size %d", size);
	}
}

static void
ir_store(int _o1, int _dst, int size)
{
	struct ir_oprnd o1 = { _o1, IRO_TEMP }, dst = { _dst, IRO_TEMP };

	switch (size) {
	case 8:
		new_ir(IR_STORE, &o1, NULL, &dst);
		break;
	case 4:
		new_ir(IR_STORE32, &o1, NULL, &dst);
		break;
	case 1:
		new_ir(IR_STORE8, &o1, NULL, &dst);
		break;
	default:
		errx(1, "Invalid store size %d", size);
	}
}

static void
ir_loadi(int imm, int _dst)
{
	struct ir_oprnd o1 = { imm, IRO_IMM }, dst = { _dst, IRO_TEMP };

	new_ir(IR_LOADI, &o1, NULL, &dst);
}

static void
ir_loadg(struct symbol *s, int _dst)
{
	struct ir_oprnd o1 = { (long)s->name, IRO_GLOBAL }, dst = { _dst,
	    IRO_TEMP };

	new_ir(IR_LOADG, &o1, NULL, &dst);
}

static void
ir_cbr(int cond, int taken, int not)
{
	struct ir_oprnd c = { cond, IRO_TEMP }, n = { not, IRO_LABEL }, t =
	    { taken, IRO_LABEL };

	new_ir(IR_CBR, &c, &t, &n); 
}

static void
ir_jump(int l)
{
	struct ir_oprnd t = { l, IRO_LABEL };

	new_ir(IR_JUMP, NULL, NULL, &t);
}

static void
ir_not(int _o1, int _dst)
{
	struct ir_oprnd o1 = { _o1, IRO_TEMP }, dst = { _dst, IRO_TEMP };

	new_ir(IR_NOT, &o1, NULL, &dst);
}

static void
ir_ret(int _o1)
{
	struct ir_oprnd o1 = { _o1, IRO_TEMP };

	new_ir(IR_RET, &o1, NULL, NULL);
}

static void
ir3(int op, int _o1, int _o2, int _dst)
{
	struct ir_oprnd o1 = { _o1, IRO_TEMP }, o2 = { _o2, IRO_TEMP },
	    dst = { _dst, IRO_TEMP };

	new_ir(op, &o1, &o2, &dst);
}

static void
ir_call(struct symbol *s, struct param *p, int _dst)
{
	struct ir_oprnd o1 = { (long)s, IRO_GLOBAL }, o2 = { (long)p,
	    IRO_MISC }, dst = { _dst, IRO_TEMP }; 

	new_ir(IR_CALL, &o1, &o2, &dst);
}

static void
ir_label(int l)
{
	struct ir_oprnd o1 = { l, IRO_LABEL };

	new_ir(IR_LABEL, &o1, NULL, NULL);
}

static int
gen_if(struct node *n)
{
	int cond, else_lbl, if_lbl, out_lbl;

	cond = gen_ir_op(n->cond);
	if_lbl = new_label();
	else_lbl = new_label();
	out_lbl = new_label();
	ir_cbr(cond, if_lbl, else_lbl);
	ir_kill(cond);
	ir_label(if_lbl);
	gen_ir_op(n->l);
	ir_jump(out_lbl);
	ir_label(else_lbl);
	if (n->r)
		gen_ir_op(n->r);
	ir_jump(out_lbl);
	ir_label(out_lbl);

	return (-1);
}

static int
gen_for(struct node *n)
{
	int cond, start, in, next, out;

	start = new_label();
	in = new_label();
	out = n->break_lbl;
	next = n->cont_lbl;
	gen_ir_op(n->pre);
	ir_jump(start);
	ir_label(start);
	cond = gen_ir_op(n->cond);
	ir_cbr(cond, in, out);
	ir_kill(cond);
	ir_label(in);
	gen_ir_op(n->l);
	ir_jump(next);
	ir_label(next);
	gen_ir_op(n->post);
	ir_jump(start);
	ir_label(out);

	return (-1);
}

static int
gen_do(struct node *n)
{
	int cond, start, next, out;

	start = new_label();
	out = n->break_lbl;
	next = n->cont_lbl;
	ir_jump(start);
	ir_label(start);
	gen_ir_op(n->l);
	ir_jump(next);
	ir_label(next);
	cond = gen_ir_op(n->cond);
	ir_cbr(cond, start, out);
	ir_kill(cond);
	ir_label(out);
	return (-1);
}

static int
gen_while(struct node *n)
{
	int cond, start, in, out;

	start = n->cont_lbl;
	in = new_label();
	out = n->break_lbl;
	ir_jump(start);
	ir_label(start);
	cond = gen_ir_op(n->cond);
	ir_cbr(cond, in, out);
	ir_kill(cond);
	ir_label(in);
	gen_ir_op(n->l);
	ir_jump(start);
	ir_label(out);

	return (-1);
}

static int
gen_lor(struct node *n)
{
	int dst, f, l, next, out, r, t;

	dst = alloc_reg();
	next = new_label();
	out = new_label();
	t = new_label();
	f = new_label();

	l = gen_ir_op(n->l);
	ir_cbr(l, t, next);
	ir_label(next);
	r = gen_ir_op(n->r);
	ir_cbr(r, t, f);
	ir_label(f);
	ir_loadi(0, dst);
	ir_jump(out);
	ir_label(t);
	ir_loadi(1, dst);
	ir_jump(out);
	ir_label(out);
	ir_kill(l);
	ir_kill(r);

	return (dst);
}

static int
gen_land(struct node *n)
{
	int dst, f, l, next, out, r, t;

	dst = alloc_reg();
	next = new_label();
	out = new_label();
	t = new_label();
	f = new_label();

	l = gen_ir_op(n->l);
	ir_cbr(l, next, f);
	ir_label(next);
	r = gen_ir_op(n->r);
	ir_cbr(r, t, f);
	ir_label(t);
	ir_loadi(1, dst);
	ir_jump(out);
	ir_label(f);
	ir_loadi(0, dst);
	ir_jump(out);
	ir_label(out);
	ir_kill(l);
	ir_kill(r);

	return (dst);
}

static int
gen_lval(struct node *n)
{
	int dst, tmp;

	switch (n->op) {
	case N_DEREF:
		return (gen_ir_op(n->l));
	case N_SYM:
		dst = alloc_reg();
		if (n->sym->global)
			ir_loadg(n->sym, dst);
		else {
			tmp = alloc_reg();
			ir_loadi(n->sym->loc, tmp);
			ir3(IR_ADD, tmp, RARP, dst);
			ir_kill(tmp);
		}
		return (dst);
	case N_FIELD:
		tmp = gen_lval(n->l);
		dst = alloc_reg();
		ir_loadi(((struct struct_field *)n->r)->off, dst);
		ir3(IR_ADD, dst, tmp, dst);
		ir_kill(tmp);
		return (dst);
	default:
		errx(1, "Invalid lvalue");
	}
}

static int
gen_ir_op(struct node *n)
{
	struct struct_field *f;
	struct param *p;
	int dst, l, op, r, tmp;

	switch (n->op) {
	case N_NOP:
		return (-1);
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
			ir_loadi(_sizeof(n->l->type->ptr), tmp);
			ir3(IR_MUL, tmp, r, r);
			ir_kill(tmp);
		}
		dst = alloc_reg();
		ir3(op, l, r, dst);
		ir_kill(l);
		ir_kill(r);
		return (dst);
	case N_MUL:
	case N_DIV:
	case N_OR:
	case N_AND:
	case N_XOR:
		if (n->op == N_MUL)
			op = IR_MUL;
		else if (n->op == N_DIV)
			op = IR_DIV;
		else if (n->op == N_OR)
			op = IR_OR;
		else if (n->op == N_AND)
			op = IR_AND;
		else if (n->op == N_XOR)
			op = IR_XOR;
		l = gen_ir_op(n->l);
		r = gen_ir_op(n->r);
		dst = alloc_reg();
		ir3(op, l, r, dst);
		ir_kill(l);
		ir_kill(r);
		return (dst);
	case N_NOT:
		dst = alloc_reg();
		l = gen_ir_op(n->l);
		ir_not(l, dst);
		ir_kill(l);
		return (dst);
	case N_ADDR:
		dst = alloc_reg();
		if (n->l->sym->global)
			ir_loadg(n->l->sym, dst);
		else {
			tmp = alloc_reg();
			ir_loadi(n->l->sym->loc, tmp);
			ir3(IR_ADD, tmp, RARP, dst);
			ir_kill(tmp);
		}
		return (dst);
	case N_DEREF:
		l = gen_ir_op(n->l);
		dst = alloc_reg();
		ir_load(l, dst, _sizeof(n->type));
		ir_kill(l);
		return (dst);
	case N_CONSTANT:
		dst = alloc_reg();
		ir_loadi(n->val, dst);
		return (dst);
	case N_SYM:
		dst = alloc_reg();
		if (n->type->array) {
			if (n->sym->global)
				ir_loadg(n->sym, dst);
			else {
				tmp = alloc_reg();
				ir_loadi(n->sym->loc, tmp);
				ir3(IR_ADD, tmp, RARP, dst);
				ir_kill(tmp);
			}
			return (dst);
		}
		tmp = alloc_reg();
		if (n->sym->global) {
			ir_loadg(n->sym, tmp);
			ir_load(tmp, dst, _sizeof(n->type));
		} else {
			ir_loadi(n->sym->loc, tmp);
			ir_loado(RARP, tmp, dst, _sizeof(n->type));
			ir_kill(tmp);
		}
		return (dst);
	case N_FIELD:
		f = (struct struct_field *)n->r;
		l = gen_lval(n->l);
		dst = alloc_reg();
		tmp = alloc_reg();
		ir_loadi(f->off, tmp);
		ir3(IR_ADD, l, tmp, dst);
		ir_kill(l);
		ir_kill(tmp);
		ir_load(dst, dst, _sizeof(f->type));
		return (dst);
	case N_ASSIGN:
		r = gen_ir_op(n->r);
		tmp = gen_lval(n->l);
		ir_store(r, tmp, _sizeof(n->l->type));
		ir_kill(tmp);
		return (r);
	case N_MULTIPLE:
		for (n = n->l; n; n = n->next) {
			l = gen_ir_op(n);
			if (l != -1)
				ir_kill(l);
		}
		return (-1);
	case N_CALL:
		dst = alloc_reg();
		assert(n->l->op == N_SYM);
		for (p = n->params; p; p = p->next)
			 p->val = gen_ir_op(p->n);
		ir_call(n->l->sym, n->params, dst);
		for (p = n->params; p; p = p->next)
			ir_kill(p->val);
		return (dst);
	case N_RETURN:
		l = -1;
		if (n->l)
			l = gen_ir_op(n->l);
		ir_ret(l);
		return (-1);
	case N_NE:
	case N_EQ:
	case N_LT:
	case N_LE:
	case N_GT:
	case N_GE:
		if (n->op == N_EQ)
			op = IR_EQ;
		else if (n->op == N_NE)
			op = IR_NE;
		else if (n->op == N_LT)
			op = IR_LT;
		else if (n->op == N_LE)
			op = IR_LE;
		else if (n->op == N_GT)
			op = IR_GT;
		else if (n->op == N_GE)
			op = IR_GE;
		dst = alloc_reg();
		l = gen_ir_op(n->l);
		r = gen_ir_op(n->r);
		ir3(op, l, r, dst);
		ir_kill(l);
		ir_kill(r);
		return (dst);
	case N_LOR:
		return (gen_lor(n));
	case N_LAND:
		return (gen_land(n));
	case N_IF:
		return (gen_if(n));
	case N_DO:
		return (gen_do(n));
	case N_FOR:
		return (gen_for(n));
	case N_WHILE:
		return (gen_while(n));
	case N_GOTO:
		ir_jump((long)n->l);
		return (-1);
	case N_COMMA:
		dst = alloc_reg();
		tmp = alloc_reg();
		dst = gen_ir_op(n->l);
		tmp = gen_ir_op(n->r);
		ir_kill(tmp);
		return (dst);
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
			cur_reg = 1;
			ir_enter(s->tab->ar_offset, s->params);
			gen_ir_op(s->body);
			s->ir = head_ir;
			s->nr_temps = cur_reg;
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
    [IR_LOAD8] = "LOAD8",
    [IR_LOADO] = "LOADO",
    [IR_LOADO32] = "LOADO32",
    [IR_LOADO8] = "LOADO8",
    [IR_STORE] = "STORE",
    [IR_STORE32] = "STORE32",
    [IR_STORE8] = "STORE8",
    [IR_KILL] = "KILL",
    [IR_RET] = "RET",
    [IR_MOV] = "MOV",
    [IR_ENTER] = "ENTER",
    [IR_RET] = "RET",
    [IR_EQ] = "EQ",
    [IR_NE] = "NE",
    [IR_LT] = "LT",
    [IR_LE] = "LE",
    [IR_GT] = "GT",
    [IR_GE] = "GE",
    [IR_CBR] = "CBR",
    [IR_JUMP] = "JUMP",
    [IR_LABEL] = "LABEL",
    [IR_CALL] = "CALL",
    [IR_PHI] = "PHI",
};

void
dump_ir_op(FILE *f, struct ir *ir)
{
	fprintf(f, "%s %ld, %ld, %ld\n", ir_names[ir->op], ir->o1.v, ir->o2.v,
	    ir->dst.v);
}

void
dump_ir(struct ir *ir)
{
	int i;
	for (i = 0; ir; ir = ir->next, i++)
		//dump_ir_op(stdout, ir);
		printf("%d: %s %ld, %ld, %ld\n", i, ir_names[ir->op], ir->o1.v,
		    ir->o2.v, ir->dst.v);
}
