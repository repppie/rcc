#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <assert.h>

#include "rcc.h"

struct bb {
	int n;
	int succ;
	int pred;
	int visited;
	struct ir *ir;
	int po;
	int idom;
	struct set *df;
	int nr_ir;
};

struct cfg_edge {
	int src;
	int sink;
	int next_succ;
	int next_pred;
};

#define	FOREACH_PRED(b, e, p) for (e =  bb[b].pred; e != -1; e = \
    edge[e].next_pred, p = &bb[edge[e].src])
#define	FOREACH_SUCC(b, e, p) for (v = bb[b].succ; e != -1; e = \
    edge[e].next_succ, p = &bb[edge[e].sink])

#define	MAX_CFG 1000

struct bb bb[MAX_CFG];
struct cfg_edge edge[MAX_CFG];
int nr_nodes;
int nr_edges;

int rpo[MAX_CFG];
int nrpo;


static int
find_node(int n)
{
	int i;

	for (i = 0; i < nr_nodes; i++)
		if (bb[i].n == n)
			return (i);
	errx(1, "couldn't find node %d\n", n);
	return (-1);
}

static struct ir *
remove_kill(struct ir *ir)
{
	struct ir *h, *n, *p;

	h = p = malloc(sizeof(struct ir));
	memcpy(p, ir, sizeof(struct ir));

	for (ir = ir->next; ir; ir = ir->next) {
		if (ir->op == IR_KILL)
			continue;
		n = malloc(sizeof(struct ir));
		memcpy(n, ir, sizeof(struct ir));
		p->next = n;
		p = n;
	}
	return (h);
}

static void
add_edge(int src, int sink)
{
	int old;

	edge[nr_edges].src = src;
	edge[nr_edges].sink = sink;
	edge[nr_edges].next_succ = -1;
	edge[nr_edges].next_pred = -1;
	if (bb[src].succ == -1)
		bb[src].succ = nr_edges;
	else {
		old = bb[src].succ;
		edge[nr_edges].next_succ = old;
		bb[src].succ = nr_edges;
	}
	if (bb[sink].pred == -1)
		bb[sink].pred = nr_edges;
	else {
		old = bb[sink].pred;
		edge[nr_edges].next_pred = old;
		bb[sink].pred = nr_edges;
	}
	nr_edges++;
}

static void
add_node(int id, int n, struct ir *ir)
{
	if (ir) {
		ir->node = id;
		ir->leader = 1;
	}
	bb[id].ir = ir;
	bb[id].n = n;
	bb[id].succ = -1;
	bb[id].pred = -1;
	nr_nodes++;
}

static void
make_cfg(struct symbol *s)
{
	struct ir *ir, *last[MAX_CFG], *leader[MAX_CFG], *p;
	int i, next, nr;

	ir = remove_kill(s->ir);
	dump_ir(ir);

	next = 0;
	leader[0] = ir;
	add_node(next++, -1, ir);
	for (i = 1, ir = ir->next; ir; ir = ir->next, i++) {
		if (ir->op == IR_LABEL) {
			leader[next] = ir;
			add_node(next++, ir->o1, ir);
		}
	}

	for (i = 0; i < next - 1; i++) {
		p = leader[i];
		ir = leader[i]->next;
		nr = 1;
		while (ir && !ir->leader) {
			p = ir;
			if (ir->op == IR_JUMP || ir->op == IR_CBR)
				break;
			nr++;
			ir = ir->next;
		}
		last[i] = p;
		bb[i].nr_ir = nr;
		if (p->op == IR_CBR) {
			add_edge(i, find_node(p->o2));
			add_edge(i, find_node(p->dst));
		} else if (p->op == IR_JUMP)
			add_edge(i, find_node(p->dst));
	}
}

static int
intersect(int b1, int b2)
{
	int f1, f2;

	f1 = b1;
	f2 = b2;
	while (f1 != f2) {
		while (bb[f1].po < bb[f2].po)
			f1 = bb[f1].idom;
		while (bb[f2].po < bb[f1].po)
			f2 = bb[f2].idom;
	}
	return (f1);
}

static void
get_doms(void)
{
	struct bb *p;
	int b, ch, i, j, new_idom;

	for (i = 0; i < nr_nodes; i++) 
		bb[i].idom = -1;

	bb[0].idom = 0;
	ch = 1;
	while (ch) {
		ch = 0;
		for (i = 0; i < nr_nodes; i++) {
			b = rpo[i];
			new_idom = edge[bb[b].pred].src;
			FOREACH_PRED(b, j, p) {
				if (j == bb[b].pred)
					continue;
				if (p->idom != -1)
					new_idom = intersect(edge[j].src,
					    new_idom);
			}
			if (new_idom != bb[b].idom) {
				bb[b].idom = new_idom;
				ch = 1;
			}
		}
	}
}

