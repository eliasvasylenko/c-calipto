#include <stdlib.h>
#include <uchar.h>

char32_t* utf8to32cps(char* text, int size) {
	char32_t* s = malloc(sizeof(char32_t) * size + 1);
	int i = 0;
	for (int j = 0; j < size; j++) {
		i += mbrtoc32(s + j, text + i, MB_CUR_MAX, NULL);
	}
	return s;
}

char32_t* utf8to32(char* text) {
	int i = 0;
	int s = 0;
	while (text[i] != 0) {
		i += mbrtoc32(NULL, text + i, MB_CUR_MAX, NULL);
		s++;
	}
	return utf8to32cps(text, s);
}

char32_t* utf8to32cs(char* text, int size) {
	int s = 0;
	for (int i = 0; i < size; i++) {
		i += mbrtoc32(NULL, text + i, MB_CUR_MAX, NULL);
		size++;
	}
	return utf8to32cps(text, s);
}

