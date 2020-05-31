#include <stdint.h>
#include <stdbool.h>

#include "c-calipto/idtrie.h"

id idtrie_intern_recur(idtrie_node* n, uint64_t o, uint64_t l, void* d) {
	void* data = (void*)(n + 1);

	if (l <= n->index) {
		for (int i = o; i < l; i++) {
			if (data[i] != d[i]) {
				return split(i);
			}
		}
		if (l < n->index) {
			split(l);

		} else {
			n.hasleaf = 1;
			return (id){ n };
		}

	} else { 

	}
	if (n->index <= 0) {
		int8_t index = popcnt(node.inner->population, 32);
		return s_intern_recur(qualifier, name, node.inner->children[index]);

	} else {
		s_expr_ref* leaf_qualifier = node.outer->symbol.qualifier;
		strref leaf_name = u_strnref(
				node.outer->symbol.name_length,
				node.outer->symbol.name);

		if (leaf_qualifier == qualifier
			&& !strref_cmp(leaf_name, name)) {
			return (s_expr_ref*)node.outer;

		} else {
			int32_t maxlen = strref_maxlen(name);
			s_expr_ref* r = ref(sizeof(s_symbol_data) + maxlen);
			int32_t len = strref_cpy(maxlen, (UChar*) &r->symbol.name, name);
			if (maxlen != len) {
				free(r);
				r = ref(sizeof(s_symbol_data) + len);
				strref_cpy(len, (UChar*) &r->symbol.name, name);
			}
			r->symbol.qualifier = qualifier;
			; // intern!

			return r;
		}
	}
}

id idtrie_intern(idtrie t, uint64_t l, void* d) {
	return idtrie_intern_recur(t.root, 0, l, d);
}