static void
get_df(void)
{
	struct bb *p;
	int i, j, r;

	for (i = 0; i < nr_nodes; i++)
		bb[i].df = new_set(nr_nodes);

	for (i = 0; i < nr_nodes; i++) {
		/* Multiple preds. */
		if (bb[i].pred != -1 && edge[bb[i].pred].next_pred != -1) {
			FOREACH_PRED(i, j, p) {
				r = edge[j].src;
				while (r != bb[i].idom) {
					set_add(bb[r].df, i);
					r = bb[r].idom;
				}
			}
		}
	}
}

static int
get_rpo(int n, int po)
{
	int i;

	bb[n].visited = 1;
	for (i = bb[n].succ; i != -1; i = edge[i].next_succ)
		if (!bb[edge[i].sink].visited)
			po = get_rpo(edge[i].sink, po);
	assert(nrpo >= 0);
	bb[n].po = po;
	rpo[nrpo--] = n;
	return (po + 1);
}

static void
add_phi(struct bb *bb, int name)
{
	printf("adding phi for %d to bb %d\n", name, bb->n);
	last_ir = NULL;
	new_ir(IR_PHI, 0, 0, name);
	last_ir->next = bb->ir;
	bb->ir = last_ir;
}

static int
has_phi(struct bb *bb, int name)
{
	struct ir *ir;
	int i;

	for (i = 0, ir = bb->ir; i < bb->nr_ir && ir->op == IR_PHI; i++, ir =
	    ir->next)
		if (ir->dst == name)
			return (1);
	return (0);
}

static void
get_live(struct symbol *sym)
{
	struct set **blocks, *globals, *varkill, *work;
	struct ir *ir;
	int b, d, i, n;

	blocks = malloc(sym->nr_temps * sizeof(struct set *));
	for (i = 0; i < sym->nr_temps; i++)
		blocks[i] = new_set(nr_edges);

	globals = new_set(sym->nr_temps);
	varkill = new_set(sym->nr_temps);

	for (i = 0; i < nr_nodes; i++) {
		for (n = 0, ir = bb[i].ir; n < bb[i].nr_ir; n++, ir =
		    ir->next) {
			if (ir->op <= IR_GE) {
				printf("2 operands ");
				if (!set_set(varkill, ir->o1))
					set_add(globals, ir->o1);
				if (!set_set(varkill, ir->o2))
					set_add(globals, ir->o2);
			} else if (ir->op <= IR_STORE8) {
				printf("1 operand ");
				if (!set_set(varkill, ir->o1))
					set_add(globals, ir->o1);
			}
			if (ir->op > IR_LOADG)
				continue;
			set_add(varkill, ir->dst);
			dump_ir_op(stdout, ir);
			printf("adding %d to block(%ld)\n", i, ir->dst);
			set_add(blocks[ir->dst], i);
		}
	}

	for (i = 0; i < sym->nr_temps; i++) {
		if (!set_set(globals, i))
			continue;
		work = blocks[i];
		for (b = 0; b < nr_nodes; b++) {
			if (!set_set(work, b))
				continue;
			for (d = 0; d < nr_nodes; d++) {
				if (!set_set(bb[b].df, d))
					continue;
				if (!has_phi(&bb[b], i)) {
					add_phi(&bb[b], i);
					set_add(work, d);
				}
			}
		}
	}
}

void
opt(void)
{
	int i;

	for (i = 0; i < SYMTAB_SIZE; i++) {
		if (symtab->tab[i] && symtab->tab[i]->body) {
			make_cfg(symtab->tab[i]);

#if 0
			for (j = 0; j < 9; j++)
				add_node(j, j, NULL);
			add_edge(0, 1);
			add_edge(1, 2);
			add_edge(1, 5);
			add_edge(2, 3);
			add_edge(5, 6);
			add_edge(5, 8);
			add_edge(6, 7);
			add_edge(8, 7);
			add_edge(7, 3);
			add_edge(3, 4);
			add_edge(3, 1);
#endif
#if 0
			add_node(0, 6, NULL);
			add_node(1, 5, NULL);
			add_node(2, 4, NULL);
			add_node(3, 1, NULL);
			add_node(4, 2, NULL);
			add_node(5, 3, NULL);
			add_edge(0, 1);
			add_edge(0, 2);
			add_edge(1, 3);
			add_edge(2, 4);
			add_edge(2, 5);

			add_edge(3, 4);
			add_edge(4, 3);
			add_edge(4, 5);
			add_edge(5, 4);
#endif

			nrpo = nr_nodes - 1;
			get_rpo(0, 0);
			get_doms();
			get_df();
			get_live(symtab->tab[i]);
		}
	}
}
