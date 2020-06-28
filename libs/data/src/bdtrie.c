#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

#include <stdio.h>

#include "c-ohvu/data/bdtrie.h"
#include "libpopcnt.h"

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
		bdtrie_leaf* leaf;
	};
	bdtrie_branch* branch;
	bdtrie_node** children;
	union {
		bdtrie_node** children_end;
		void* end;
	};
} node_layout;

size_t sizeof_branch(uint8_t population) {
	return offsetof(bdtrie_branch, children) + sizeof(bdtrie_node*) * population;
}

uint8_t* data_of(bdtrie_node* n) {
	return (uint8_t*)(n + 1);
}

bdtrie_leaf* leaf_of(bdtrie_node* n) {
	return (bdtrie_leaf*)(data_of(n) + n->keysize);
}

bdtrie_value value_of(bdtrie_node* n) {
	return (bdtrie_value){ n, leaf_of(n)->value };
}

bdtrie_branch* branch_of(bdtrie_node* n) {
	return (bdtrie_branch*)(leaf_of(n) + n->hasleaf);
}

bdtrie_node** children_of(bdtrie_node* n) {
	return (bdtrie_node**)((uint8_t*)branch_of(n) + offsetof(bdtrie_branch, children));
}

void* end_of(bdtrie_node* n) {
	return n->branchsize
		? (void*)children_of(n) + n->branchsize
		: (void*)branch_of(n);
}

node_layout layout_of(bdtrie_node* n) {
	node_layout l;
	l.data = (uint8_t*)(n + 1);
	l.leaf = (bdtrie_leaf*)(l.data + n->keysize);
	l.branch = (bdtrie_branch*)(l.leaf + n->hasleaf);
	l.children = (bdtrie_node**)((uint8_t*)l.branch + offsetof(bdtrie_branch, children));
	l.end = n->branchsize
		? (void*)(l.children + n->branchsize)
		: (void*)l.branch;

	return l;
}

typedef struct key {
	uint32_t size;
	uint8_t* data;
} key;

bdtrie_node* make_split(bdtrie* t, bdtrie_node* n, uint32_t index, bdtrie_node* parent) {
	node_layout l = layout_of(n);

	ptrdiff_t size = (uint8_t*)l.end - (uint8_t*)n;

	bdtrie_node* after = malloc(size - index);
	after->parent = parent;
	after->layout = n->layout;
	after->keysize -= index;
	memcpy(data_of(after), l.data + index, l.data_end - l.data - index);
	for (bdtrie_node** child = l.children; child < l.children_end; child++) {
		(**child).parent = after;
	}
	if (n->hasleaf) {
		t->update_value(l.leaf, after);
	}

	return after;
}

bdtrie_node* make_leaf(bdtrie* t, key k, key tail, bdtrie_node* parent) {
	bdtrie_node* leaf = malloc(sizeof(bdtrie_node) + tail.size + sizeof(bdtrie_leaf));
	leaf->parent = parent;
	leaf->hasleaf = true;
	leaf->branchsize = 0;
	leaf->keysize = tail.size;
	memcpy(data_of(leaf), tail.data, tail.size);

	bdtrie_leaf* l = leaf_of(leaf);
	l->value = t->get_value(k.size, k.data, leaf);
	l->key_size = k.size;

	return leaf;
}

bdtrie_value split_with_leaf(bdtrie* t, key k, bdtrie_node** np, uint32_t index) {
	bdtrie_node* n = *np;

	bdtrie_node* before = malloc(sizeof(bdtrie_node) + index + sizeof(bdtrie_leaf) + sizeof_branch(1));
	before->parent = n->parent;
	before->hasleaf = true;
	before->branchsize = 1;
	before->keysize = index;
	memcpy(data_of(before), data_of(n), index);

	bdtrie_node* after = make_split(t, n, index, before);

	free(n);
	*np = before;

	bdtrie_leaf* leaf = leaf_of(before);
	bdtrie_branch* branch = branch_of(before);

	leaf->value = t->get_value(k.size, k.data, before);
	leaf->key_size = k.size;

	popclear(branch->population);
	popadd(branch->population, *data_of(after));
	branch->children[0] = after;

	return (bdtrie_value){ before, leaf->value };
}

bdtrie_value split_with_branch(bdtrie* t, key k, bdtrie_node** np, uint32_t index, key tail) {
	bdtrie_node* n = *np;

	bdtrie_node* before = malloc(sizeof(bdtrie_node) + index + sizeof_branch(2));
	before->parent = n->parent;
	before->hasleaf = false;
	before->branchsize = 2;
	before->keysize = index;
	memcpy(data_of(before), data_of(n), index);

	bdtrie_node* child = make_leaf(t, k, tail, before);
	bdtrie_node* after = make_split(t, n, index, before);

	free(n);
	*np = before;

	bdtrie_branch* branch = branch_of(before);

	popclear(branch->population);
	popadd(branch->population, *data_of(after));
	popadd(branch->population, *data_of(child));
	if (data_of(child) > data_of(after)) {
		branch->children[0] = after;
		branch->children[1] = child;
	} else {
		branch->children[0] = child;
		branch->children[1] = after;
	}

	return value_of(child);
}

key key_tail(key k, uint32_t head) {
	k.size -= head;
	k.data = (uint8_t*)k.data + head;
	return k;
}

