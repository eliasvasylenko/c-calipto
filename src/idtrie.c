#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

#include "c-calipto/idtrie.h"
#include "c-calipto/libpopcnt.h"

static const uint64_t slot_bit = 1;
static const uint64_t slot_mask = (int64_t) -1;

uint8_t pop(uint64_t* p) {
	popcnt(p, 16);
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

typedef struct cursor {
	idtrie_node node;
	idtrie_node** pointer;

	uint8_t* data_start;
	union {
		void* data_end;
		idtrie_leaf* leaf;
	};
	idtrie_branch* branch;
	void* end;
} cursor;

size_t sizeof_branch(uint8_t population) {
	return offsetof(idtrie_branch, children) + sizeof(idtrie_node*) * population;
}

void update_cursor(cursor* c) {
	c->node = **c->pointer;
	c->data_start = (uint8_t*)(*c->pointer + 1);
	c->data_end = c->data_start + c->node.size;
	c->branch =  + c->node.hasleaf ? sizeof(idtrie_leaf) + c->data_end : c->data_end;
	c->end = c->node.hasbranch
		? sizeof_branch(pop(c->branch->population)) + c->branch
		: c->branch;
}

cursor make_cursor(idtrie_node** n) {
	cursor c;
	c.pointer = n;
	update_cursor(&c);
	return c;
}

idtrie_node* make_split(idtrie* t, cursor c, uint32_t index, idtrie_node* parent) {
	idtrie_node* after = malloc(c.end - (void*)*c.pointer - index);
	after->parent = parent;
	after->layout = c.node.layout;
	after->size -= index;
	memcpy(after + sizeof(idtrie_node), c.data_start + index, (uint8_t*)c.end - c.data_start - index);
	for (idtrie_node** child = &c.branch->children[0]; child < (idtrie_node**)c.end; child++) {
		(**child).parent = after;
	}
	if (c.node.hasleaf) {
		t->update_value(c.leaf, after);
	}

	return after;
}

idtrie_node* make_leaf(idtrie* t, idtrie_node* p, uint32_t len, uint8_t* data, uint32_t ksize, void* k) {
	idtrie_node* leaf = malloc(sizeof(idtrie_node) + len + sizeof(idtrie_leaf));
	leaf->parent = p;
	leaf->hasleaf = true;
	leaf->hasbranch = false;
	leaf->size = len;
	memcpy((uint8_t*)leaf + sizeof(idtrie_node), data, len);

	idtrie_leaf* l = (idtrie_leaf*)((uint8_t*)(leaf + 1) + len);
	l->data = t->get_value(k, leaf);
	l->size = ksize;

	return leaf;
}

idtrie_leaf insert_leaf(idtrie* t, cursor c, uint32_t index, uint32_t ksize, void* k) {
	idtrie_node* before = malloc(sizeof(idtrie_node) + index + sizeof(idtrie_leaf) + sizeof_branch(1));
	before->parent = c.node.parent;
	before->hasleaf = true;
	before->hasbranch = true;
	before->size = index;
	memcpy(before + sizeof(idtrie_node), c.data_start, index);

	idtrie_node* after = make_split(t, c, index, before);

	// free previous
	*c.pointer = before;
	update_cursor(&c);

	addpop(c.branch->population, *(c.data_start + index));
	c.branch->children[0] = after;

	c.leaf->data = t->get_value(k, before);
	c.leaf->size = ksize;

	return *c.leaf;
}

idtrie_leaf insert_branch(idtrie* t, cursor c, uint32_t index, uint32_t len, uint8_t* data, uint32_t ksize, void* k) {
	idtrie_node* before = malloc(sizeof(idtrie_node) + index + sizeof_branch(2));
	before->parent = c.node.parent;
	before->hasleaf = false;
	before->hasbranch = true;
	before->size = index;
	memcpy(before + sizeof(idtrie_node), c.data_start, index);

	idtrie_node* leaf = make_leaf(t, before, len, data, ksize, k);

	idtrie_node* after = make_split(t, c, index, before);

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

	return *(idtrie_leaf*)((uint8_t*)(leaf + 1) + len);
}

idtrie_leaf idtrie_insert_recur(idtrie* t, cursor c, uint32_t l, uint8_t* d, uint32_t ksize, void* k) {
	if (l <= c.node.size) {
		for (int i = 0; i < l; i++) {
			if (c.data_start[i] != d[i]) {
				return insert_branch(t, c, i, l - i, d + i, ksize, k);
			}
		}

		if (l < c.node.size) {
			return insert_leaf(t, c, l, ksize, k);

		}

		(**c.pointer).hasleaf = true;
		return *c.leaf;

	} else {
		for (int i = 0; i < c.node.size; i++) {
			if (c.data_start[i] != d[i]) {
				return insert_branch(t, c, i, l - i, d + i, ksize, k);
			}
		}

		assert(false);
		// TODO find branch and recur into it, or add branch
	}
}

void idtrie_clear(idtrie t) {
	;
}

idtrie_leaf idtrie_insert(idtrie* t, uint32_t l, void* d) {
	if (t->root == NULL ) {
		t->root = make_leaf(t, NULL, l, d, l, d);
		return *(idtrie_leaf*)((uint8_t*)(t->root + 1) + l);
	} else {
		return idtrie_insert_recur(t, make_cursor(&t->root), l, d, l, d);
	}
}

void idtrie_remove(idtrie_node* n) {
}

void idtrie_fetch_key_recur(uint32_t size, uint8_t* dest, idtrie_node* n) {
	size -= n->size;
	if (n->parent != NULL) {
		idtrie_fetch_key_recur(size, dest, n->parent);
	}

	memcpy(dest + size, n + 1, n->size);
}

uint32_t idtrie_fetch_key(void* dest, idtrie_node* n) {
	uint32_t size = idtrie_key_size(n);
	if (size > 0) {
		idtrie_fetch_key_recur(size, dest, n);
	}
	return size;
}

uint32_t idtrie_key_size(idtrie_node* n) {
	if (!n->hasleaf) {
		return -1;
	}

	return ((idtrie_leaf*)((uint8_t*)(n + 1) + n->size))->size;
}

idtrie_leaf idtrie_fetch_recur(idtrie* t, cursor c, uint32_t l, uint8_t* d) {
	if (l <= c.node.size) {
		for (int i = 0; i < l; i++) {
			if (c.data_start[i] != d[i]) {
				return (idtrie_leaf){ -1, NULL };
			}
		}

		if (l < c.node.size || !(**c.pointer).hasleaf) {
			return (idtrie_leaf){ -1, NULL };
		}

		return *c.leaf;

	} else {
		for (int i = 0; i < c.node.size; i++) {
			if (c.data_start[i] != d[i]) {
				return (idtrie_leaf){ -1, NULL };
			}
		}
		// find branch and recur into it, or add branch
	}
}

idtrie_leaf idtrie_fetch(idtrie* t, uint32_t l, void* d) {
	return idtrie_fetch_recur(t, make_cursor(&t->root), l, d);
}

