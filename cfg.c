#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <assert.h>

#include "rcc.h"

#define	MAX_CFG 1000

static struct bb *bb;
static struct cfg_edge *edge;
static int nr_bbs;
static int nr_edges;
static int nrpo;

static int
find_bb(int n)
{
	int i;

	for (i = 0; i < nr_bbs; i++)
		if (bb[i].n == n)
			return (i);
	errx(1, "couldn't find bb %d\n", n);
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
remove_labels(struct bb *bb)
{
	struct ir *i, *o;
	o = bb->ir;
	if (o->op == IR_LABEL) {
		i = o->next;
		free(o);
		bb->ir = i;
		bb->nr_ir--;
	}
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
add_bb(int id, int n, struct ir *ir)
{
	if (ir) {
		ir->node = id;
		ir->leader = 1;
	}
	bb[id].ir = ir;
	bb[id].n = n;
	bb[id].succ = -1;
	bb[id].pred = -1;
	nr_bbs++;
}

static void
make_cfg(struct func *f)
{
	struct ir *ir, *last[MAX_CFG], *leader[MAX_CFG], *p;
	int i, next, nr;

	ir = remove_kill(f->ir);

	next = 0;
	leader[0] = ir;
	add_bb(next++, -1, ir);
	for (i = 1, ir = ir->next; ir; ir = ir->next, i++) {
		if (ir->op == IR_LABEL) {
			leader[next] = ir;
			add_bb(next++, ir->o1.v, ir);
		}
	}

	for (i = 0; i < next; i++) {
		p = leader[i];
		ir = leader[i]->next;
		nr = 1;
		while (ir && !ir->leader) {
			p = ir;
			nr++;
			if (ir->op == IR_JUMP || ir->op == IR_CBR)
				break;
			ir = ir->next;
		}
		last[i] = p;
		bb[i].nr_ir = nr;
		if (p->op == IR_CBR) {
			add_edge(i, find_bb(p->o2.v));
			add_edge(i, find_bb(p->dst.v));
		} else if (p->op == IR_JUMP)
			add_edge(i, find_bb(p->dst.v));
	}
}

static int
intersect(struct func *f, int b1, int b2)
{
	int f1, f2;

	f1 = b1;
	f2 = b2;
	while (f1 != f2) {
		while (f->bb[f1].po < f->bb[f2].po)
			f1 = f->bb[f1].idom;
		while (f->bb[f2].po < f->bb[f1].po)
			f2 = f->bb[f2].idom;
	}
	return (f1);
}

static void
get_doms(struct func *f)
{
	struct bb *p, *pred;
	int b, ch, i, j, new_idom;

	for (i = 0; i < nr_bbs; i++) {
		f->bb[i].idom = -1;
		f->bb[i].domsucc = -1;
		f->bb[i].domsib = -1;
	}

	f->bb[0].idom = 0;
	ch = 1;
	while (ch) {
		ch = 0;
		for (i = 1; i < nr_bbs; i++) {
			b = f->rpo[i];
			pred = NULL;
			FOREACH_PRED(f, &f->bb[b], j, p) {
				if (p->idom != -1) {
					new_idom = f->edge[j].src;
					pred = p;
					break;
				}
			}
			FOREACH_PRED(f, &f->bb[b], j, p) {
				if (p == pred)
					continue;
				if (p->idom != -1)
					new_idom = intersect(f, f->edge[j].src,
					    new_idom);
			}
			if (new_idom != f->bb[b].idom) {
				f->bb[b].idom = new_idom;
				ch = 1;
			}
		}
	}

	/* Dominator tree. */
	for (i = 1; i < f->nr_bbs; i++) {
		j = f->bb[i].idom;
		if (f->bb[j].domsucc != -1) {
			f->bb[i].domsib = f->bb[j].domsucc;
			f->bb[j].domsucc = i;
		} else
			f->bb[j].domsucc = i;
	}
}

static void
get_df(struct func *f)
{
	struct bb *p;
	int i, j, r;

	for (i = 0; i < f->nr_bbs; i++)
		f->bb[i].df = new_set(f->nr_bbs);

	for (i = 0; i < nr_bbs; i++) {
		/* Multiple preds. */
		if (f->bb[i].pred != -1 && f->edge[f->bb[i].pred].next_pred !=
		    -1) {
			FOREACH_PRED(f, &f->bb[i], j, p) {
				r = f->edge[j].src;
				while (r != f->bb[i].idom) {
					set_add(f->bb[r].df, i);
					r = f->bb[r].idom;
				}
			}
		}
	}
}

static int
get_rpo(struct func *f, int n, int po)
{
	int i;

	f->bb[n].visited = 1;
	for (i = f->bb[n].succ; i != -1; i = f->edge[i].next_succ)
		if (!f->bb[f->edge[i].sink].visited)
			po = get_rpo(f, f->edge[i].sink, po);
	assert(nrpo >= 0);
	f->bb[n].po = po;
	f->rpo[nrpo--] = n;
	return (po + 1);
}

void
opt(void)
{
	struct func *func;
	int i, j;

	for (i = 0; i < SYMTAB_SIZE; i++) {
		if (symtab->tab[i] && symtab->tab[i]->body) {
			func = symtab->tab[i]->func;
			bb = malloc(MAX_CFG * sizeof(struct bb));
			memset(bb, 0, MAX_CFG * sizeof(struct bb));
			edge = malloc(MAX_CFG * sizeof(struct cfg_edge));
			memset(edge, 0, MAX_CFG * sizeof(struct cfg_edge));
			make_cfg(func);

			func->nr_edges = nr_edges;
			func->nr_bbs = nr_bbs;
			func->bb = malloc(nr_bbs * sizeof(struct bb));
			memcpy(func->bb, bb, nr_bbs * sizeof(struct bb));
			func->edge = malloc(nr_edges * sizeof(struct
			    cfg_edge));
			memcpy(func->edge, edge, nr_edges * sizeof(struct
			    cfg_edge));
			free(bb);
			free(edge);

			for (j = 0; j < func->nr_bbs; j++)
				remove_labels(&func->bb[j]);

			nrpo = func->nr_bbs - 1;
			func->rpo = malloc(nrpo * sizeof(int));
			get_rpo(func, 0, 0);

			get_doms(func);
			get_df(func);
			make_ssa(symtab->tab[i]->func);
		}
	}
}