bdtrie_value add_first_child(bdtrie* t, key k, bdtrie_node** np, key tail) {
	bdtrie_node* n = *np;
	bdtrie_branch* b = branch_of(n);

	ptrdiff_t size = (uint8_t*)(b->children + n->branchsize) - (uint8_t*)n;
	ptrdiff_t size_to_branch = (uint8_t*)b - (uint8_t*)n;
	ptrdiff_t size_to_child = (uint8_t*)b->children - (uint8_t*)n;

	bdtrie_node* before = malloc(size + sizeof_branch(1));
	bdtrie_node** child = (bdtrie_node**)((uint8_t*)before + size_to_child);

	memcpy(before, n, size_to_branch);
	*child = make_leaf(t, k, tail, before);

	free(n);
	*np = before;

	bdtrie_leaf* leaf = leaf_of(before);
	bdtrie_branch* branch = branch_of(before);

	t->update_value(leaf->value, before);

	before->branchsize = 1;
	popclear(branch->population);
	popadd(branch->population, tail.data[0]);

	return value_of(*child);
}

bdtrie_value add_child(bdtrie* t, key k, bdtrie_node** np, bdtrie_branch* b, uint8_t index, key tail) {
	bdtrie_node* n = *np;

	ptrdiff_t size = (uint8_t*)(b->children + n->branchsize) - (uint8_t*)n;
	ptrdiff_t size_to_child = (uint8_t*)(b->children + index) - (uint8_t*)n;
	ptrdiff_t size_from_child = size - size_to_child;

	bdtrie_node* before = malloc(size + sizeof(bdtrie_node*));
	bdtrie_node** child = (bdtrie_node**)((uint8_t*)before + size_to_child);

	memcpy(before, n, size_to_child);
	*child = make_leaf(t, k, tail, before);
	memcpy(child + 1, (b->children + index), size_from_child);

	free(n);
	*np = before;

	bdtrie_leaf* leaf = leaf_of(before);
	bdtrie_branch* branch = branch_of(before);

	if (before->hasleaf) {
		t->update_value(leaf, before);
	}

	for (int i = 0; i < before->branchsize; i++) {
		branch->children[i]->parent = before;
	}
	before->branchsize++;
	popadd(branch->population, tail.data[0]);

	return value_of(*child);
}

bdtrie_value insert_recur(bdtrie* t, key k, bdtrie_node** np, key tail) {
	bdtrie_node* n = *np;

	if (tail.size < n->keysize) {
		for (int i = 0; i < tail.size; i++) {
			if (data_of(n)[i] != tail.data[i]) {
				return split_with_branch(t, k, np, i, key_tail(tail, i));
			}
		}

		return split_with_leaf(t, k, np, tail.size);
	}

	for (int i = 0; i < n->keysize; i++) {
		if (data_of(n)[i] != tail.data[i]) {
			return split_with_branch(t, k, np, i, key_tail(tail, i));
		}
	}

	if (tail.size == n->keysize) {
		n->hasleaf = true;
		return value_of(n);
	}

	tail = key_tail(tail, n->keysize);

	if (n->branchsize == 0) {
		return add_first_child(t, k, np, tail);
	}

	bdtrie_branch* b = branch_of(n);

	uint8_t pi = popindex(b->population, tail.data[0]);
	bdtrie_node** child = b->children + pi;

	if (pi == n->branchsize || data_of(*child)[0] != tail.data[0]) {
		return add_child(t, k, np, b, pi, tail);
	} else {
		return insert_recur(t, k, child, tail);
	}
}

bdtrie_value bdtrie_insert(bdtrie* t, uint32_t key_size, void* key_data) {
	key k = { key_size, key_data };

	if (t->root == NULL ) {
		t->root = make_leaf(t, k, k, NULL);
		return value_of(t->root);
	} else {
		return insert_recur(t, k, &t->root, k);
	}
}

void bdtrie_delete(bdtrie_node* n) {
}

void key_data_recur(uint32_t size, uint8_t* dest, bdtrie_node* n) {
	size -= n->keysize;
	if (n->parent != NULL) {
		key_data_recur(size, dest, n->parent);
	}

	memcpy(dest + size, n + 1, n->keysize);
}

uint32_t bdtrie_key(void* dest, bdtrie_node* n) {
	uint32_t size = bdtrie_key_size(n);
	if (size > 0) {
		key_data_recur(size, dest, n);
	}
	return size;
}

uint32_t bdtrie_key_size(bdtrie_node* n) {
	if (!n->hasleaf) {
		return -1;
	}
	return leaf_of(n)->key_size;
}

bdtrie_value find_recur(bdtrie* t, key tail, bdtrie_node* n) {
	if (tail.size < n->keysize) {
		return (bdtrie_value){ NULL, NULL };
	}

	for (int i = 0; i < n->keysize; i++) {
		if (data_of(n)[i] != tail.data[i]) {
			return (bdtrie_value){ NULL, NULL };
		}
	}

	if (tail.size == n->keysize) {
		return value_of(n);
	}

	if (n->branchsize == 0) {
		return (bdtrie_value){ NULL, NULL };
	}

	bdtrie_branch* b = branch_of(n);
	tail = key_tail(tail, n->keysize);

	uint8_t pi = popindex(b->population, tail.data[0]);
	bdtrie_node** child = b->children + pi;

	return find_recur(t, tail, *child);
}

bdtrie_value bdtrie_find(bdtrie* t, uint32_t key_size, void* key_data) {
	key k = { key_size, key_data };
	return find_recur(t, k, t->root);
}

void clear_node(bdtrie* t, bdtrie_node* n) {
	if (n->hasleaf) {
		bdtrie_leaf* leaf = leaf_of(n);
		t->free_value(leaf->value);
	}

	if (n->branchsize) {
		bdtrie_branch* branch = branch_of(n);
		for (int i = 0; i < n->branchsize; i++) {
			clear_node(t, branch->children[i]);
		}
	}

	free(n);
}

void bdtrie_clear(bdtrie* t) {
	if (t->root != NULL) {
		clear_node(t, t->root);
	}
}

