#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

#include <stdio.h>

#include "c-calipto/idtrie.h"
#include "c-calipto/libpopcnt.h"

static const uint64_t slot_bit = 1;
static const uint64_t slot_mask = (int64_t) -1;

uint8_t popcount(const uint64_t* p) {
	return popcnt64_unrolled(p, 4);
}

bool pophas(const uint64_t* p, uint8_t slot) {
	return p[slot / 64] & slot_bit << (slot % 64);
}

void popadd(uint64_t* p, uint8_t slot) {
	p[slot / 64] |= slot_bit << (slot % 64);
}

uint8_t popindex(const uint64_t* p, uint8_t slot) {
	static const uint64_t toggle_mask[] = { 0, (int64_t) -1 };
	uint8_t block = slot / 64;

	uint64_t m[4];
	m[0] = p[0];
	m[1] = p[1] & toggle_mask[block >= 1];
	m[2] = p[2] & toggle_mask[block >= 2];
	m[3] = p[3] & toggle_mask[block >= 3];
	m[block] &= ~(slot_mask << (slot % 64));

	return popcount(m);
}

typedef struct node_layout {
	uint8_t* data;
	union {
		uint8_t* data_end;
		idtrie_leaf* leaf;
	};
	idtrie_branch* branch;
	idtrie_node** children;
	union {
		idtrie_node** children_end;
		void* end;
	};
} node_layout;

size_t sizeof_branch(uint8_t population) {
	return offsetof(idtrie_branch, children) + sizeof(idtrie_node*) * population;
}

uint8_t* data_of(idtrie_node* n) {
	return (uint8_t*)(n + 1);
}

node_layout layout_of(idtrie_node* n) {
	node_layout l;
	l.data = (uint8_t*)(n + 1);
	l.leaf = (idtrie_leaf*)(l.data + n->size);
	l.branch = n->hasleaf
		? (idtrie_branch*)(l.leaf + 1)
		: (idtrie_branch*)l.leaf;
	l.children = (idtrie_node**)((uint8_t*)l.branch + offsetof(idtrie_branch, children));
	l.end = n->hasbranch
		? (void*)(l.children + popcount(l.branch->population))
		: (void*)l.branch;

	return l;
}

idtrie_node* make_split(idtrie* t, idtrie_node* n, uint32_t index, idtrie_node* parent) {
	node_layout l = layout_of(n);

	ptrdiff_t size = (uint8_t*)l.end - (uint8_t*)n;

	idtrie_node* after = malloc(size - index);
	after->parent = parent;
	after->layout = n->layout;
	after->size -= index;
	memcpy(data_of(after), l.data + index, l.data_end - l.data - index);
	for (idtrie_node** child = l.children; child < l.children_end; child++) {
		(**child).parent = after;
	}
	if (n->hasleaf) {
		t->update_value(l.leaf, after);
	}

	return after;
}

idtrie_node* make_leaf(idtrie* t, idtrie_node* p, uint32_t len, uint8_t* data, uint32_t ksize, void* k) {
	idtrie_node* leaf = malloc(sizeof(idtrie_node) + len + sizeof(idtrie_leaf));
	leaf->parent = p;
	leaf->hasleaf = true;
	leaf->hasbranch = false;
	leaf->size = len;
	memcpy(data_of(leaf), data, len);

	idtrie_leaf* l = (idtrie_leaf*)(data_of(leaf) + len);
	l->data = t->get_value(k, leaf);
	l->size = ksize;

	return leaf;
}

idtrie_leaf insert_leaf(idtrie* t, idtrie_node** np, uint32_t index, uint32_t ksize, void* k) {
	idtrie_node* n = *np;

	idtrie_node* before = malloc(sizeof(idtrie_node) + index + sizeof(idtrie_leaf) + sizeof_branch(1));
	before->parent = n->parent;
	before->hasleaf = true;
	before->hasbranch = true;
	before->size = index;
	memcpy(data_of(before), data_of(n), index);

	idtrie_node* after = make_split(t, n, index, before);

	free(*np);
	*np = before;

	idtrie_leaf* leaf = (idtrie_leaf*)(data_of(before) + index);
	idtrie_branch* branch = (idtrie_branch*)(leaf + 1);

	popadd(branch->population, *data_of(after));
	branch->children[0] = after;

	leaf->data = t->get_value(k, before);
	leaf->size = ksize;

	return *leaf;
}

