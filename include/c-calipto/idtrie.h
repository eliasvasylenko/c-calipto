/*
 * This trie is designed for interning data. There is a two-way association
 * between keys and values. Keys are sequences of bytes, and values are
 * identifiers for unique keys.
 *
 * Entries are inserted by key, resulting in a unique, canonical ID. Entries
 * are then removed by ID.
 *
 * It is designed for the following case:
 * - Shared prefixes are expected to be relatively common amongst keys.
 * - Efficient storage is the highest priority.
 *
 * Storage of keys is distributed between nodes, so as to facilitate sharing.
 * This means less space is used, but key lookup for an ID may be slow as the
 * key needs to be reconstituted by walking up the tree.
 */

typedef struct idtrie_node {
	struct idtrie_node* parent;
	union {
		struct {
			bool
				hasleaf: 1,
				hasbranch: 1;
			uint64_t
				size : 62;
		};
		uint64_t layout;
	};
	// followed by 'index' bytes of key data
	// if 'hasbranch' then followed by idtrie_branch
	// if 'hasleaf' then followed by void*
} idtrie_node;

typedef struct idtrie_branch {
	uint64_t population[4]; // for popcount compression
	idtrie_node* children[1]; // variable length equal to popcount
} idtrie_branch;

typedef struct idtrie {
	idtrie_node** root;
	void* (*get_value)(void* key, idtrie_node* owner);
	void (*update_value)(void* value, idtrie_node* owner);
	void (*free_value)(void* value);
} idtrie;

void idtrie_clear(idtrie t);

void* idtrie_insert(idtrie t, uint64_t l, void* key);

void idtrie_remove(idtrie_node* n);

void* idtrie_fetch_key(idtrie_node* n);
