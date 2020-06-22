typedef enum ovio_strref_type {
	OVIO_CSTR,
	OVIO_USTR
} ovio_strref_type;

typedef struct ovio_strref {
	ovio_strref_type type;
	int32_t size;
	UConverter* conv;
	union {
		const char* c_chars;
		const UChar* u_chars;
	};
	int32_t len;
} ovio_strref;

ovio_strref ovio_c_strref(UConverter* conv, const char* chars);
ovio_strref ovio_c_strnref(UConverter* conv, int32_t size, const char* chars);
ovio_strref ovio_u_strref(const UChar* chars);
ovio_strref ovio_u_strnref(int32_t size, const UChar* chars);

uint32_t ovio_strref_maxlen(ovio_strref s);
uint32_t ovio_strref_cpy(int32_t destSize, UChar* dest, ovio_strref s);
uint32_t ovio_strref_cmp(ovio_strref a, ovio_strref b);
