/*
 * This design is very slow! Linear time
 * lookup is truly abysmal for a binding
 * table.
 *
 * It's just here as a quick proof of
 * concept.
 */

typedef struct binding {
	sexpr* name;
	sexpr* value;
} binding;

typedef struct bindings {
	struct bindings* parent;
	int32_t binding_count;
} bindings;

bindings* collect_bindings(bindings* p, int32_t c, binding* b);

sexpr* resolve(bindings* b, sexpr* name);

sexpr* eval(sexpr* e, bindings* b);
