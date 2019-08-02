#ifndef _RCC_H
#define _RCC_H

extern struct token *tok;

struct token {
	struct token *next;
	int tok;
	int line;
	union {
		long val;
		char *str;
	};
};

enum tokens {
	TOK_CONSTANT = 0x80,
	TOK_ID,
	TOK_STRING,
	TOK_PTR,
	TOK_INCR,
	TOK_DECR,
	TOK_SL,
	TOK_SR,
	TOK_LT,
	TOK_GT,
	TOK_LE,
	TOK_GE,
	TOK_EQ,
	TOK_NE,
	TOK_AND,
	TOK_OR,
	TOK_ELL,
	TOK_ASSMUL,
	TOK_ASSDIV,
	TOK_ASSMOD,
	TOK_ASSADD,
	TOK_ASSSUB,
	TOK_ASSSL,
	TOK_ASSSR,
	TOK_ASSAND,
	TOK_ASSXOR,
	TOK_ASSOR,
	TOK_AUTO,
	TOK_BREAK,
	TOK_CASE,
	TOK_CONST,
	TOK_CONTINUE,
	TOK_DEFAULT,
	TOK_DO,
	TOK_DOUBLE,
	TOK_ELSE,
	TOK_ENUM,
	TOK_EXTERN,
	TOK_FLOAT,
	TOK_FOR,
	TOK_GOTO,
	TOK_IF,
	TOK_INLINE,
	TOK_INT,
	TOK_LONG,
	TOK_REGISTER,
	TOK_RESTRICT,
	TOK_RETURN,
	TOK_SHORT,
	TOK_SIGNED,
	TOK_SIZEOF,
	TOK_STATIC,
	TOK_STRUCT,
	TOK_SWITCH,
	TOK_TYPEDEF,
	TOK_UNION,
	TOK_UNSIGNED,
	TOK_VOID,
	TOK_VOLATILE,
	TOK_WHILE,
	TOK_EOF,
};

struct node {
	struct node *l;
	struct node *r;
	long val;
	int op;
};

enum node_op {
	N_ADD,
	N_SUB,
	N_MUL,
	N_DIV,
	N_CONSTANT,
};

/* OP l,r -> dst */
struct ir {
	struct ir *next;
	int op;
	long o1;
	long o2;
	long dst;
};

enum ir_op {
	IR_ADD,
	IR_SUB,
	IR_MUL,
	IR_DIV,
	IR_LOADI,
	NR_IR_OPS,
};

void lex(FILE *f);

void parse(void);

int gen_ir(struct node *n);
void dump_ir(void);

#endif
