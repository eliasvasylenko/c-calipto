#include <stdbool.h>
#include <stdlib.h> 
#include <string.h>

#include <unity.h>

#include "c-ohvu/data/bdtrie.h"

static bdtrie trie;

typedef struct entry {
	char* key;
	uint32_t index;
} entry;

int compare_entries(entry* a, entry* b) {
	int c = strcmp(a->key, b->key);
	if (!c) {
		c = b->index - a->index;
	}
	return c;
}

void* get_value(uint32_t key_size, void* key_data, bdtrie_node* owner) {
	;
}

void update_value(void* value, bdtrie_node* owner) {
	;
}

void free_value(void* value) {
	;
}

void setUp() {
	trie = (bdtrie){
		NULL,
		get_value,
		update_value,
		free_value
	};
}

void tearDown() {
	bdtrie_clear(trie);
}

void test_insert(size_t s, char** k) {
	entry* e = malloc(sizeof(entry) * s);
	for (int i = 0; i < s; i++) {
		e[i] = (entry){ k[i], i };
	}

	// 
	// associate each key with its index value
	//
	// insert each into trie
	//
	// qsort entries by key (secondary by index)
	//
	// check trie iterate in same order!
	//
}

void test_insert_and_remove_multiple_keys() {

}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_insert_multiple_entries);

	return UNITY_END();
}

