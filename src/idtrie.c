#include <stdint.h>
#include <stdbool.h>

#include "c-calipto/idtrie.h"

typedef struct cursor {
	idtrie_node** node;
	int8_t* data_start;
	union {
		int8_t* data_end;
		idtrie_branch* branch;
	};
	void* end;
} cursor;

cursor make_cursor(idtrie_node* n) {
	cursor c;
	c.node = n;
	c.data_start = (uint8_t*)(n + 1);
	c.data_end = data_start + n->size;
	return c;
}

uint8_t* branch_index(cursor c, int8_t* d) {
	;
}

uint64_t sizeof_node(cursor c) {
	return sizeof(idtrie_node)
		+ c.data_end - c.data_start
		+ c.branch != NULL ? sizeof(idtrie_node*) * (popcount(4, c.branch) - 1) : 0;
}

id split(cursor c, uint64_t index) {
	idtrie_node* before = malloc(sizeof(idtrie_node) + size + sizeof(idtrie_branch));
	before->parent = (**c.node).parent;
	before->hasleaf = 1;
	before->hasbranch = 1;
	before->size = index;

	idtrie_node* after = malloc(sizeof_node(c) - index);
	after->parent = before;
	after->hasleaf = (**c.node).hasleaf;
	after->hasbranch = (**c.node).hasbranch;
	after->size = (**c.node).size - index;


	cursor split_c = make_cursor(split);
	split_c->branch.population = 1 << c.data_start[index];
	split_c->branch.children[0] = c.node;
	c.node->parent = split;
	*c.node = split;

	return (id){ split };
}

id split(idtrie_node* node, uint64_t index, uint64_t len, int8_t* data) {
	idtrie_node* parent = n->parent;

	idtrie_node* split = malloc(sizeof(idtrie_node)
			+ index - parent->index
			+ sizeof(idtrie_branch)
			+ sizeof(idtrie_node*));

	split->parent = parent;
	split->index = index;
	split->hasbranch = 1;

	idtrie_branch* pb = parent + parent;
	pb->children[branch_index(pb, data[parent->index])] = split;

	int8_t* d = (void*)(split + 1);
	d -= parent->index;
	for (int i = parent->index; i < index; i++) {
		d[i] = data[i];
	}
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
