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
		struct node *cond;
		struct param *params;
	};
	struct node *pre;
	struct node *post;
	union {
		long val;
		char *str;
		struct symbol *sym;
	};
	int op;
	struct type *type;
};

enum node_op {
	N_NOP,
	N_ADD,
	N_SUB,
	N_MUL,
	N_DIV,
	N_CONSTANT,
	N_SYM,
	N_CALL,
	N_ASSIGN,
	N_MULTIPLE,
	N_RETURN,
	N_EQ,
	N_NE,
	N_LT,
	N_LE,
	N_GT,
	N_GE,
	N_IF,
	N_FIELD,
	N_WHILE,
	N_FOR,
	N_DO,
	N_DEREF,
	N_ADDR,
	N_NOT,
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
	IR_NOT,
	IR_LOADI,
	IR_LOADG,
	IR_LOAD,
	IR_LOAD32,
	IR_LOAD8,
	IR_LOADO,
	IR_LOADO32,
	IR_LOADO8,
	IR_STORE,
	IR_STORE32,
	IR_STORE8,
	IR_KILL,
	IR_ENTER,
	IR_RET,
	IR_MOV,
	IR_EQ,
	IR_NE,
	IR_LT,
	IR_LE,
	IR_GT,
	IR_GE,
	IR_CBR,
	IR_JUMP,
	IR_LABEL,
	IR_CALL,
	NR_IR_OPS,
};

#define	SYMTAB_SIZE 1021

extern struct symtab *symtab;
extern struct symbol *strings;

struct symbol {
	struct symbol *next;
	char *name;
	int loc;
	int assigned;
	int func;
	int global;
	struct type *type;
	struct node *body;
	struct ir *ir;
	struct param *params;
	struct symtab *tab;
	char *str;
};

struct symtab {
	struct symbol *tab[SYMTAB_SIZE];
	struct _struct *structs[SYMTAB_SIZE];
	struct symtab *prev;
	int level;
	int ar_offset;
};

struct param {
	struct param *next;
	union {
		struct node *n;
		struct symbol *sym;
	};
	int val;
};

struct type {
	int size;
	int stacksize;
	int array;
	struct type *ptr;
	struct _struct *_struct;
};

struct struct_field {
	struct struct_field *next;
	char *name;
	struct type *type;
	int off;
};

struct _struct {
	struct _struct *next;
	char *name;
	struct type *type;
	struct struct_field *fields;
};

void lex(FILE *f);

struct type *new_type(int size);
void parse(void);

void dump_ir_op(FILE *f, struct ir *ir);
void dump_ir(void);
void gen_ir(void);

void emit_x86(void);

struct symbol *add_sym(char *name, struct type *type);
struct symbol *add_string(char *str);
struct _struct *add_struct(char *name);
struct symbol *find_sym(char *name);
struct symbol *find_global_sym(char *name);
struct _struct *find_struct(char *name);
void new_symtab(void);
void del_symtab(void);

#endif
