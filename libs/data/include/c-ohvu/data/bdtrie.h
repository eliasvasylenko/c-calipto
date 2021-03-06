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

struct bdtrie;

typedef struct bdtrie_node {
	union {
		struct bdtrie* trie;
		struct bdtrie_node* parent;
	};
	union {
		struct {
			uint8_t parent_index : 8;
			bool has_parent : 1;
			bool has_leaf: 1;
			uint32_t key_size : 14;
			uint8_t branch_size: 8;
		};
		uint32_t layout;
	};
	/*
	 * TODO if we need keys > 2^14 bytes we can just chain nodes
	 */

	// followed by 'keysize' bytes of key data
	// if 'hasleaf' then followed by bdtrie_leaf
	// if 'branchsize' then followed by bdtrie_branch
} bdtrie_node;

typedef struct bdtrie_leaf {
	uint32_t key_size;
	void* value;
} bdtrie_leaf;

typedef struct bdtrie_branch {
	uint64_t population[4]; // for popcount compression
	bdtrie_node* children[1]; // variable length equal to popcount
} bdtrie_branch;

/*
 * API surface
 */

typedef struct bdtrie_value {
	bdtrie_node* node;
	void* data;
} bdtrie_value;

typedef struct bdtrie {
	bdtrie_node* root;
	void* (*alloc_value)(uint32_t key_size, const void* key_data, const void* value_data, bdtrie_node* owner);
	void (*update_value)(void* value, bdtrie_node* owner);
	void (*free_value)(void* value);
} bdtrie;

bdtrie_value bdtrie_insert(bdtrie* t, uint32_t key_size, const void* key_data, const void* value_data);

bdtrie_value bdtrie_find(const bdtrie* t, uint32_t key_size, const void* key_data);

bdtrie_value bdtrie_find_or_insert(bdtrie* t, uint32_t key_size, const void* key_data, const void* value_data);

void bdtrie_delete(bdtrie_node* n);

bdtrie* bdtrie_trie(bdtrie_node* n);

uint32_t bdtrie_key(void* dest, bdtrie_node* n);

uint32_t bdtrie_key_size(bdtrie_node* n);

void bdtrie_clear(bdtrie* t);

bdtrie_value bdtrie_first(bdtrie* t);

bdtrie_value bdtrie_next(bdtrie_value v);

bool bdtrie_is_present(bdtrie_value v);