idtrie_leaf insert_branch(idtrie* t, idtrie_node** np, uint32_t index, uint32_t len, uint8_t* data, uint32_t ksize, void* k) {
	idtrie_node* n = *np;

	idtrie_node* before = malloc(sizeof(idtrie_node) + index + sizeof_branch(2));
	before->parent = n->parent;
	before->hasleaf = false;
	before->hasbranch = true;
	before->size = index;
	memcpy(before + 1, n + 1, index);

	idtrie_node* leaf = make_leaf(t, before, len, data, ksize, k);
	idtrie_node* after = make_split(t, n, index, before);

	free(*np);
	*np = before;

	idtrie_branch* branch = (idtrie_branch*)(data_of(before) + index);

	popadd(branch->population, *data_of(after));
	popadd(branch->population, *data_of(leaf));
	if (data_of(leaf) > data_of(after)) {
		branch->children[0] = after;
		branch->children[1] = leaf;
	} else {
		branch->children[0] = leaf;
		branch->children[1] = after;
	}

	return *(idtrie_leaf*)((uint8_t*)(leaf + 1) + len);
}

idtrie_leaf idtrie_insert_recur(idtrie* t, idtrie_node** np, uint32_t l, uint8_t* d, uint32_t ksize, void* k) {
	idtrie_node* n = *np;

	if (l <= n->size) {
		for (int i = 0; i < l; i++) {
			if (data_of(n)[i] != d[i]) {
				return insert_branch(t, np, i, l - i, d + i, ksize, k);
			}
		}

		if (l < n->size) {
			return insert_leaf(t, np, l, ksize, k);

		}

		n->hasleaf = true;
		return *(idtrie_leaf*)(data_of(n) + n->size);

	} else {
		for (int i = 0; i < n->size; i++) {
			if (data_of(n)[i] != d[i]) {
				return insert_branch(t, np, i, l - i, d + i, ksize, k);
			}
		}

		// TODO find branch and recur into it, or add branch
		
		l -= n->size;
		d += n->size;

		node_layout layout = layout_of(n);

		if (!n->hasbranch) {
			// TODO add a new branch with our leaf

			assert(false);
		} else {
			uint8_t pi = popindex(layout.branch->population, d[0]);
			printf(" pop index: %i\n", (uint32_t) pi);

			idtrie_node** child = &layout.branch->children[pi];
			
			if (data_of(*child)[0] != d[0]) {
				// TODO add a new branch with our leaf

				assert(false);
			} else {
				return idtrie_insert_recur(t, child, l, d, ksize, k);
			}
		}
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
		return idtrie_insert_recur(t, &t->root, l, d, l, d);
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

idtrie_leaf idtrie_fetch_recur(idtrie* t, idtrie_node* n, uint32_t l, uint8_t* d) {
	if (l <= n->size) {
		for (int i = 0; i < l; i++) {
			if (data_of(n)[i] != d[i]) {
				return (idtrie_leaf){ -1, NULL };
			}
		}

		if (l < n->size || !n->hasleaf) {
			return (idtrie_leaf){ -1, NULL };
		}

		return *(idtrie_leaf*)(data_of(n) + n->size);

	} else {
		for (int i = 0; i < n->size; i++) {
			if (data_of(n)[i] != d[i]) {
				return (idtrie_leaf){ -1, NULL };
			}
		}

		assert(false);
		// TODO find branch and recur into it, or add branch
	}
}

idtrie_leaf idtrie_fetch(idtrie* t, uint32_t l, void* d) {
	return idtrie_fetch_recur(t, t->root, l, d);
}

