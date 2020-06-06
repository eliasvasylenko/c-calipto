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
		void* leaf;
	};
	void* end;
} cursor;

void update_cursor(cursor* c) {
	c->node = **c->pointer;
	c->data_start = (uint8_t*)(*c->pointer + 1);
	c->data_end = c->data_start + c->node.size;
	c->branch_end = c->node.hasbranch ? c->data_end + sizeof(idtrie_branch) : c->data_end;
	c->end = c->node.hasleaf ? c->branch_end + sizeof(void*) : c->branch_end;
}

cursor make_cursor(idtrie_node** n) {
	cursor c;
	c.pointer = n;
	update_cursor(&c);
	return c;
}

idtrie_node* split(idtrie t, cursor c, uint64_t index, idtrie_node* parent) {
	idtrie_node* after = malloc(c.end - (void*)*c.pointer - index);
	after->parent = parent;
	after->layout = c.node.layout;
	after->size -= index;
	memcpy(after + sizeof(idtrie_node), c.data_start + index, (uint8_t*)c.end - c.data_start - index);
	for (idtrie_node** child = &c.branch->children[0]; child < (idtrie_node**)c.branch_end; child++) {
		(**child).parent = after;
	}
	if (c.node.hasleaf) {
		t.update_value(c.leaf, after);
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

void* insert_leaf(idtrie t, cursor c, uint64_t index, void* k) {
	idtrie_node* before = malloc(sizeof(idtrie_node) + index + sizeof(idtrie_branch) + sizeof(void*));
	before->parent = c.node.parent;
	before->hasleaf = true;
	before->hasbranch = true;
	before->size = index;
	memcpy(before + sizeof(idtrie_node), c.data_start, index);

	idtrie_node* after = split(t, c, index, before);

	// free previous
	*c.pointer = before;
	update_cursor(&c);

	addpop(c.branch->population, *(c.data_start + index));
	c.branch->children[0] = after;

	c.leaf = t.get_value(k, before);
	return c.leaf;
}

void* insert_branch(idtrie t, cursor c, uint64_t index, uint64_t len, uint8_t* data, void* k) {
	idtrie_node* before = malloc(sizeof(idtrie_node) + index + sizeof(idtrie_branch) + sizeof(idtrie_node*));
	before->parent = c.node.parent;
	before->hasleaf = false;
	before->hasbranch = true;
	before->size = index;
	memcpy(before + sizeof(idtrie_node), c.data_start, index);

	idtrie_node* leaf = malloc(sizeof(idtrie_node) + len + sizeof(void*));
	leaf->parent = before;
	leaf->hasleaf = true;
	leaf->hasbranch = false;
	leaf->size = len;
	memcpy(leaf + sizeof(idtrie_node), data, len);

	idtrie_node* after = split(t, c, index, before);

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

	void** l = (void**)((uint8_t*)(leaf + 1) + len);
	*l = t.get_value(k, leaf);
	return *l;
}

void* idtrie_intern_recur(idtrie t, cursor c, uint64_t l, uint8_t* d, void* k) {
	if (l <= c.node.size) {
		for (int i = 0; i < l; i++) {
			if (c.data_start[i] != d[i]) {
				return insert_branch(t, c, i, l - i, d + i, k);
			}
		}
		if (l < c.node.size) {
			return insert_leaf(t, c, l, k);

		} else {
			(**c.pointer).hasleaf = true;
			return c.leaf;
		}

	} else {
		for (int i = 0; i < c.node.size; i++) {
			if (c.data_start[i] != d[i]) {
				return insert_branch(t, c, i, l - i, d + i, k);
			}
		}
		// find branch and recur into it, or add branch
	}
}

void idtrie_clear(idtrie t) {
	;
}

void* idtrie_insert(idtrie t, uint64_t l, void* d) {
	return idtrie_intern_recur(t, make_cursor(t.root), l, d, d);
}

void idtrie_remove(idtrie_node* n) {
}

void* idtrie_fetch_key(idtrie_node* n) {
}

