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

size_t sizeof_node(uint16_t keysize, bool hasleaf, bool hasbranch, uint8_t branchsize) {
	return sizeof(bdtrie_node)
		+ keysize
		+ hasleaf * sizeof(bdtrie_leaf)
		+ hasbranch * (offsetof(bdtrie_branch, children) + sizeof(bdtrie_node*) * branchsize);
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

typedef struct entry {
	key key;
	void* value_data;
} entry;

bdtrie_node* make_split(bdtrie* t, bdtrie_node* n, uint32_t index, bdtrie_node* parent) {
	node_layout l = layout_of(n);

	ptrdiff_t size = (uint8_t*)l.end - (uint8_t*)n - index;
	ptrdiff_t tail_size = (uint8_t*)l.end - (uint8_t*)l.data - index;

	bdtrie_node* after = malloc(size);
	after->parent = parent;
	after->layout = n->layout;
	after->keysize -= index;
	memcpy(data_of(after), l.data + index, tail_size);
	for (bdtrie_node** child = l.children; child < l.children_end; child++) {
		(**child).parent = after;
	}
	if (n->hasleaf) {
		bdtrie_leaf* leaf = leaf_of(after);
		t->update_value(l.leaf->value, after);
	}

	return after;
}

bdtrie_node* make_leaf(bdtrie* t, entry e, key k, bdtrie_node* parent) {
	bdtrie_node* leaf = malloc(sizeof_node(k.size, true, false, 0));
	leaf->parent = parent;
	leaf->hasleaf = true;
	leaf->branchsize = 0;
	leaf->keysize = k.size;
	memcpy(data_of(leaf), k.data, k.size);

	bdtrie_leaf* l = leaf_of(leaf);
	l->value = t->alloc_value(e.key.size, e.key.data, e.value_data, leaf);
	l->key_size = e.key.size;

	return leaf;
}

bdtrie_value split_with_leaf(bdtrie* t, entry e, bdtrie_node** np, uint32_t index) {
	bdtrie_node* n = *np;

	bdtrie_node* before = malloc(sizeof_node(index, true, true, 1));
	before->parent = n->parent;
	before->parent_index = n->parent_index;
	before->hasleaf = true;
	before->branchsize = 1;
	before->keysize = index;
	memcpy(data_of(before), data_of(n), index);

	bdtrie_node* after = make_split(t, n, index, before);

	free(n);
	*np = before;

	bdtrie_leaf* leaf = leaf_of(before);
	bdtrie_branch* branch = branch_of(before);

	leaf->value = t->alloc_value(e.key.size, e.key.data, e.value_data, before);
	leaf->key_size = e.key.size;

	popclear(branch->population);
	popadd(branch->population, *data_of(after));
	branch->children[0] = after;
	after->parent_index = 0;

	return (bdtrie_value){ before, leaf->value };
}

bdtrie_value split_with_branch(bdtrie* t, entry e, bdtrie_node** np, uint32_t index, key k) {
	bdtrie_node* n = *np;

	bdtrie_node* before = malloc(sizeof_node(index, false, true, 2));
	before->parent = n->parent;
	before->parent_index = n->parent_index;
	before->hasleaf = false;
	before->branchsize = 2;
	before->keysize = index;
	memcpy(data_of(before), data_of(n), index);

	bdtrie_node* child = make_leaf(t, e, k, before);
	bdtrie_node* after = make_split(t, n, index, before);

	free(n);
	*np = before;

	bdtrie_branch* branch = branch_of(before);

	popclear(branch->population);
	popadd(branch->population, *data_of(after));
	popadd(branch->population, *data_of(child));
	if (*data_of(child) > *data_of(after)) {
		branch->children[0] = after;
		after->parent_index = 0;
		branch->children[1] = child;
		child->parent_index = 1;
	} else {
		branch->children[0] = child;
		child->parent_index = 0;
		branch->children[1] = after;
		after->parent_index = 1;
	}

	return value_of(child);
}

key key_tail(key k, uint32_t head) {
	k.size -= head;
	k.data = (uint8_t*)k.data + head;
	return k;
}

bdtrie_value add_leaf(bdtrie* t, entry e, bdtrie_node** np) {
	bdtrie_node* n = *np;

	bdtrie_node* replacement = malloc(sizeof_node(n->keysize, true, true, n->branchsize));
	memcpy(replacement, n, sizeof_node(n->keysize, false, true, 0));
	replacement->hasleaf = true;

	bdtrie_leaf* leaf = leaf_of(replacement);
	bdtrie_branch* branch = branch_of(replacement);

	memcpy(branch, branch_of(n), offsetof(bdtrie_branch, children) + sizeof(bdtrie_node*) * n->branchsize);

	free(n);
	*np = replacement;

	leaf->value = t->alloc_value(e.key.size, e.key.data, e.value_data, replacement);
	leaf->key_size = e.key.size;

	for (int i = 0; i < replacement->branchsize; i++) {
		branch->children[i]->parent = replacement;
	}

	return value_of(replacement);
}

bdtrie_value add_first_child(bdtrie* t, entry e, bdtrie_node** np, key k) {
	bdtrie_node* n = *np;

	bdtrie_node* replacement = malloc(sizeof_node(n->keysize, true, true, 1));
	memcpy(replacement, n, sizeof_node(n->keysize, true, false, 0));

	free(n);
	*np = replacement;

	bdtrie_leaf* leaf = leaf_of(replacement);
	t->update_value(leaf->value, replacement);

	bdtrie_branch* branch = branch_of(replacement);
	branch->children[0] = make_leaf(t, e, k, replacement);
	replacement->branchsize = 1;
	branch->children[0]->parent = replacement;
	branch->children[0]->parent_index = 0;
	popclear(branch->population);
	popadd(branch->population, k.data[0]);

	return value_of(branch->children[0]);
}

bdtrie_value add_child(bdtrie* t, entry e, bdtrie_node** np, bdtrie_branch* b, uint8_t index, key k) {
	bdtrie_node* n = *np;

	size_t size = sizeof_node(n->keysize, n->hasleaf, true, n->branchsize + 1);
	bdtrie_node* replacement = malloc(size);

	size_t size_to_child = sizeof_node(n->keysize, n->hasleaf, true, index);
	memcpy(replacement, n, size_to_child);

	size_t size_to_next_child = size_to_child + sizeof(bdtrie_branch*);
	memcpy((uint8_t*)replacement + size_to_next_child, (uint8_t*)n + size_to_child, size - size_to_next_child);

	free(n);
	*np = replacement;

	bdtrie_branch* branch = branch_of(replacement);
	branch->children[index] = make_leaf(t, e, k, replacement);

	if (replacement->hasleaf) {
		bdtrie_leaf* leaf = leaf_of(replacement);
		t->update_value(leaf, replacement);
	}

	replacement->branchsize++;
	for (int i = 0; i < replacement->branchsize; i++) {
		branch->children[i]->parent = replacement;
		branch->children[i]->parent_index = i;
	}
	popadd(branch->population, k.data[0]);

	return value_of(branch->children[index]);
}

bdtrie_value insert_recur(bdtrie* t, entry e, bdtrie_node** np, key k, bool override) {
	bdtrie_node* n = *np;

	if (k.size < n->keysize) {
		for (int i = 0; i < k.size; i++) {
			if (data_of(n)[i] != k.data[i]) {
				return split_with_branch(t, e, np, i, key_tail(k, i));
			}
		}

		return split_with_leaf(t, e, np, k.size);
	}

	for (int i = 0; i < n->keysize; i++) {
		if (data_of(n)[i] != k.data[i]) {
			return split_with_branch(t, e, np, i, key_tail(k, i));
		}
	}

	if (k.size == n->keysize) {
		if (n->hasleaf) {
			if (override) {
				bdtrie_leaf* l = leaf_of(n);
				t->free_value(l->value);
				l->value = t->alloc_value(e.key.size, e.key.data, e.value_data, n);
				return (bdtrie_value){ n, l->value };
			}
			return value_of(n);
		} else {
			return add_leaf(t, e, np);
		}
	}

	k = key_tail(k, n->keysize);

	if (n->branchsize == 0) {
		return add_first_child(t, e, np, k);
	}

	bdtrie_branch* b = branch_of(n);

	uint8_t pi = popindex(b->population, k.data[0]);
	bdtrie_node** child = b->children + pi;

	if (pi == n->branchsize || data_of(*child)[0] != k.data[0]) {
		return add_child(t, e, np, b, pi, k);
	} else {
		return insert_recur(t, e, child, k, override);
	}
}

bdtrie_value bdtrie_insert(bdtrie* t, uint32_t key_size, void* key_data, void* value_data) {
	key k = { key_size, key_data };
	entry e = { k, value_data };

	if (t->root == NULL ) {
		t->root = make_leaf(t, e, k, NULL);
		return value_of(t->root);
	} else {
		return insert_recur(t, e, &t->root, k, true);
	}
}

bdtrie_value bdtrie_find_or_insert(bdtrie* t, uint32_t key_size, void* key_data, void* value_data) {
	key k = { key_size, key_data };
	entry e = { k, value_data };

	if (t->root == NULL ) {
		t->root = make_leaf(t, e, k, NULL);
		return value_of(t->root);
	} else {
		return insert_recur(t, e, &t->root, k, false);
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
	if (t->root == NULL) {
		return (bdtrie_value){ NULL, NULL };
	}
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

bdtrie_value bdtrie_first(bdtrie* t) {
	if (!t->root) {
		return (bdtrie_value){ NULL, NULL };
	}

	bdtrie_node* n = t->root;

	while (!n->hasleaf) {
		n = children_of(n)[0];
	}

	return value_of(n);
}

bdtrie_value bdtrie_next(bdtrie_value v) {
	bdtrie_node* n = v.node;

	if (n->branchsize > 0) {
		do {
			n = children_of(n)[0];
		} while (!n->hasleaf);

		return value_of(n);
	}

	while (n->parent != NULL) {
		int i = n->parent_index + 1;
		n = n->parent;
		if (i < n->branchsize) {
			n = children_of(n)[i];

			while (!n->hasleaf) {
				n = children_of(n)[0];
			}

			return value_of(n);
		}
	}

	return (bdtrie_value){ NULL, NULL };
}

bool bdtrie_is_present(bdtrie_value v) {
	return v.node != NULL;
}

