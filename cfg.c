#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <assert.h>

#include "rcc.h"

struct cfg_node {
	int n;
	int succ;
	int pred;
	int visited;
	struct ir *ir;
	int po;
	int idom;
};

struct cfg_edge {
	int src;
	int sink;
	int next_succ;
	int next_pred;
};

#define	MAX_CFG 1000

struct cfg_node cfg[MAX_CFG];
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
		if (cfg[i].n == n)
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
	if (cfg[src].succ == -1)
		cfg[src].succ = nr_edges;
	else {
		old = cfg[src].succ;
		edge[nr_edges].next_succ = old;
		cfg[src].succ = nr_edges;
	}
	if (cfg[sink].pred == -1)
		cfg[sink].pred = nr_edges;
	else {
		old = cfg[sink].pred;
		edge[nr_edges].next_pred = old;
		cfg[sink].pred = nr_edges;
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
	cfg[id].ir = ir;
	cfg[id].n = n;
	cfg[id].succ = -1;
	cfg[id].pred = -1;
	nr_nodes++;
}

static void
make_cfg(struct symbol *s)
{
	struct ir *ir, *last[MAX_CFG], *leader[MAX_CFG], *p;
	int i, next;

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
		while (ir && !ir->leader) {
			p = ir;
			if (ir->op == IR_JUMP || ir->op == IR_CBR)
				break;
			ir = ir->next;
		}
		last[i] = p;
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
		while (cfg[f1].po < cfg[f2].po)
			f1 = cfg[f1].idom;
		while (cfg[f2].po < cfg[f1].po)
			f2 = cfg[f2].idom;
	}
	return (f1);
}

/* From "A Simple, Fast Dominance Algorithm" by KD Cooper. */
static void
get_doms(void)
{
	int b, ch, i, j, new_idom;

	for (i = 0; i < nr_nodes; i++) 
		cfg[i].idom = -1;

	cfg[0].idom = 0;
	ch = 1;
	while (ch) {
		ch = 0;
		for (i = 0; i < nr_nodes; i++) {
			b = rpo[i];
			new_idom = edge[cfg[b].pred].src;
			if (cfg[b].pred == -1)
				continue;
			for (j = edge[cfg[b].pred].next_pred; j != -1; j =
			    edge[j].next_pred)
				if (cfg[edge[j].src].idom != -1)
					new_idom = intersect(edge[j].src,
					    new_idom);
			if (new_idom != cfg[b].idom) {
				cfg[b].idom = new_idom;
				ch = 1;
			}
		}
	}
}

static int
get_rpo(int n, int po)
{
	int i;

	cfg[n].visited = 1;
	for (i = cfg[n].succ; i != -1; i = edge[i].next_succ)
		if (!cfg[edge[i].sink].visited)
			po = get_rpo(edge[i].sink, po);
	assert(nrpo >= 0);
	cfg[n].po = po;
	rpo[nrpo--] = n;
	return (po + 1);
}

void
opt(void)
{
	int i, j;

	for (i = 0; i < SYMTAB_SIZE; i++) {
		if (symtab->tab[i] && symtab->tab[i]->body) {
			//make_cfg(symtab->tab[i]);

#if 1
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

			printf("rpo: ");
			for (j = 0; j < nr_nodes; j++)
				printf("%d ", rpo[j]);
			printf("\n");

			get_doms();
		}
	}
}
