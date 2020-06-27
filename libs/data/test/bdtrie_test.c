#include <stdbool.h>
#include <stdlib.h> 
#include <string.h>

#include <unity.h>

#include "c-ohvu/data/bdtrie.h"

static bdtrie trie;

typedef enum operation {
	INSERT,
	DELETE
} operation;

typedef struct update {
	char* key;
	uint32_t index;
	operation op;
} update;

int compare_entries(const void* a, const void* b) {
	const update* ua = a;
	const update* ub = b;
	int c = strcmp(ua->key, ub->key);
	if (!c) {
		c = ub->index - ua->index;
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

size_t sort_updates(size_t s, update* u) {
	qsort(u, s, sizeof(update), compare_entries);
	int count = 0, i = 0, j = 1;
	while (i < s) {
		if ((j == s || strcmp(u[i].key, u[j].key) != 0) && u[i].op == INSERT) {
			u[count++] = u[i];
		}
		i++; j++;
	}
}

void apply_updates(size_t s, update* u) {
	for (int i = 0; i < s; i++) {
		switch (u[i].op) {
			case INSERT:
				bdtrie_insert(&trie, strlen(u[i].key), u[i].key);
				break;
			case DELETE:
				bdtrie_delete(bdtrie_find(&trie, strlen(u[i].key), u[i].key).node);
				break;
		}
	}
}

void test_insert(size_t s, char** k) {
	update* u = malloc(sizeof(update) * s);
	for (int i = 0; i < s; i++) {
		u[i] = (update){ k[i], i, INSERT };
	}

	apply_updates(s, u);
	s = sort_updates(s, u);

	// TODO check sorted updates match trie iteration order

	free(u);
}

void test_insert_and_remove(size_t s, char** k, operation* o) {
	update* u = malloc(sizeof(update) * s);
	for (int i = 0; i < s; i++) {
		u[i] = (update){ k[i], i, o[i] };
	}

	apply_updates(s, u);
	s = sort_updates(s, u);

	// TODO check sorted updates match trie iteration order

	free(u);
}

void test_insert_1() {
	char* k[] = {};
	test_insert(sizeof(k) / sizeof(char*), k);
}

void test_insert_2() {
	char* k[] = { "" };
	test_insert(sizeof(k) / sizeof(char*), k);
}

void test_insert_3() {
	char* k[] = { "a" };
	test_insert(sizeof(k) / sizeof(char*), k);
}

void test_insert_4() {
	char* k[] = { "", "a", "ab" };
	test_insert(sizeof(k) / sizeof(char*), k);
}

void test_insert_5() {
	char* k[] = { "a", "", "ab" };
	test_insert(sizeof(k) / sizeof(char*), k);
}

void test_insert_6() {
	char* k[] = { "a", "a", "a", "ab" };
	test_insert(sizeof(k) / sizeof(char*), k);
}

void test_insert_7() {
	char* k[] = { "abcdef", "abc", "abcx" };
	test_insert(sizeof(k) / sizeof(char*), k);
}

void test_insert_8() {
	char* k[] = { "abc", "abd", "abz", "abe", "aby", "abx", "abz", "abzzz" };
	test_insert(sizeof(k) / sizeof(char*), k);
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_insert_1);
	RUN_TEST(test_insert_2);
	RUN_TEST(test_insert_3);
	RUN_TEST(test_insert_4);
	RUN_TEST(test_insert_5);
	RUN_TEST(test_insert_6);
	RUN_TEST(test_insert_7);
	RUN_TEST(test_insert_8);

	return UNITY_END();
}

