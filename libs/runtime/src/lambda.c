typedef struct bound_lambda {
	ovru_lambda* lambda;
	ovs_expr* closure;
} ovru_bound_lambda;

ovs_expr represent_param(const void* p) {
	return (ovs_expr){ OVS_SYMBOL, .p=*(ovs_expr_ref**)p };
}

ovs_expr bound_lambda_represent(const ovs_function_data* d) {
	const ovru_bound_lambda* l = (ovru_bound_lambda*)(d + 1);

	ovs_table* t = d->context->root_tables + OVS_DATA_LAMBDA;
	ovs_expr form[] = {
		ovs_list_of(t, l->lambda->param_count, (void**)l->lambda->params, represent_param),
		ovs_list(t, 0, NULL) // TODO
	};
	uint32_t size = sizeof(form) / sizeof(ovs_expr);

	ovs_expr r = ovs_list(t, 2, form);

	for (int i = 0; i < size; i++) {
		ovs_dealias(form[i]);
	}

	return r;
}

ovs_function_info bound_lambda_inspect(const ovs_function_data* d) {
	const ovru_bound_lambda* l = ovs_function_extra_data(d);

	return (ovs_function_info){ l->lambda->param_count, l->lambda->body.term_count };
}

void eval_statement(ovs_context* c, ovs_instruction* result, ovru_statement s, const ovs_expr* args, const ovs_expr* closure);

int32_t bound_lambda_apply(ovs_instruction* result, ovs_expr* args, const ovs_function_data* d) {
	const ovru_bound_lambda* l = ovs_function_extra_data(d);

	eval_statement(d->context, result, l->lambda->body, args + 1, l->closure);

	return OVRU_SUCCESS;
}

void bound_lambda_free(const void* d) {
	const ovru_bound_lambda* l = d;
	for (int i = 0; i < l->lambda->capture_count; i++) {
		ovs_dealias(l->closure[i]);
	}
	free(l->closure);
	free_lambda(l->lambda);
}

static ovs_function_type bound_lambda_function = {
	u"lambda",
	bound_lambda_represent,
	bound_lambda_inspect,
	bound_lambda_apply,
	bound_lambda_free
};

ovs_expr bind_lambda(ovru_lambda* l, ovs_expr* closure) {
		ovru_bound_lambda* b;
		ovs_expr f = ovs_function(
				s->context,
				&bound_lambda_function,
				sizeof(bound_lambda),
				(void**)&b);
		b->lambda = ref_lambda(l);
		b->closure = closure;
		for (int i = 0; i < l->capture_count; i++) {
			ovs_alias(b->closure[i]);
		}
		return f;
}

