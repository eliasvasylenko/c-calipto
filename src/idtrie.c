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
	// awkward trick to avoid branching. There's probably a better way.
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

void popclear(uint64_t* p) {
	p[0] = p[1] = p[2] = p[3] = 0;
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
	l.branch = (idtrie_branch*)(l.leaf + n->hasleaf);
	l.children = (idtrie_node**)((uint8_t*)l.branch + offsetof(idtrie_branch, children));
	l.end = n->hasbranch
		? (void*)(l.children + popcount(l.branch->population))
		: (void*)l.branch;

	return l;
}

idtrie_value value_of(idtrie_node* leaf) {
	return (idtrie_value){ leaf, ((idtrie_leaf*)(data_of(leaf) + leaf->size))->value };
}

typedef struct key {
	uint32_t size;
	uint8_t* data;
} key;

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

idtrie_node* make_leaf(idtrie* t, key k, key tail, idtrie_node* parent) {
	idtrie_node* leaf = malloc(sizeof(idtrie_node) + tail.size + sizeof(idtrie_leaf));
	leaf->parent = parent;
	leaf->hasleaf = true;
	leaf->hasbranch = false;
	leaf->size = tail.size;
	memcpy(data_of(leaf), tail.data, tail.size);

	idtrie_leaf* l = (idtrie_leaf*)(data_of(leaf) + tail.size);
	l->value = t->get_value(k.size, k.data, leaf);
	l->key_size = k.size;

	return leaf;
}

idtrie_value split_with_leaf(idtrie* t, key k, idtrie_node** np, uint32_t index) {
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

	popclear(branch->population);
	popadd(branch->population, *data_of(after));
	branch->children[0] = after;

	leaf->value = t->get_value(k.size, k.data, before);
	leaf->key_size = k.size;

	return (idtrie_value){ before, leaf->value };
}

idtrie_value split_with_branch(idtrie* t, key k, idtrie_node** np, uint32_t index, key tail) {
	idtrie_node* n = *np;

	idtrie_node* before = malloc(sizeof(idtrie_node) + index + sizeof_branch(2));
	before->parent = n->parent;
	before->hasleaf = false;
	before->hasbranch = true;
	before->size = index;
	memcpy(data_of(before), data_of(n), index);

	idtrie_node* leaf = make_leaf(t, k, tail, before);
	idtrie_node* after = make_split(t, n, index, before);

	free(*np);
	*np = before;

	idtrie_branch* branch = (idtrie_branch*)(data_of(before) + index);

	popclear(branch->population);
	popadd(branch->population, *data_of(after));
	popadd(branch->population, *data_of(leaf));
	if (data_of(leaf) > data_of(after)) {
		branch->children[0] = after;
		branch->children[1] = leaf;
	} else {
		branch->children[0] = leaf;
		branch->children[1] = after;
	}

	return value_of(leaf);
}

key key_tail(key k, uint32_t head) {
	k.size -= head;
	k.data = (uint8_t*)k.data + head;
	return k;
}

idtrie_value insert_recur(idtrie* t, key k, idtrie_node** np, key tail) {
	idtrie_node* n = *np;

	if (tail.size < n->size) {
		for (int i = 0; i < tail.size; i++) {
			if (data_of(n)[i] != tail.data[i]) {
				return split_with_branch(t, k, np, i, key_tail(tail, i));
			}
		}

		return split_with_leaf(t, k, np, tail.size);
	}

	for (int i = 0; i < n->size; i++) {
		if (data_of(n)[i] != tail.data[i]) {
			return split_with_branch(t, k, np, i, key_tail(tail, i));
		}
	}

	if (tail.size == n->size) {
		n->hasleaf = true;
		return value_of(n);
	}

	if (!n->hasbranch) {
		insert_branch();

		assert(false);
	}

	node_layout layout = layout_of(n);
	tail = key_tail(tail, n->size);

	uint8_t pi = popindex(layout.branch->population, tail.data[0]);
	idtrie_node** child = &layout.branch->children[pi];
			
	if (data_of(*child)[0] != tail.data[0]) {
		insert_branch();

		assert(false);
	} else {
		return insert_recur(t, k, child, tail);
	}
}

idtrie_value idtrie_insert(idtrie* t, uint32_t key_size, void* key_data) {
	key k = { key_size, key_data };

	if (t->root == NULL ) {
		t->root = make_leaf(t, k, k, NULL);
		return value_of(t->root);
	} else {
		return insert_recur(t, k, &t->root, k);
	}
}

void idtrie_delete(idtrie_node* n) {
}

void key_data_recur(uint32_t size, uint8_t* dest, idtrie_node* n) {
	size -= n->size;
	if (n->parent != NULL) {
		key_data_recur(size, dest, n->parent);
	}

	memcpy(dest + size, n + 1, n->size);
}

uint32_t idtrie_key(void* dest, idtrie_node* n) {
	uint32_t size = idtrie_key_size(n);
	if (size > 0) {
		key_data_recur(size, dest, n);
	}
	return size;
}

uint32_t idtrie_key_size(idtrie_node* n) {
	if (!n->hasleaf) {
		return -1;
	}
	return ((idtrie_leaf*)(data_of(n) + n->size))->key_size;
}

idtrie_value find_recur(idtrie* t, key tail, idtrie_node* n) {
	if (tail.size < n->size) {
		return (idtrie_value){ NULL, NULL };
	}

	for (int i = 0; i < n->size; i++) {
		if (data_of(n)[i] != tail.data[i]) {
			return (idtrie_value){ NULL, NULL };
		}
	}

	if (tail.size == n->size) {
		return value_of(n);
	}

	if (!n->hasbranch) {
		return (idtrie_value){ NULL, NULL };
	}

	node_layout layout = layout_of(n);
	tail = key_tail(tail, n->size);

	uint8_t pi = popindex(layout.branch->population, tail.data[0]);
	idtrie_node* child = layout.branch->children[pi];

	return find_recur(t, tail, child);
}

idtrie_value idtrie_find(idtrie* t, uint32_t key_size, void* key_data) {
	key k = { key_size, key_data };
	return find_recur(t, k, t->root);
}

void idtrie_clear(idtrie t) {
	;
}

