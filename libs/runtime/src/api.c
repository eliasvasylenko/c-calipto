void* ref_of(ovs_expr e) {
	return (void*)e.p;
}

int32_t statement_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d) {
	statement_data* data = (statement_data*)(d + 1);

	if (data->type == STATEMENT_END) {
		// TODO
		return 91546;
	}

	ovru_term t;
	ovru_result r;
	ovs_expr cont;
	switch (data->type) {
		case STATEMENT_WITH_LAMBDA:
			ovs_expr params = args[0];
			ovs_expr body = args[1];
			// TODO
			break;

		case STATEMENT_WITH_VARIABLE:
			;
			ovs_expr variable = args[0];
			cont = args[1];

			t.type = OVRU_VARIABLE;
			r = find_variable(&t.variable, data->data->context, variable.p);
			break;

		case STATEMENT_WITH_QUOTE:
			;
			ovs_expr data = args[0];
			cont = args[1];

			ovru_term t = { .quote=data };
			r = OVRU_SUCCESS;
			break;

		default:
			assert(false);
	}

	if (r == OVRU_SUCCESS) {
		statement_function_data* fd = malloc(sizeof(statement_function_data));

		fd->context = data->data->context;
		fd->statement.term_count = data->data->statement.term_count + 1;
		fd->statement.terms = malloc(sizeof(ovru_term) * fd->statement.term_count);
		fd->parent = data->data->parent;
		for (int i = 0; i < data->data->statement.term_count; i++) {
			fd->statement.terms[i] = data->data->statement.terms[i];
		}
		fd->statement.terms[data->data->statement.term_count] = t;

		int choices = 4;
		statement_data* s[choices];

		i->size = choices + 1;
		i->values[0] = ovs_alias(cont);
		for (int j = 0; j < choices; j++) {
			i->values[j + 1] = ovs_function(
					d->context,
					&statement_function,
					sizeof(statement_data),
					(void**)&s[j]);
			s[j]->data = fd;
		}
		s[0]->type = STATEMENT_WITH_LAMBDA;
		s[1]->type = STATEMENT_WITH_VARIABLE;
		s[2]->type = STATEMENT_WITH_QUOTE;
		s[3]->type = STATEMENT_END;
	} else {
		// TODO fail
	}

	return r;
}
int32_t parameters_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d) {
	parameters_data* data = (parameters_data*)(d + 1);

	ovs_expr cont;
	switch (data->type) {
		case PARAMETERS_WITH:
			{
				ovs_expr param = args[0];
				cont = args[1];

				parameters_function_data* pfd = malloc(sizeof(parameters_function_data));
				pfd->params = ovs_cons(&d->context->root_tables[OVS_UNQUALIFIED], param, data->data->params);
				pfd->parent = data->data->parent;

				int choices = 2;
				parameters_data* p[choices];

				i->size = choices + 1;
				i->values[0] = ovs_alias(cont);
				for (int j = 0; j < choices; j++) {
					i->values[j + 1] = ovs_function(
							d->context,
							&parameters_function,
							sizeof(parameters_data),
							(void**)&p[j]);
					p[j]->data = pfd;
				}
				p[0]->type = PARAMETERS_WITH;
				p[1]->type = PARAMETERS_END;
			}
			break;

		case PARAMETERS_END:
			{
				cont = (ovs_expr){ OVS_FUNCTION, .p=data->data->statement_cont };

				statement_function_data* sfd = malloc(sizeof(statement_function_data));
				sfd->context = malloc(sizeof(compile_context));
				sfd->statement.term_count = 0;
				sfd->statement.terms = NULL;
				sfd->parent = data->data->parent;

				const ovs_expr_ref** param_list;
				int32_t param_count = ovs_delist_of(&d->context->root_tables[OVS_UNQUALIFIED], data->data->params, (void***)&param_list, ref_of);
				compile_parameters(sfd->context, d->context, param_count, param_list);

				int choices = 2;
				statement_data* s[choices];

				i->size = choices + 1;
				i->values[0] = ovs_alias(cont);
				for (int j = 0; j < choices; j++) {
					i->values[j + 1] = ovs_function(
							d->context,
							&statement_function,
							sizeof(statement_data),
							(void**)&s[j]);
					s[j]->data = sfd;
				}
				s[0]->type = STATEMENT_WITH_LAMBDA;
				s[1]->type = STATEMENT_WITH_VARIABLE;
			}
			break;

		default:
			assert(false);
	}
	return OVRU_SUCCESS;
}

int32_t compile_apply(ovs_instruction* i, ovs_expr* args, const ovs_function_data* d) {
	ovs_expr params = args[0];
	ovs_expr body = args[1];
	ovs_expr cont = args[2];

	parameters_function_data* pfd = malloc(sizeof(parameters_function_data));
	pfd->params = ovs_root_symbol(OVS_DATA_NIL)->expr;
	pfd->statement_cont = build.p;
	pfd->parent = cont.p;

	int choices = 2;
	parameters_data* p[choices];
	p[0]->type = PARAMETERS_WITH;
	p[1]->type = PARAMETERS_END;

	i->size = choices + 1;
	i->values[0] = ovs_alias(params);
	for (int j = 0; j < choices; j++) {
		i->values[j + 1] = ovs_function(
				d->context,
				&parameters_function,
				sizeof(parameters_data),
				(void**)&p[j]);
		p[j]->data = pfd;
	}

	return OVRU_SUCCESS;
}

ovs_expr ovru_compiler(ovs_context* c) {
	return ovs_function(c, &compile_function, 0, NULL);
}

