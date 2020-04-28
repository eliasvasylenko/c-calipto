/*
 * This design is very slow! Linear time
 * lookup is truly abysmal for a binding
 * table.
 *
 * It's just here as a quick proof of
 * concept.
 */

/*
 *
 *
 *
 *
 *
 * TODO we need to get rid "term" as a distinct notion from
 * "sexpr". The reason is that having to cons terms means
 * too much duplication. Simpler to just fold the special
 * behavior directly into the data model.
 *
 * The problem then is that the two layers are nolonger
 * nicely decoupled. That's not a huge deal though.
 *
 *
 *
 *
 */

typedef enum term_type {
	DATA,
	FUNCTION,
	BUILTIN,
	ERROR
} term_type;

typedef struct term {
	term_type type;
	void* data;
} term;

typedef struct binding {
	sexpr* name;
	term value;
} binding;

typedef struct bindings {
	_Atomic(int32_t) ref_count;
	struct bindings* parent;
	int32_t count;
} bindings;

bindings* make_bindings(const bindings* p, int32_t c, const binding* b);

void free_bindings(bindings* p);

term resolve(const bindings* b, const sexpr* name);

void eval(const sexpr* e, const bindings* b);
