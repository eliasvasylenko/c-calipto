s_expr no_represent(void* d) { return s_list(0, NULL); }

void no_free(void* d) {}



static s_function_type exit = {
	u"scan",
	no_represent,
	0,
	exit_apply,
	no_free
};

s_expr cal_exit() {
	return s_function(*exit, 0, NULL);
}

typedef struct scanner_data {
	UFILE* file;
	s_expr file_name;
	s_expr* next;
	e_expr* text;
} scanner_data;

s_expr scanner_represent(void* d) {
	;
}

bool scanner_apply(s_statement* b, s_expr* args, void* d) {
	s_expr fail = args[0];
	s_expr cont = args[1];

	s_expr* bindings = malloc(sizeof(s_expr) * 1);
	bindings[0] = fail;
	*b = (s_statement){ 1, terms, bindings };

	return true;
}

void scanner_free(void* d) {
	scanner_data data = *(scanner_data*)d;

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

static s_function_type scanner = {
	u"scan",
	scanner_represent,
	2,
	scanner_apply,
	scanner_free
};

s_expr cal_open_scanner(UFILE* f, s_expr file_name) {
	scanner_data data = { f, n, NULL, NULL };
	return s_function(*scanner, sizeof(scanner_data), &data);
}
typedef struct printer_data {
	UFILE* file;
	s_expr* next;
	UChar* text;
} printer_data;

s_expr printer_represent(void* d) {
	;
}

bool printer_apply(s_statement* b, s_expr* args, void* d) {
	s_expr string = args[0];
	s_expr fail = args[1];
	s_expr cont = args[2];
	
	printer_data data = *(printer_data*)d;

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

void printer_free(void* d) {
	printer_data data = *(printer_data*)d;

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

static s_function_type printer = {
	u"print",
	printer_represent,
	2,
	printer_apply,
	printer_free
};

s_expr cal_open_printer(UFILE* f, s_expr file_name) {
	printer_data data = { f, n, NULL, NULL };
	return s_function(*printer, sizeof(printer_data), &data);
}

bool data_cons(s_statement* b, s_expr* args, void* d) {
	s_expr car = args[0];
	s_expr cdr = args[1];
	s_expr cont = args[2];

	s_expr* bindings = malloc(sizeof(s_expr) * 2);
	bindings[0] = cont;
	bindings[1] = s_cons(car, cdr);
	*b = (s_statement){ 2, terms, bindings };

	return true;
}

bool data_des(s_statement* b, s_expr* args, void* d) {
	s_expr e = args[0];
	s_expr fail = args[1];
	s_expr cont = args[2];

	if (s_atom(e)) {
		s_expr* bindings = malloc(sizeof(s_expr) * 1);
		bindings[0] = fail;
		*b = (s_statement){ 1, terms, bindings };

	} else {
		s_expr* bindings = malloc(sizeof(s_expr) * 3);
		bindings[0] = cont;
		bindings[1] = s_car(e);
		bindings[2] = s_cdr(e);
		*b = (s_statement){ 3, terms, bindings };
	}
	return true;
}

bool data_eq(s_statement* b, s_expr* args, void* d) {
	s_expr e_a = args[0];
	s_expr e_b = args[1];
	s_expr f = args[2];
	s_expr t = args[3];

	if (s_eq(e_a, e_b)) {
		s_expr* bindings = malloc(sizeof(s_expr) * 1);
		bindings[0] = t;
		*b = (s_statement){ 1, terms, bindings };
	} else {
		s_expr* bindings = malloc(sizeof(s_expr) * 1);
		bindings[0] = f;
		*b = (s_statement){ 1, terms, bindings };
	}

	return true;
}

