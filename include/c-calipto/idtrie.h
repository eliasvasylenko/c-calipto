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

/*
 * Internal structure
 */

typedef struct idtrie_node {
	struct idtrie_node* parent;
	union {
		struct {
			bool
				hasleaf: 1,
				hasbranch: 1;
			uint32_t
				size : 30;
		};
		uint32_t layout;
	};
	// followed by 'index' bytes of key data
	// if 'hasleaf' then followed by idtrie_leaf
	// if 'hasbranch' then followed by idtrie_branch
} idtrie_node;

typedef struct idtrie_leaf {
	uint32_t key_size;
	void* value;
} idtrie_leaf;

typedef struct idtrie_branch {
	uint64_t population[4]; // for popcount compression
	idtrie_node* children[1]; // variable length equal to popcount
} idtrie_branch;

/*
 * API surface
 */

typedef struct idtrie_value {
	idtrie_node* node;
	void* data;
} idtrie_value;

typedef struct idtrie {
	idtrie_node* root;
	void* (*get_value)(uint32_t key_size, void* key_data, idtrie_node* owner);
	void (*update_value)(void* value, idtrie_node* owner);
	void (*free_value)(void* value);
} idtrie;

idtrie_value idtrie_insert(idtrie* t, uint32_t key_size, void* key_data);

idtrie_value idtrie_find(idtrie* t, uint32_t key_size, void* key_data);

void idtrie_delete(idtrie_node* n);

uint32_t idtrie_key(void* dest, idtrie_node* n);

uint32_t idtrie_key_size(idtrie_node* n);

void idtrie_clear(idtrie t);

