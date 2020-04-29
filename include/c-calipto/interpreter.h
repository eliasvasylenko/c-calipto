typedef struct binding {
	sexpr* name;
	sexpr* value;
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

