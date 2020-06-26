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

int compare_entries(const void* a, const void* b) {
	const entry* ea = a;
	const entry* eb = b;
	int c = strcmp(ea->key, eb->key);
	if (!c) {
		c = eb->index - ea->index;
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

	qsort(e, s, sizeof(entry), compare_entries);
	int count = 0;
	for (int i = 1; i < s; i++) {
		if (e[i].index < 0 || strcmp(e[count].key, e[i + 1].key)) {
			count++;
		}
		e[count] = e[i - 1];
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

	free(e);
}

void test_insert_and_remove_multiple_keys() {

}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_insert_multiple_entries);

	return UNITY_END();
}

