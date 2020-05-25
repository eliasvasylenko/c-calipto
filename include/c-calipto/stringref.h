typedef enum strref_type {
	CSTR,
	USTR
} strref_type;

typedef struct strref {
	strref_type type;
	int32_t size;
	UConverter* conv;
	union {
		const char* c_chars;
		const UChar* u_chars;
	};
	int32_t len;
} strref;

strref c_strref(UConverter* conv, const char* chars);
strref c_strnref(UConverter* conv, int32_t size, const char* chars);
strref u_strref(const UChar* chars);
strref u_strnref(int32_t size, const UChar* chars);

uint32_t strref_maxlen(strref s);
uint32_t strref_cpy(int32_t destSize, UChar* dest, strref s);
uint32_t strref_cmp(strref a, strref b);
