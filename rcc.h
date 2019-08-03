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
	TOK_CHAR,
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
	struct node *next;
	union {
		long val;
		char *str;
		struct symbol *sym;
	};
	int op;
	int killable;
};

enum node_op {
	N_ADD,
	N_SUB,
	N_MUL,
	N_DIV,
	N_CONSTANT,
	N_SYM,
	N_ASSIGN,
	N_MULTIPLE,
	N_RETURN,
};

extern struct ir *head_ir;

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
	IR_KILL,
	IR_RET,
	IR_MOV,
	NR_IR_OPS,
};

struct symbol {
	struct symbol *next;
	char *name;
	int val;
	int assigned;
};

void lex(FILE *f);

void parse(void);

void dump_ir_op(FILE *f, struct ir *ir);
void dump_ir(void);
void gen_ir(struct node *n);

void emit_x86(void);

struct symbol *add_sym(char *name);
struct symbol *find_sym(char *name);

#endif
