/*
 * This trie is designed for interning data. Each key is a sequence of bytes, and
 * each value is a unique pointer into the trie.
 *
 * There is a two-way association between keys and values, and key data is stored
 * on each node, meaning common prefixes are shared.
 */

typedef struct idtrie_node {
	struct idtrie_node* parent;
	uint64_t
		hasleaf: 1,
		hasbranch: 1,
		size : 62;
	// followed by 'index' bytes of key data
	// if 'hasbranch' then followed by idtrie_branch
} idtrie_node;

typedef struct idtrie_branch {
	uint64_t population[4]; // for popcount compression
	idtrie_node* children[1]; // variable length equal to popcount
} idtrie_branch;

typedef struct idtrie {
	idtrie_node* root;
} idtrie;

typedef struct id {
	idtrie_node* node;
} id;

id idtrie_intern(idtrie t, uint64_t l, int8_t* d);

void idtrie_remove(id i);

void* idtrie_fetch(id i);

