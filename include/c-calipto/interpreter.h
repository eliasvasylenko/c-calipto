/*
 * Associative trie for binding symbols to indices. This is how the array of
 * variables captured by a lambda is populated from the enclosing scope.
 *
 * All keys are known before-hand, so we can allocate every node into one flat
 * block of memory. We only need to support update and retrieval of keys which
 * are present. Keys are pointers to interned symbols.
 *
 * Node arrays use popcnt compression, and the structure uses prefix compression.
 *
 * We never need to synchronize access.
 */
typedef struct s_bindings {
	uint32_t node_count; // only needed for fast copying

	/*
	 * Tree from symbols to expression.
	 *
	 * Since symbols are interned, we can distinguish them uniquely by pointer.
	 *
	 * This means we have a unique 32/64 bit key ready to go, so perhaps a clever
	 * trie implementation can have good results and be simple.
	 *
	 * For some tables (e.g. captured bindings) we have a small, fixed set of keys
	 * and want to make many copies of the table with different values. To achieve
	 * this perhaps we can find a perfect hash for a set of keys? Or just make a
	 * best-effort to find an offset into the pointer data which already produces
	 * a unique byte/nibble for the pointer data. In those cases we might also be
	 * able to assume that all lookups will succeed in which case can omit key.
	 *
	 * Can also try popcount compression on resulting array.
	 */
	struct s_bindings_node {
		bool leaf;
		union {
			struct {
				uint8_t offset;
				uint16_t population;
				struct s_bindings_node* children;
			};
			s_expr value;
		};
	}* nodes;
} s_bindings;

typedef struct s_bound_arguments {
	s_bindings bindings;
	s_expr* values;
} s_bound_arguments;

s_bindings s_prepare_bindings(s_expr_ref** symbols);
s_bindings s_free_bindings(s_bindings);

s_bound_arguments s_bind_arguments(s_expr_ref** symbols, s_expr* v);
void s_unbind_arguments(s_bound_arguments b);

void s_eval(const s_expr e, const s_bound_arguments b);

