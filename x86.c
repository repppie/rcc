#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <err.h>

#include "rcc.h"

static FILE *out;

static void
emit_no_nl(char *s, ...)
{
	va_list ap;

	va_start(ap, s);
	vfprintf(out, s, ap);
	va_end(ap);
}

static void
emit(char *s, ...)
{
	va_list ap;

	va_start(ap, s);
	vfprintf(out, s, ap);
	va_end(ap);

	fputs("\n", out);
}

#define	MAX_IR_REGS 1024
static int ir_regs[MAX_IR_REGS];
#define	NR_X86_REGS 13
static int x86_regs[NR_X86_REGS];
static char *x86_regs_names[NR_X86_REGS] = { "X", "rax", "rbx", "rcx", "rdx",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" };

#define	RAX 1

static int
next_x86_reg(void)
{
	int i;

	for (i = 1; i < NR_X86_REGS; i++) {
		if (!x86_regs[i]) {
			x86_regs[i]++;
			return (i);
		}
	}

	errx(1, "Ran out of x86 registers\n");
}

static char *
x86_reg(int ireg)
{
	if (!ir_regs[ireg])
		ir_regs[ireg] = next_x86_reg();

	return (x86_regs_names[ir_regs[ireg]]);
}

static void
kill_reg(int ireg)
{
	int x;

	x = ir_regs[ireg];
	if (x == 0)
		errx(1, "Kill on free register %d\n", ireg);
	x86_regs[x] = 0;
	ir_regs[ireg] = 0;
}

static void
kill_all(void)
{
	int i;

	for (i = 0; i < MAX_IR_REGS; i++)
		if (ir_regs[i])
			kill_reg(i);
}

static void
emit_x86_op(struct ir *ir)
{
	emit_no_nl("# ");
	dump_ir_op(out, ir);

	switch (ir->op) {
	case IR_LOADI:
		emit("movq $%ld, %%%s", ir->o1, x86_reg(ir->dst));
		break;
	case IR_KILL:
		kill_reg(ir->o1);
		break;
	case IR_ADD:
	case IR_SUB:
	case IR_MUL:
		emit("pushq %%%s", x86_reg(ir->o1));
		if (ir->op == IR_ADD)
			emit("addq %%%s, %%%s", x86_reg(ir->o2),
			    x86_reg(ir->o1));
		else if (ir->op == IR_SUB)
			emit("subq %%%s, %%%s", x86_reg(ir->o2),
			    x86_reg(ir->o1));
		else if (ir->op == IR_MUL)
			emit("imulq %%%s, %%%s", x86_reg(ir->o2),
			    x86_reg(ir->o1));
		emit("movq %%%s, %%%s", x86_reg(ir->o1), x86_reg(ir->dst));
		emit("popq %%%s", x86_reg(ir->o1));
		break;
	case IR_DIV:
		emit("pushq %%rax");
		emit("pushq %%rdx");
		if (ir_regs[ir->o2] == RAX) {
			emit("xchg %%%s, %%rax", x86_reg(ir->o1));
			emit("xorl %%edx,%%edx");
			emit("idivq %%%s", x86_reg(ir->o1));
		} else {
			emit("movq %%%s,%%rax", x86_reg(ir->o1));
			emit("xorl %%edx,%%edx");
			emit("idivq %%%s", x86_reg(ir->o2));
		}
		emit("popq %%rdx");
		emit("movq %%rax, %%%s", x86_reg(ir->dst));
		if (ir_regs[ir->dst] != RAX)
			emit("popq %%rax");
		break;
	case IR_MOV:
		emit("mov %%%s, %%%s", x86_reg(ir->o1), x86_reg(ir->dst));
		break;
	case IR_RET:
		emit("leaq fmt, %%rdi");
		emit("movq %%%s, %%rsi", x86_reg(ir->o1));
		emit("xorl %%eax,%%eax");
		emit("callq printf");
		kill_all();
		break;
	default:
		errx(1, "Unknown IR instruction %d\n", ir->op);
	}
}

void
emit_x86(void)
{
	struct ir *ir;

	if ((out = fopen("out.S", "w")) < 0)
		err(1, "fopen");

	emit("fmt: .asciz \"%%ld\\n\"");
	emit(".globl main");
	emit("main:");

	for(ir = head_ir; ir; ir = ir->next)
		emit_x86_op(ir);

	emit("xorl %%eax,%%eax");
	emit("retq");

	if (fclose(out))
		err(1, "fclose");
}
