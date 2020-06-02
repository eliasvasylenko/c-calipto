#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "c-calipto/idtrie.h"
#include "c-calipto/libpopcnt.h"

static const uint64_t slot_bit = 1;
static const uint64_t slot_mask = (int64_t) -1;

typedef struct cursor {
	idtrie_node node;
	idtrie_node** pointer;

	uint8_t* data_start;
	union {
		uint8_t* data_end;
		idtrie_branch* branch;
	};
	union {
		void* branch_end;
		idtrie_leaf* leaf;
	};
	void* end;
} cursor;

void update_cursor(cursor* c) {
	c->node = **c->pointer;
	c->data_start = (uint8_t*)(*c->pointer + 1);
	c->data_end = c->data_start + c->node.size;
	c->branch_end = c->node.hasbranch ? c->data_end + sizeof(idtrie_branch) : c->data_end;
	c->end = c->node.hasleaf ? c->branch_end + sizeof(idtrie_leaf*) : c->branch_end;
}

cursor make_cursor(idtrie_node** n) {
	cursor c;
	c.pointer = n;
	update_cursor(&c);
	return c;
}

idtrie_node* split(cursor c, uint64_t index, idtrie_node* parent) {
	idtrie_node* after = malloc(c.end - (void*)*c.pointer - index);
	after->parent = parent;
	after->layout = c.node.layout;
	after->size -= index;
	memcpy(after + sizeof(idtrie_node), c.data_start + index, (uint8_t*)c.end - c.data_start - index);
	for (idtrie_node** child = &c.branch->children[0]; child < (idtrie_node**)c.branch_end; child++) {
		(**child).parent = after;
	}
	if (c.node.hasleaf) {
		c.leaf->owner = after;
	}

	return after;
}

bool haspop(uint64_t* p, uint8_t slot) {
	return slot_bit << (slot) & p[0]
		|| slot_bit << (slot - 64) & p[1]
		|| slot_bit << (slot - 128) & p[2]
		|| slot_bit << (slot - 192) & p[3];
}

void addpop(uint64_t* p, uint8_t slot) {
	p[0] = slot_bit << (slot) | p[0];
	p[1] = slot_bit << (slot - 64) | p[1];
	p[2] = slot_bit << (slot - 128) | p[2];
	p[3] = slot_bit << (slot - 192) | p[3];
}

id insert_leaf(cursor c, uint64_t index) {
	idtrie_node* before = malloc(sizeof(idtrie_node) + index + sizeof(idtrie_branch) + sizeof(idtrie_leaf*));
	before->parent = c.node.parent;
	before->hasleaf = true;
	before->hasbranch = true;
	before->size = index;
	memcpy(before + sizeof(idtrie_node), c.data_start, index);

	idtrie_node* after = split(c, index, before);

	// free previous
	*c.pointer = before;
	update_cursor(&c);

	addpop(c.branch->population, *(c.data_start + index));
	c.branch->children[0] = after;

	c.leaf = malloc(sizeof(idtrie_leaf));
	c.leaf->owner = before;
	return (id){ c.leaf };
}

id insert_branch(cursor c, uint64_t index, uint64_t len, uint8_t* data) {
	idtrie_node* before = malloc(sizeof(idtrie_node) + index + sizeof(idtrie_branch) + sizeof(idtrie_node*));
	before->parent = c.node.parent;
	before->hasleaf = false;
	before->hasbranch = true;
	before->size = index;
	memcpy(before + sizeof(idtrie_node), c.data_start, index);

	idtrie_node* leaf = malloc(sizeof(idtrie_node) + len + sizeof(idtrie_leaf*));
	leaf->parent = before;
	leaf->hasleaf = true;
	leaf->hasbranch = false;
	leaf->size = len;
	memcpy(leaf + sizeof(idtrie_node), data, len);

	idtrie_node* after = split(c, index, before);

	*c.pointer = before;
	update_cursor(&c);

	addpop(c.branch->population, *(c.data_start + index));
	addpop(c.branch->population, *data);
	if (*data > *(c.data_start + index)) {
		c.branch->children[0] = after;
		c.branch->children[1] = leaf;
	} else {
		c.branch->children[0] = leaf;
		c.branch->children[1] = after;
	}

	idtrie_leaf* l = (void*)(leaf + 1) + len;
	l = malloc(sizeof(idtrie_leaf));
	l->owner = leaf;
	return (id){ l };
}

id idtrie_intern_recur(cursor c, uint64_t l, uint8_t* d) {
	if (l <= c.node.size) {
		for (int i = 0; i < l; i++) {
			if (c.data_start[i] != d[i]) {
				return insert_branch(c, i, l - i, d + i);
			}
		}
		if (l < c.node.size) {
			return insert_leaf(c, l);

		} else {
			(**c.pointer).hasleaf = true;
			return (id){ c.leaf };
		}

	} else {
		for (int i = 0; i < c.node.size; i++) {
			if (c.data_start[i] != d[i]) {
				return insert_branch(c, i, l - i, d + i);
			}
		}
		// find branch and recur into it, or add branch
	}
}

void idtrie_clear(idtrie t) {
	;
}

id idtrie_insert(idtrie t, uint64_t l, void* d) {
	return idtrie_intern_recur(make_cursor(t.root), l, d);
}

void* idtrie_fetch(id i) {
	;
}

void idtrie_ref(id i) {
}

void idtrie_free(id i) {
}

