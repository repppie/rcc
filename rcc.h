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
	struct type *type;
	union {
		struct node *cond;
		struct param *params;
	};
	struct node *pre;
	struct node *post;
	int break_lbl;
	int cont_lbl;
	union {
		long val;
		char *str;
		struct symbol *sym;
	};
	int op;
};

enum node_op {
	N_NOP,
	N_ADD,
	N_SUB,
	N_MUL,
	N_DIV,
	N_OR,
	N_AND,
	N_XOR,
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
	N_LOR,
	N_LAND,
	N_IF,
	N_FIELD,
	N_WHILE,
	N_FOR,
	N_DO,
	N_GOTO,
	N_DEREF,
	N_ADDR,
	N_NOT,
	N_COMMA,
};

extern struct ir *head_ir;
extern struct ir *last_ir;

struct ir_oprnd {
	long v;
	int type;
	int sub;
};

/* OP l,r -> dst */
struct ir {
	struct ir *next;
	int op;
	struct ir_oprnd o1;
	struct ir_oprnd o2;
	struct ir_oprnd dst;
	int node;
	int leader;
};

enum {
	IRO_UNUSED,
	IRO_IMM,
	IRO_TEMP,
	IRO_GLOBAL,
	IRO_LABEL,
	IRO_MISC,
};

enum ir_op {
	IR_ADD,
	IR_SUB,
	IR_MUL,
	IR_DIV,
	IR_NOT,
	IR_OR,
	IR_AND,
	IR_XOR,
	IR_EQ,
	IR_NE,

	IR_LT,
	IR_LE,
	IR_GT,
	IR_GE,
	IR_MOV,
	IR_STORE,
	IR_STORE32,
	IR_STORE8,
	IR_LOADO,
	IR_LOADO32,

	IR_LOADO8,
	IR_LOAD,
	IR_LOAD32,
	IR_LOAD8,
	IR_LOADI,
	IR_LOADG,
	IR_KILL,
	IR_ENTER,
	IR_RET,
	IR_CBR,

	IR_JUMP,
	IR_LABEL,
	IR_CALL,
	IR_ALLOC,
	IR_PHI,
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
	int global;
	struct type *type;
	struct node *body;
	struct param *params;
	struct symtab *tab;
	char *str;
	struct func *func;
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
	int sub;
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

struct func {
	struct bb *bb;
	struct cfg_edge *edge;
	int nr_bbs;
	int nr_edges;
	int nr_temps;
	struct set *globals;
	int *rpo;
	struct ir *ir;
};

struct bb {
	int n;
	int succ;
	int pred;
	int visited;
	struct ir *ir;
	int po;
	int idom;
	int domsucc;
	int domsib;
	struct set *df;
	int nr_ir;
};

struct cfg_edge {
	int src;
	int sink;
	int next_succ;
	int next_pred;
};

#define FOREACH_PRED(f, b, e, p) for (e =  (b)->pred, p = \
    &(f)->bb[(f)->edge[e].src]; e != -1; e = f->edge[e].next_pred, p = \
    &(f)->bb[(f)->edge[e].src])
#define FOREACH_SUCC(f, b, e, p) for (e = (b)->succ, p = \
    &(f)->bb[(f)->edge[e].sink]; e != -1; e = (f)->edge[e].next_succ, p = \
    &(f)->bb[f->edge[e].sink])

struct set {
	int n;
	char *bits;
};

void lex(FILE *f);

struct type *new_type(int size);
void parse(void);

struct ir *new_ir(int op, struct ir_oprnd *o1, struct ir_oprnd *o2,
    struct ir_oprnd *dst);
void dump_ir_op(FILE *f, struct ir *ir);
void dump_ir(struct ir *ir);
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
int new_label(void);

void opt(void);

struct set *new_set(int n);
int set_has(struct set *s, int n);
void set_add(struct set *s, int n);
void set_del(struct set *s, int n);
int set_empty(struct set *s);

void make_ssa(struct func *f);

#endif
