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

void popremove(uint64_t* p, uint8_t slot) {
	p[slot / 64] &= ~(slot_bit << (slot % 64));
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

size_t sizeof_node(uint16_t key_size, bool has_leaf, bool hasbranch, uint8_t branch_size) {
	return sizeof(bdtrie_node)
		+ key_size
		+ has_leaf * sizeof(bdtrie_leaf)
		+ hasbranch * (offsetof(bdtrie_branch, children) + sizeof(bdtrie_node*) * branch_size);
}

uint8_t* data_of(bdtrie_node* n) {
	return (uint8_t*)(n + 1);
}

bdtrie_leaf* leaf_of(bdtrie_node* n) {
	return (bdtrie_leaf*)(data_of(n) + n->key_size);
}

bdtrie_value value_of(bdtrie_node* n) {
	return (bdtrie_value){ n, leaf_of(n)->value };
}

bdtrie_branch* branch_of(bdtrie_node* n) {
	return (bdtrie_branch*)(leaf_of(n) + n->has_leaf);
}

bdtrie_node** children_of(bdtrie_node* n) {
	return (bdtrie_node**)((uint8_t*)branch_of(n) + offsetof(bdtrie_branch, children));
}

void* end_of(bdtrie_node* n) {
	return n->branch_size
		? (void*)children_of(n) + n->branch_size
		: (void*)branch_of(n);
}

node_layout layout_of(bdtrie_node* n) {
	node_layout l;
	l.data = (uint8_t*)(n + 1);
	l.leaf = (bdtrie_leaf*)(l.data + n->key_size);
	l.branch = (bdtrie_branch*)(l.leaf + n->has_leaf);
	l.children = (bdtrie_node**)((uint8_t*)l.branch + offsetof(bdtrie_branch, children));
	l.end = n->branch_size
		? (void*)(l.children + n->branch_size)
		: (void*)l.branch;

	return l;
}

typedef struct key {
	uint32_t size;
	const uint8_t* data;
} key;

bdtrie_node* make_split(bdtrie* t, bdtrie_node* n, uint32_t index, bdtrie_node* parent) {
	node_layout l = layout_of(n);

	ptrdiff_t size = (uint8_t*)l.end - (uint8_t*)n - index;
	ptrdiff_t tail_size = (uint8_t*)l.end - (uint8_t*)l.data - index;

	bdtrie_node* after = malloc(size);
	after->parent = parent;
	after->layout = n->layout;
	after->has_parent = true;
	after->key_size -= index;
	memcpy(data_of(after), l.data + index, tail_size);
	for (bdtrie_node** child = l.children; child < l.children_end; child++) {
		(**child).parent = after;
	}
	if (n->has_leaf) {
		bdtrie_leaf* leaf = leaf_of(after);
		t->update_value(l.leaf->value, after);
	}

	return after;
}

bdtrie_node* make_leaf(key k, bdtrie_node* parent) {
	bdtrie_node* leaf = malloc(sizeof_node(k.size, true, false, 0));
	leaf->parent = parent;
	leaf->has_parent = true;
	leaf->has_leaf = true;
	leaf->branch_size = 0;
	leaf->key_size = k.size;
	memcpy(data_of(leaf), k.data, k.size);

	leaf_of(leaf)->value = NULL;

	return leaf;
}

bdtrie_node* make_root(key k, bdtrie* trie) {
	bdtrie_node* n = make_leaf(k, (void*)trie);
	n->has_parent = false;
	return n;
}

bdtrie_node* split_with_leaf(bdtrie* t, bdtrie_node** np, uint32_t index) {
	bdtrie_node* n = *np;

	bdtrie_node* before = malloc(sizeof_node(index, true, true, 1));
	before->parent = n->parent;
	before->parent_index = n->parent_index;
	before->has_parent = n->has_parent;
	before->has_leaf = true;
	before->branch_size = 1;
	before->key_size = index;
	memcpy(data_of(before), data_of(n), index);

	bdtrie_node* after = make_split(t, n, index, before);

	free(n);
	*np = before;

	leaf_of(before)->value = NULL;
	bdtrie_branch* branch = branch_of(before);

	popclear(branch->population);
	popadd(branch->population, *data_of(after));
	branch->children[0] = after;
	after->parent_index = 0;

	return before;
}

bdtrie_node* split_with_branch(bdtrie* t, bdtrie_node** np, uint32_t index, key k) {
	bdtrie_node* n = *np;

	bdtrie_node* before = malloc(sizeof_node(index, false, true, 2));
	before->parent = n->parent;
	before->parent_index = n->parent_index;
	before->has_parent = n->has_parent;
	before->has_leaf = false;
	before->branch_size = 2;
	before->key_size = index;
	memcpy(data_of(before), data_of(n), index);

	bdtrie_node* child = make_leaf(k, before);
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

	return child;
}

key key_tail(key k, uint32_t head) {
	k.size -= head;
	k.data = (uint8_t*)k.data + head;
	return k;
}

bdtrie_node* add_leaf(bdtrie* t, bdtrie_node** np) {
	bdtrie_node* n = *np;

	bdtrie_node* replacement = malloc(sizeof_node(n->key_size, true, true, n->branch_size));
	memcpy(replacement, n, sizeof_node(n->key_size, false, true, 0));
	replacement->has_leaf = true;

	leaf_of(replacement)->value = NULL;
	bdtrie_branch* branch = branch_of(replacement);

	memcpy(branch, branch_of(n), offsetof(bdtrie_branch, children) + sizeof(bdtrie_node*) * n->branch_size);

	free(n);
	*np = replacement;

	for (int i = 0; i < replacement->branch_size; i++) {
		branch->children[i]->parent = replacement;
	}

	return replacement;
}

bdtrie_node* add_first_child(bdtrie* t, bdtrie_node** np, key k) {
	bdtrie_node* n = *np;

	bdtrie_node* replacement = malloc(sizeof_node(n->key_size, true, true, 1));
	memcpy(replacement, n, sizeof_node(n->key_size, true, false, 0));

	free(n);
	*np = replacement;

	bdtrie_leaf* leaf = leaf_of(replacement);
	t->update_value(leaf->value, replacement);

	bdtrie_branch* branch = branch_of(replacement);
	branch->children[0] = make_leaf(k, replacement);
	replacement->branch_size = 1;
	branch->children[0]->parent = replacement;
	branch->children[0]->parent_index = 0;
	popclear(branch->population);
	popadd(branch->population, k.data[0]);

	return branch->children[0];
}

bdtrie_node* add_child(bdtrie* t, bdtrie_node** np, uint8_t index, key k) {
	bdtrie_node* n = *np;

	size_t size = sizeof_node(n->key_size, n->has_leaf, true, n->branch_size + 1);
	bdtrie_node* replacement = malloc(size);

	size_t size_to_child = sizeof_node(n->key_size, n->has_leaf, true, index);
	memcpy(replacement, n, size_to_child);

	size_t size_to_next_child = size_to_child + sizeof(bdtrie_branch*);
	memcpy((uint8_t*)replacement + size_to_next_child, (uint8_t*)n + size_to_child, size - size_to_next_child);

	free(n);
	*np = replacement;

	bdtrie_branch* branch = branch_of(replacement);
	branch->children[index] = make_leaf(k, replacement);

	if (replacement->has_leaf) {
		bdtrie_leaf* leaf = leaf_of(replacement);
		t->update_value(leaf, replacement);
	}

	replacement->branch_size++;
	for (int i = 0; i < replacement->branch_size; i++) {
		branch->children[i]->parent = replacement;
		branch->children[i]->parent_index = i;
	}
	popadd(branch->population, k.data[0]);

	return branch->children[index];
}

bdtrie_node* insert_recur(bdtrie* t, bdtrie_node** np, key k) {
	bdtrie_node* n = *np;

	if (k.size < n->key_size) {
		for (int i = 0; i < k.size; i++) {
			if (data_of(n)[i] != k.data[i]) {
				return split_with_branch(t, np, i, key_tail(k, i));
			}
		}

		return split_with_leaf(t, np, k.size);
	}

	for (int i = 0; i < n->key_size; i++) {
		if (data_of(n)[i] != k.data[i]) {
			return split_with_branch(t, np, i, key_tail(k, i));
		}
	}

	if (k.size == n->key_size) {
		if (n->has_leaf) {
			return n;
		} else {
			return add_leaf(t, np);
		}
	}

	k = key_tail(k, n->key_size);

	if (n->branch_size == 0) {
		return add_first_child(t, np, k);
	}

	bdtrie_branch* b = branch_of(n);

	uint8_t pi = popindex(b->population, k.data[0]);
	bdtrie_node** child = b->children + pi;

	if (pi == n->branch_size || data_of(*child)[0] != k.data[0]) {
		return add_child(t, np, pi, k);
	} else {
		return insert_recur(t, child, k);
	}
}

bdtrie_value bdtrie_insert(bdtrie* t, uint32_t key_size, const void* key_data, const void* value_data) {
	key k = { key_size, key_data };

	bdtrie_node* n;
	if (t->root == NULL) {
		n = make_root(k, t);
		t->root = n;

	} else {
		n = insert_recur(t, &t->root, k);
	}

	bdtrie_leaf* l = leaf_of(n);

	if (l->value == NULL) {
		l->key_size = key_size;
	} else {
		t->free_value(l->value);
	}
	l->value = t->alloc_value(key_size, key_data, value_data, n);
	return (bdtrie_value){ n, l->value };
}

bdtrie_value bdtrie_find_or_insert(bdtrie* t, uint32_t key_size, const void* key_data, const void* value_data) {
	key k = { key_size, key_data };

	bdtrie_node* n;
	if (t->root == NULL) {
		n = make_root(k, t);
		t->root = n;

	} else {
		n = insert_recur(t, &t->root, k);
	}

	bdtrie_leaf* l = leaf_of(n);

	if (l->value == NULL) {
		l->key_size = key_size;
		l->value = t->alloc_value(key_size, key_data, value_data, n);
	}
	return (bdtrie_value){ n, l->value };
}

bdtrie_node** pointer_of(bdtrie_node* n) {
	if (n->has_parent) {
		return &children_of(n->parent)[n->parent_index];
	} else {
		return &n->trie->root;
	}
}


void splice_node(bdtrie_node* n, bdtrie_node* c) {
	bdtrie_node** np = pointer_of(n);

	size_t size = sizeof_node(n->key_size + c->key_size, c->has_leaf, c->branch_size, c->branch_size);
	bdtrie_node* combined = malloc(size);
	combined->layout = c->layout;
	combined->parent = n->parent;
	combined->parent_index = n->parent_index;
	combined->has_parent = n->has_parent;
	combined->key_size += n->key_size;

	memcpy(data_of(combined), data_of(n), n->key_size);
	memcpy(data_of(combined) + n->key_size, data_of(c), size - sizeof_node(n->key_size, false, false, 0));

	free(n);
	free(c);
	*np = combined;

	bdtrie_branch* branch = branch_of(combined);

	for (int i = 0; i < combined->branch_size; i++) {
		branch->children[i]->parent = combined;
	}
	if (combined->has_leaf) {
		bdtrie_leaf* leaf = leaf_of(combined);
		bdtrie_trie(combined)->update_value(leaf->value, combined);
	}
}

void remove_last_child(bdtrie_node* n) {
	bdtrie_node** np = pointer_of(n);

	uint32_t size = sizeof_node(n->key_size, true, true, 1);
	bdtrie_node* replacement = malloc(size);
	memcpy(replacement, n, size);
	replacement->branch_size = 0;

	free(n);
	*np = replacement;
}

void remove_child_at(bdtrie_node* n, uint8_t index) {
	bdtrie_node** np = pointer_of(n);

	size_t size = sizeof_node(n->key_size, n->has_leaf, true, n->branch_size - 1);
	bdtrie_node* replacement = malloc(size);

	size_t size_to_child = sizeof_node(n->key_size, n->has_leaf, true, index);
	memcpy(replacement, n, size_to_child);

	size_t size_to_next_child = size_to_child + sizeof(bdtrie_branch*);
	memcpy((uint8_t*)replacement + size_to_child, (uint8_t*)n + size_to_next_child, size - size_to_child);

	free(n);
	*np = replacement;

	bdtrie_branch* branch = branch_of(replacement);

	if (replacement->has_leaf) {
		bdtrie_leaf* leaf = leaf_of(replacement);
		bdtrie_trie(n)->update_value(leaf->value, replacement);
	}

	replacement->branch_size--;
	for (int i = 0; i < replacement->branch_size; i++) {
		branch->children[i]->parent = replacement;
		branch->children[i]->parent_index = i;
	}
	popremove(branch->population, data_of(replacement)[0]);
}

void remove_child(bdtrie_node* p, bdtrie_node* c) {
	if (p->has_leaf && p->branch_size == 1) {
		remove_last_child(p);

	} else if (!p->has_leaf && p->branch_size == 2) {
		bdtrie_node** pc = children_of(p);
		splice_node(p, pc[0] == c ? pc[1] : pc[0]);

	} else {
		remove_child_at(p, c->parent_index);
	}

	free(c);
}

void remove_leaf(bdtrie_node* n) {
	bdtrie_node** np = pointer_of(n);

	bdtrie_node* replacement = malloc(sizeof_node(n->key_size, false, true, n->branch_size));
	memcpy(replacement, n, sizeof_node(n->key_size, false, true, 0));
	replacement->has_leaf = false;

	bdtrie_branch* branch = branch_of(replacement);

	memcpy(branch, branch_of(n), offsetof(bdtrie_branch, children) + sizeof(bdtrie_node*) * n->branch_size);

	free(n);
	*np = replacement;

	for (int i = 0; i < replacement->branch_size; i++) {
		branch->children[i]->parent = replacement;
	}
}

void bdtrie_delete(bdtrie_node* n) {
	bdtrie_trie(n)->free_value(leaf_of(n)->value);

	if (n->branch_size == 0) {
		if (n->has_parent) {
			remove_child(n->parent, n);

		} else {
			n->trie->root = NULL;
			free(n);
		}

	} else if (n->branch_size == 1) {
		splice_node(n, children_of(n)[0]);

	} else {
		remove_leaf(n);
	}
}

void key_data_recur(uint32_t size, uint8_t* dest, bdtrie_node* n) {
	size -= n->key_size;
	if (n->has_parent) {
		key_data_recur(size, dest, n->parent);
	}

	memcpy(dest + size, n + 1, n->key_size);
}

uint32_t bdtrie_key(void* dest, bdtrie_node* n) {
	uint32_t size = bdtrie_key_size(n);
	if (size > 0) {
		key_data_recur(size, dest, n);
	}
	return size;
}

uint32_t bdtrie_key_size(bdtrie_node* n) {
	if (!n->has_leaf) {
		return -1;
	}
	return leaf_of(n)->key_size;
}

bdtrie_value find_recur(const bdtrie* t, key tail, bdtrie_node* n) {
	if (tail.size < n->key_size) {
		return (bdtrie_value){ NULL, NULL };
	}

	for (int i = 0; i < n->key_size; i++) {
		if (data_of(n)[i] != tail.data[i]) {
			return (bdtrie_value){ NULL, NULL };
		}
	}

	if (tail.size == n->key_size) {
		return value_of(n);
	}

	if (n->branch_size == 0) {
		return (bdtrie_value){ NULL, NULL };
	}

	bdtrie_branch* b = branch_of(n);
	tail = key_tail(tail, n->key_size);

	uint8_t pi = popindex(b->population, tail.data[0]);
	bdtrie_node** child = b->children + pi;

	return find_recur(t, tail, *child);
}

bdtrie_value bdtrie_find(const bdtrie* t, uint32_t key_size, const void* key_data) {
	key k = { key_size, key_data };
	if (t->root == NULL) {
		return (bdtrie_value){ NULL, NULL };
	}
	return find_recur(t, k, t->root);
}

void clear_node(bdtrie* t, bdtrie_node* n) {
	if (n->has_leaf) {
		bdtrie_leaf* leaf = leaf_of(n);
		t->free_value(leaf->value);
	}

	if (n->branch_size) {
		bdtrie_branch* branch = branch_of(n);
		for (int i = 0; i < n->branch_size; i++) {
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

	while (!n->has_leaf) {
		n = children_of(n)[0];
	}

	return value_of(n);
}

bdtrie_value bdtrie_next(bdtrie_value v) {
	bdtrie_node* n = v.node;

	if (n->branch_size > 0) {
		do {
			n = children_of(n)[0];
		} while (!n->has_leaf);

		return value_of(n);
	}

	while (n->has_parent) {
		int i = n->parent_index + 1;
		n = n->parent;
		if (i < n->branch_size) {
			n = children_of(n)[i];

			while (!n->has_leaf) {
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

bdtrie* bdtrie_trie(bdtrie_node* n) {
	while (n->has_parent) {
		n = n->parent;
	}
	return n->trie;
}

