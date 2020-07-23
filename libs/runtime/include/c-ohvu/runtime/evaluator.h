typedef struct ovru_bound_lambda {
	ovru_lambda* lambda;
	ovs_expr* closure;
} ovru_bound_lambda;

ovru_result ovru_eval(
		const ovru_statement s,
		ovs_context* c,
		const ovs_expr* args);

