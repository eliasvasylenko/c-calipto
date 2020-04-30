/*
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 * Consider consolidating statement and expression into a single
 * concept "bound_sexpr".
 *
 * Also consider moving "bindings" and "bound sexpr" into sexpr.h
 * since they may be generally useful
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */

typedef struct binding {
	sexpr* name;
	sexpr* value;
} binding;

/*
 * TODO This has linear lookup time for bindings, this should
 * be rewritten with a proper table lookup
 */
typedef struct bindings {
	_Atomic(int32_t) ref_count;
	struct bindings* parent;
	int32_t count;
} bindings;

typedef struct statement {
	sexpr* form;
	bindings* bindings;
} statement;

typedef struct expression {
	sexpr* form;
	bindings* bindings;
} expression;

bindings* make_bindings(const bindings* p, int32_t c, const binding* b);

void free_bindings(bindings* p);

sexpr* resolve(const bindings* b, const sexpr* name);

void eval(const sexpr* e, const bindings* b);

