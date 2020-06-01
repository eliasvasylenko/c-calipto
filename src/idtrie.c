#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "c-calipto/idtrie.h"
#include "c-calipto/libpopcnt.h"

static const uint64_t slot_bit = 1;
static const uint64_t slot_mask = (int64_t) -1;

typedef struct cursor {
	idtrie_node** node;
	uint8_t* data_start;
	union {
		struct {
			bool
				hasleaf: 1,
				hasbranch: 1;
			uint64_t
				size : 62;
		};
		uint64_t layout;
	};
	union {
		uint8_t* data_end;
		idtrie_branch* branch;
	};
	union {
		void* branch_end;
		id id;
	};
	void* end;
} cursor;

void update_cursor(cursor* c, idtrie_node* n) {
	(*c->node) = n;
	c->layout = (*n).layout;
	c->data_start = (uint8_t*)(n + 1);
	c->data_end = c->data_start + c->size;
	c->branch_end = c->hasbranch ? c->data_end + sizeof(idtrie_branch) : c->data_end;
	c->end = c->hasleaf ? c->branch_end + sizeof(id) : c->branch_end;
}

cursor make_cursor(idtrie_node** n) {
	cursor c;
	c.node = n;
	update_cursor(&c, *n);
	return c;
}



uint8_t* branch_index(cursor c, int8_t* d) {
	;
}

uint64_t sizeof_node(int64_t size, bool leaf, int8_t branches) {
	return sizeof(idtrie_node)
		+ size
		+ branches
			? (sizeof(idtrie_branch) + sizeof(idtrie_node*) * (branches - 1))
			: 0
		+ leaf
			? sizeof(id*)
			: 0;
}

id split(cursor c, uint64_t index) {
	idtrie_node* before = malloc(sizeof_node(index, true, 1));
	before->parent = (**c.node).parent;
	before->hasleaf = 1;
	before->hasbranch = 1;
	before->size = index;
	// copy data

	idtrie_node* after = malloc(c.end - (void*)c.node - index);
	after->parent = before;
	after->hasleaf = (**c.node).hasleaf;
	after->hasbranch = (**c.node).hasbranch;
	after->size = (**c.node).size - index;
	// copy data & branch data

	// free previous

	update_cursor(&c, before);
	uint8_t slot = *(uint8_t*)(after + 1);
	c.branch->population[0] = slot_bit << (slot);
	c.branch->population[1] = slot_bit << (slot - 64);
	c.branch->population[2] = slot_bit << (slot - 128);
	c.branch->population[3] = slot_bit << (slot - 192);
	c.branch->children[0] = after;

	id id = { malloc(sizeof(idtrie_node*)) };
	*(id.node) = before;
	return id;
}

id split_and_branch(cursor c, uint64_t index, uint64_t len, int8_t* data) {
	idtrie_node* before = malloc(sizeof_node(index, false, 2));
	before->parent = (**c.node).parent;
	before->hasleaf = 0;
	before->hasbranch = 1;
	before->size = index;
	// copy data

	idtrie_node* after = malloc(c.end - (void*)c.node - index);
	after->parent = before;
	after->hasleaf = (**c.node).hasleaf;
	after->hasbranch = (**c.node).hasbranch;
	after->size = (**c.node).size - index;
	// copy data & branch data

	// free previous

	update_cursor(&c, before);
	uint8_t slot = *(uint8_t*)(after + 1);
	c.branch->population[0] = slot_bit << (slot);
	c.branch->population[1] = slot_bit << (slot - 64);
	c.branch->population[2] = slot_bit << (slot - 128);
	c.branch->population[3] = slot_bit << (slot - 192);
	c.branch->children[0] = after;

	id id = { malloc(sizeof(idtrie_node*)) };
	*(id.node) = leaf;
	return id;
}

id idtrie_intern_recur(cursor c, uint64_t l, int8_t* d) {
	if (l <= (**c.node).size) {
		for (int i = 0; i < l; i++) {
			if (c.data_start[i] != d[i]) {
				return split_and_branch(c, i, l - i, d + i);
			}
		}
		if (l < n->index) {
			return split(c, l);

		} else {
			c.node->hasleaf = 1;
			return (id){ c.node };
		}

	} else {
		for (int i = 0; i < c.node->size; i++) {
			if (c.data_start[i] != d[i]) {
				return split_and_branch(c, i, l - i, d + i);
			}
		}
		// find branch and recur into it, or add branch
	}
}

id idtrie_intern(idtrie t, uint64_t l, int8_t* d) {
	return idtrie_intern_recur(make_cursor(t.root), l, d);
}

void idtrie_remove(id i) {
}

void* idtrie_fetch(id i) {
	;
}
