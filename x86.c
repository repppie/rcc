#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <err.h>
#include <assert.h>

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
static char *x86_regs_names[NR_X86_REGS] = { "rsp", "rax", "rbx", "rcx",
    "rdx", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" };
static char *x86_32_regs_names[NR_X86_REGS] = { "XXX", "eax", "ebx", "ecx",
    "edx", "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d" };
static char *x86_16_regs_names[NR_X86_REGS] = { "XXX", "ax", "bx", "cx", "dx",
    "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w" };
static char *x86_8_regs_names[NR_X86_REGS] = { "XXX", "al", "bl", "cl", "dl",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b" };

#define	NR_FUNC_PARAM_REGS 6
static char *param_regs[NR_FUNC_PARAM_REGS] = { "rdi", "rsi", "rdx", "rcx",
    "r8", "r9" };
static char *param_32_regs[NR_FUNC_PARAM_REGS] = { "edi", "esi", "edx", "ecx",
    "r8d", "r9d" };
static char *param_16_regs[NR_FUNC_PARAM_REGS] = { "di", "si", "dx", "cx",
    "r8w", "r9w" };
static char *param_8_regs[NR_FUNC_PARAM_REGS] = { "dil", "sil", "dl", "cl",
    "r8b", "r9b" };

#define	RAX 1

static char *
func_param_reg(int r, int size)
{
	if (size == 1)
		return (param_8_regs[r]);
	else if (size == 2)
		return (param_16_regs[r]);
	else if (size == 4)
		return (param_32_regs[r]);
	else
		return (param_regs[r]);
}

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
x86_reg(struct ir_oprnd *ireg, int size)
{
	assert(ireg->type == IRO_TEMP);

	if (ireg->v && !ir_regs[ireg->v])
		ir_regs[ireg->v] = next_x86_reg();

	if (size == 1)
		return (x86_8_regs_names[ir_regs[ireg->v]]);
	else if (size == 2)
		return (x86_16_regs_names[ir_regs[ireg->v]]);
	else if (size == 4)
		return (x86_32_regs_names[ir_regs[ireg->v]]);
	else
		return (x86_regs_names[ir_regs[ireg->v]]);
}

static void
kill_reg(struct ir_oprnd *ireg)
{
	int x;

	assert(ireg->type == IRO_TEMP);

	x = ir_regs[ireg->v];
	if (x == 0)
		errx(1, "Kill on free register %ld\n", ireg->v);
	x86_regs[x] = 0;
	ir_regs[ireg->v] = 0;
}

static void
kill_all(void)
{
	struct ir_oprnd o;
	int i;

	o.type = IRO_TEMP;
	for (i = 0; i < MAX_IR_REGS; i++) {
		o.v = i;
		if (ir_regs[i])
			kill_reg(&o);
	}
}

static void
emit_x86_op(struct ir *ir)
{
	struct param *p;
	char *instr;
	int i;

	emit_no_nl("# ");
	dump_ir_op(out, ir);

	switch (ir->op) {
	case IR_LOADI:
		emit("movq $%ld, %%%s", ir->o1.v, x86_reg(&ir->dst, 8));
		break;
	case IR_LOADG:
		emit("leaq %s(%%rip), %%%s", (char *)ir->o1.v,
		    x86_reg(&ir->dst, 8));
		break;
	case IR_LOAD:
		emit("movq (%%%s), %%%s", x86_reg(&ir->o1, 8), x86_reg(&ir->dst,
		    8));
		break;
	case IR_LOAD32:
		emit("movl (%%%s), %%%s", x86_reg(&ir->o1, 8), x86_reg(&ir->dst,
		    4));
		break;
	case IR_LOAD8:
		emit("movb (%%%s), %%%s", x86_reg(&ir->o1, 8), x86_reg(&ir->dst,
		    1));
		break;
	case IR_LOADO:
		emit("movq 0(%%%s,%%%s,1), %%%s", x86_reg(&ir->o1, 8),
		    x86_reg(&ir->o2, 8), x86_reg(&ir->dst, 8));
		break;
	case IR_LOADO32:
		emit("movl 0(%%%s,%%%s,1), %%%s", x86_reg(&ir->o1, 8),
		    x86_reg(&ir->o2, 8), x86_reg(&ir->dst, 4));
		break;
	case IR_LOADO8:
		emit("movb 0(%%%s,%%%s,1), %%%s", x86_reg(&ir->o1, 8),
		    x86_reg(&ir->o2, 8), x86_reg(&ir->dst, 1));
		break;
	case IR_STORE:
		emit("movq %%%s, (%%%s)", x86_reg(&ir->o1, 8), x86_reg(&ir->dst,
		    8));
		break;
	case IR_STORE32:
		emit("movl %%%s, (%%%s)", x86_reg(&ir->o1, 4), x86_reg(&ir->dst,
		    8));
		break;
	case IR_STORE8:
		emit("movb %%%s, (%%%s)", x86_reg(&ir->o1, 1), x86_reg(&ir->dst,
		    8));
		break;
	case IR_KILL:
		kill_reg(&ir->o1);
		break;
	case IR_ADD:
	case IR_SUB:
		if (ir->op == IR_SUB)
			emit("negq %%%s", x86_reg(&ir->o2, 8));
		emit("leaq (%%%s, %%%s), %%%s", x86_reg(&ir->o2, 8),
		    x86_reg(&ir->o1, 8), x86_reg(&ir->dst, 8));
		break;
	case IR_MUL:
		if (ir->next->op !=  IR_KILL || ir->next->o1.v != ir->o1.v)
			emit("pushq %%%s", x86_reg(&ir->o1, 8));
		if (ir->op == IR_MUL)
			emit("imulq %%%s, %%%s", x86_reg(&ir->o2, 8),
			    x86_reg(&ir->o1, 8));
		emit("movq %%%s, %%%s", x86_reg(&ir->o1, 8), x86_reg(&ir->dst,
		    8));
		if (ir->next->op !=  IR_KILL || ir->next->o1.v != ir->o1.v)
			emit("popq %%%s", x86_reg(&ir->o1, 8));
		break;
	case IR_DIV:
		emit("pushq %%rax");
		emit("pushq %%rdx");
		if (ir_regs[ir->o2.v] == RAX) {
			emit("xchg %%%s, %%rax", x86_reg(&ir->o1, 8));
			emit("xorl %%edx,%%edx");
			emit("idivq %%%s", x86_reg(&ir->o1, 8));
		} else {
			emit("movq %%%s,%%rax", x86_reg(&ir->o1, 8));
			emit("xorl %%edx,%%edx");
			emit("idivq %%%s", x86_reg(&ir->o2, 8));
		}
		emit("popq %%rdx");
		emit("movq %%rax, %%%s", x86_reg(&ir->dst, 8));
		if (ir_regs[ir->dst.v] != RAX)
			emit("popq %%rax");
		break;
	case IR_OR:
	case IR_AND:
	case IR_XOR:
		if (ir->op == IR_OR)
			instr = "orq";
		else if (ir->op == IR_AND)
			instr = "andq";
		else if (ir->op == IR_XOR)
			instr = "xorq";
		emit("pushq %%%s", x86_reg(&ir->o2, 8));
		emit("%s %%%s, %%%s", instr, x86_reg(&ir->o1, 8),
		    x86_reg(&ir->o2, 8));
		emit("movq %%%s, %%%s", x86_reg(&ir->o2, 8), x86_reg(&ir->dst,
		    8));
		emit("popq %%%s", x86_reg(&ir->o2, 8));
		break;
	case IR_NOT:
		emit("testq %%%s, %%%s", x86_reg(&ir->o1, 8), x86_reg(&ir->o1,
		    8));
		emit("seteb %%%s", x86_reg(&ir->dst, 1));
		emit("movzbq %%%s, %%%s", x86_reg(&ir->dst, 1),
		    x86_reg(&ir->dst, 8));
		break;
	case IR_NE:
	case IR_EQ:
	case IR_LT:
	case IR_LE:
	case IR_GT:
	case IR_GE:
		emit("xorl %%%s,%%%s", x86_reg(&ir->dst, 4), x86_reg(&ir->dst,
		    4));
		emit("cmpq %%%s,%%%s", x86_reg(&ir->o2, 8), x86_reg(&ir->o1,
		    8));
		if (ir->op == IR_EQ)
			emit("sete %%%s", x86_reg(&ir->dst, 1));
		else if (ir->op == IR_NE)
			emit("setne %%%s", x86_reg(&ir->dst, 1));
		else if (ir->op == IR_LT)
			emit("setl %%%s", x86_reg(&ir->dst, 1));
		else if (ir->op == IR_LE)
			emit("setle %%%s", x86_reg(&ir->dst, 1));
		else if (ir->op == IR_GT)
			emit("setg %%%s", x86_reg(&ir->dst, 1));
		else if (ir->op == IR_GE)
			emit("setge %%%s", x86_reg(&ir->dst, 1));
		break;
	case IR_CBR:
		emit("testq %%%s, %%%s", x86_reg(&ir->o1, 8), x86_reg(&ir->o1,
		    8));
		emit("jne .L%d", ir->o2.v);
		emit("je .L%d", ir->dst.v);
		break;
	case IR_JUMP:
		emit("jmp .L%d", ir->dst.v);
		break;
	case IR_LABEL:
		emit(".L%d:", ir->o1.v);
		break;
	case IR_MOV:
		emit("mov %%%s, %%%s", x86_reg(&ir->o1, 8), x86_reg(&ir->dst,
		    8));
		break;
	case IR_CALL:
		for (i = 1; i < MAX_IR_REGS; i++)
			if (ir_regs[i] && i != ir->dst.v)
				emit("pushq %%%s", x86_regs_names[ir_regs[i]]);
		p = (struct param *)ir->o2.v;
		i = 0;
		while (p) {
			int size;

			size = p->n->type->size;
			/* XXX more than 6 params */
			if (i < NR_FUNC_PARAM_REGS) {
				struct ir_oprnd o = { p->val, IRO_TEMP };
				emit("mov %%%s, %%%s", x86_reg(&o, size),
				    func_param_reg(i++, size));
			}
			p = p->next;
		}
		emit("xorl %%eax, %%eax");
		emit("callq %s", ((struct symbol *)ir->o1.v)->name);
		emit("movq %%rax, %%%s", x86_reg(&ir->dst, 8));
		for (i = MAX_IR_REGS; i >= 1; i--)
			if (ir_regs[i] && i != ir->dst.v)
				emit("popq %%%s", x86_regs_names[ir_regs[i]]);
		break;
	case IR_ENTER:
		kill_all();
		emit("pushq %%rbp");
		emit("movq %%rsp, %%rbp");
		break;
	case IR_ALLOC:
		emit("subq $%d, %%rsp", ir->o1.v);
		break;
	case IR_RET:
		emit("movq %%%s, %%rax", x86_reg(&ir->o1, 8));
		emit("leaveq");
		emit("retq");
		break;
	default:
		errx(1, "Unknown IR instruction %d", ir->op);
	}
}

void
emit_x86(void)
{
	struct ir *ir;
	int i;

	if ((out = fopen("out.S", "w")) < 0)
		err(1, "fopen");

	emit(".data");
	for (i = 0; i < SYMTAB_SIZE; i++) {
		if (symtab->tab[i] && !symtab->tab[i]->func) {
			emit("%s:", symtab->tab[i]->name);
			emit(".skip %ld", symtab->tab[i]->type->stacksize);
		}
	}
	while (strings) {
		emit("%s:", strings->name);
		emit(".asciz \"%s\"", strings->str);
		strings = strings->next;
	}

	emit(".text");
	for (i = 0; i < SYMTAB_SIZE; i++) {
		if (symtab->tab[i] && symtab->tab[i]->body) {
			emit(".globl %s", symtab->tab[i]->name);
			emit("%s:", symtab->tab[i]->name);
			for(ir = symtab->tab[i]->func->ir; ir; ir = ir->next)
				emit_x86_op(ir);
		}
	}

	if (fclose(out))
		err(1, "fclose");
}
