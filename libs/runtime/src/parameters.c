#include "c-ohvu/data/sexpr.h"
#include "c-ohvu/runtime/evaluator.h"
#include "c-ohvu/runtime/compiler.h"
#include "compilerapi.h"

typedef struct parameters_data {
	compile_state* state;
	ovs_expr params;
	const ovs_expr_ref* cont;
} parameters_data;

ovs_expr parameters_represent(const ovs_function_data* d) {
	return ovs_symbol(d->context->root_tables + OVS_SYSTEM_BUILTIN, u_strlen(d->type->name), d->type->name);
}

void parameters_free(const void* d) {
	parameters_data* data = *(parameters_data**)d;

	free_compile_state(data->state);
	free(data);
}

ovs_function_info parameters_inspect(const ovs_function_data* d);

int32_t parameters_with_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);
int32_t parameters_end_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d);

static ovs_function_type parameters_with_function = {
	u"parameters-with",
	parameters_represent,
	parameters_inspect,
	parameters_with_apply,
	parameters_free
};

static ovs_function_type parameters_end_function = {
	u"parameters-end",
	parameters_represent,
	parameters_inspect,
	parameters_end_apply,
	parameters_free
};

ovs_function_info parameters_inspect(const ovs_function_data* d) {
	if (d->type == &parameters_with_function)
		return (ovs_function_info){ 2, 3 };

	else if (d->type == &parameters_end_function)
		return (ovs_function_info){ 1, 3 };

	assert(false);
}

ovs_expr parameters_function(compile_state* s, ovs_expr params, const ovs_expr_ref* cont, ovs_function_type* t) {
	parameters_data* p;
	ovs_expr e = ovs_function(s->context, t, sizeof(parameters_data*), (void**)&p);
	p->state = ref_compile_state(s);
	p->params = ovs_alias(params);
	p->cont = ovs_ref(cont);
	return e;
}

int32_t parameters_with_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	parameters_data* e = ovs_function_extra_data(f);

	ovs_expr param = args[1];
	ovs_expr cont = args[2];

	ovs_expr params = ovs_cons(&f->context->root_tables[OVS_UNQUALIFIED], param, e->params);

	i->size = 3;
	i->values[0] = ovs_alias(cont);
	i->values[1] = parameters_function(e->state, params, e->cont, &parameters_with_function);
	i->values[2] = parameters_function(e->state, params, e->cont, &parameters_end_function);

	ovs_dealias(params);

	return 0;
}

int32_t parameters_end_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* f) {
	parameters_data* e = ovs_function_extra_data(f);

	ovs_expr cont = (ovs_expr){ OVS_FUNCTION, .p=e->cont };

	compile_state* s;
	with_parameters(e->state, e->params);

	i->size = 3;
	i->values[0] = ovs_alias(cont);
	i->values[1] = statement_function(s, &statement_with_lambda_function);
	i->values[2] = statement_function(s, &statement_with_variable_function);

	return 0;
}

