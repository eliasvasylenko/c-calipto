#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>

#include "c-ohvu/data/bdtrie.h"
#include "c-ohvu/data/sexpr.h"
#include "c-ohvu/runtime/evaluator.h"
#include "c-ohvu/runtime/compiler.h"
#include "compilerapi.h"

ovs_expr compile_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

ovs_function_info compile_inspect(const ovs_function_data* d) {
	return (ovs_function_info){ 3, 3 };
}

void compile_free(const void* d) {}

int32_t compile_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);

static ovs_function_type compile_function = {
	u"compile",
	compile_represent,
	compile_inspect,
	compile_apply,
	compile_free
};

int32_t compile_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	ovs_expr params = args[1];
	ovs_expr body = args[2];
	ovs_expr cont = args[3];

	compile_state* s = make_compile_state(NULL, f->context, cont.p);
	ovs_expr empty = ovs_root_symbol(OVS_DATA_NIL)->expr;

	i->size = 3;
	i->values[0] = ovs_alias(params);
	i->values[1] = parameters_function(s, empty, body.p, PARAMETERS_WITH);
	i->values[2] = parameters_function(s, empty, body.p, PARAMETERS_END);

	return 0;
}

ovs_expr ovru_compile(ovs_context* c) {
	return ovs_function(c, &compile_function, 0, NULL);
}

