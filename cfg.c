#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>

#include "rcc.h"

struct cfg_node {
	int n;
	int succ;
	int pred;
	struct ir *ir;
};

struct cfg_edge {
	int src;
	int sink;
	int next_succ;
	int next_pred;
};

struct cfg_node cfg[1000];
struct cfg_edge edge[1000];
int nr_nodes;
int nr_edges;

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
	ir->node = id;
	ir->leader = 1;
	cfg[id].ir = ir;
	cfg[id].n = n;
	cfg[id].succ = -1;
	cfg[id].pred = -1;
	nr_nodes++;
}

static void
make_cfg(struct symbol *s)
{
	struct ir *ir, *last[1000], *leader[1000], *p;
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

void
opt(void)
{
	int i;

	for (i = 0; i < SYMTAB_SIZE; i++)
		if (symtab->tab[i] && symtab->tab[i]->body)
			make_cfg(symtab->tab[i]);
}
