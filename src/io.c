typedef struct system_in_data {
	UFILE* file;
	s_expr file_name;
	s_expr* next;
	e_expr* text;
} system_in_data;

s_expr system_in_represent(void* d) {
	;
}

bool system_in(s_statement* b, s_expr* args, void* d) {
	s_expr fail = args[0];
	s_expr cont = args[1];

	s_expr* bindings = malloc(sizeof(s_expr) * 1);
	bindings[0] = fail;
	*b = (s_statement){ 1, terms, bindings };

	return true;
}

void system_in_free(void* d) {
	system_in_data data = *(system_in_data*)d;

	if (data.next) {
		s_dealias(*data.next);
		free(data.next);

		if (data.text) {
			free(data.text);
		}
	} else if (data.file) {
		u_fclose(data.file);
	}

	free(d);
}

typedef struct system_out_data {
	UFILE* file;
	s_expr* next;
	UChar* text;
} system_out_data;

bool system_out(s_statement* b, s_expr* args, void* d) {
	s_expr string = args[0];
	s_expr fail = args[1];
	s_expr cont = args[2];
	
	system_out_data data = *(system_out_data*)d;

	if (string.type != STRING) {
		s_expr* bindings = malloc(sizeof(s_expr) * 1);
		bindings[0] = fail;
		*b = (s_statement){ 1, terms, bindings };

	} else if (data.next == NULL) {
		u_fprintf(data.file, "%S", &string.p->string);

		s_expr* bindings = malloc(sizeof(s_expr) * 1);
		bindings[0] = cont;
		*b = (s_statement){ 1, terms, bindings };

	} else {


	}
	return true;
}

void system_out_free(void* d) {
	system_out_data data = *(system_out_data*)d;

	if (data.next) {
		s_dealias(*data.next);
		free(data.next);

		if (data.text) {
			free(data.text);
		}
	} else if (data.file) {
		u_fclose(data.file);
	}

	free(d);
}

static s_function_type s_scanner = {
	u"scan",
	system_in_represent,
	2,
	system_in,
	system_in_free
};

s_expr cal_open_scanner(UFILE* f, s_expr file_name) {
	system_in_data data = { f, n, NULL, NULL };
	return s_function(*s_function_type, sizeof(system_in_data), &data);
}
