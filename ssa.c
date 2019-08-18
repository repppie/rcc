#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <assert.h>

#include "rcc.h"

static void
add_phi(struct bb *bb, int name)
{
	struct ir_oprnd dst = { name, IRO_TEMP };

	last_ir = NULL;
	new_ir(IR_PHI, NULL, NULL, &dst);
	last_ir->next = bb->ir;
	bb->ir = last_ir;
	bb->nr_ir++;
}

static int
has_phi(struct bb *bb, int name)
{
	struct ir *ir;
	int i;

	for (i = 0, ir = bb->ir; i < bb->nr_ir && ir->op == IR_PHI; i++, ir =
	    ir->next)
		if (ir->dst.type == IRO_TEMP && ir->dst.v == name)
			return (1);
	return (0);
}

void
make_ssa(struct func *f)
{
	struct set **blocks, *varkill, *work;
	struct ir *ir;
	int b, d, i, n;

	blocks = malloc(f->nr_temps * sizeof(struct set *));
	for (i = 0; i < f->nr_temps; i++)
		blocks[i] = new_set(f->nr_edges);

	f->globals = new_set(f->nr_temps);

	for (i = 0; i < f->nr_bbs; i++) {
		varkill = new_set(f->nr_temps);
		for (n = 0, ir = f->bb[i].ir; n < f->bb[i].nr_ir; n++, ir =
		    ir->next) {
			if (ir->o1.type == IRO_TEMP && !set_has(varkill,
			    ir->o1.v))
				set_add(f->globals, ir->o1.v);
			if (ir->o2.type == IRO_TEMP && !set_has(varkill,
			    ir->o2.v))
				set_add(f->globals, ir->o2.v);
			if (ir->op == IR_CALL) {
				struct param *pm;

				for (pm = (struct param *)ir->o2.v; pm; pm =
				    pm->next)
					if (!set_has(varkill, pm->val))
						set_add(f->globals, pm->val);
			}
			if (ir->dst.type != IRO_TEMP || (ir->op >= IR_STORE &&
			    ir->op <= IR_STORE8))
				continue;
			set_add(varkill, ir->dst.v);
			set_add(blocks[ir->dst.v], i);
		}
	}

	for (i = 0; i < f->nr_temps; i++) {
		if (!set_has(f->globals, i))
			continue;
		work = blocks[i];
		while (!set_empty(work)) {
			for (b = 0; b < f->nr_bbs; b++) {
				if (!set_has(work, b))
					continue;
				set_del(work, b);
				for (d = 0; d < f->nr_bbs; d++) {
					if (!set_has(f->bb[b].df, d))
						continue;
					if (!has_phi(&f->bb[d], i)) {
					        add_phi(&f->bb[d], i);
					        set_add(work, d);
					}
				}
			}
		}
	}

	int j;
	struct ir *_ir;
	for (i = 0; i < f->nr_bbs; i++) {
		printf("%d: \n", f->bb[i].n);
		for (j = 0, _ir = f->bb[i].ir; j < f->bb[i].nr_ir; j++,
		    _ir = _ir->next)
			dump_ir_op(stdout, _ir);
	}
}

