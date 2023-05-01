int _fltused = 1;

unsigned char lowercaseLookup[] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255 };

unsigned int str_cat(char* dst, char* src) {
    char* c = dst;
    for (; *c; c++);
    for (char* s = src; *c = *s; s++, c++);
	return (int)(c - dst);
}

unsigned int str_vacat(char* dst, int argc, ...) {
	char* c = dst;
	va_list ap;
	va_start(ap, argc);
	for (int i = 0; i < argc; i++)
		c += str_cat(c, va_arg(ap, char*));
	va_end(ap);
	return (int)(c - dst);
}

char* strstr(const char* str1, const char* str2) {
	char* c1 = (char*)str1, * c2 = (char*)str2;
	for (; *c1 && *c2; c2 = (*c1 == *c2 ? c2 + 1 : str2), c1++);
	return *c2 ? 0 : c1 - (c2 - str2);
}

char* strstrl(const char* str1, const char* str2) {
	char* c1 = (char*)str1, * c2 = (char*)str2;
	for (; *c1 && *c2; c2 = (lowercaseLookup[(unsigned char)*c1] == lowercaseLookup[(unsigned char)*c2] ? c2 + 1 : str2), c1++);
	return *c2 ? 0 : c1 - (c2 - str2);
}

#pragma function(memcpy)
void* memcpy(void* dst, void* src, size_t num) {
	while (num--)
		*((unsigned char*)dst)++ = *((unsigned char*)src)++;
	return dst;
}

void* memcpy_r(void* dst, void* src, size_t num) {
	while (num--)
		*((unsigned char*)dst + num) = *((unsigned char*)src + num);
	return dst;
}


#pragma function(memset)
void* memset(void* ptr, int value, size_t num) {
	while(num--)
		((unsigned char*)ptr)[num] = (unsigned char)value;
	return ptr;
}

#pragma function(memcmp)
int memcmp(const void* ptr1, const void* ptr2, size_t num) {
	int diff = 0;
	for (int i = 0; i < num && diff == 0; i++) {
		diff = ((unsigned char*)ptr1)[i] - ((unsigned char*)ptr2)[i];
	}
	return diff;
}

#pragma function(memmove)
void* memmove(void* dst, void* src, size_t num) {
	return dst > src ? memcpy_r(dst, src, num) : memcpy(dst, src, num);
}

#pragma function(strcmp)
int strcmp(const char* str1, const char* str2) {
	int diff = 0;
	for (; !(diff = *str1 - *str2) && *str1 && *str2; str1++, str2++);
	return diff;
}

#pragma function(strlen)
size_t strlen(const char* str) {
	size_t len = 0;
	while (*(str++))
		len++;
	return len;
}

void* malloc(size_t size) {
	return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void free(void* ptr) {
	VirtualFree(ptr, 0, MEM_RELEASE);
}

char* strchr(const char* str, int character) {
	for (; *str && *str != (char)character; str++);
	return (*str == (char)character) ? (char*)str : 0;
}

char* rstrchr(const char* str, const char* searchPos, int character) {
	for (; searchPos >= str && *searchPos != (char)character; searchPos--);
	return (*searchPos == (char)character) ? (char*)searchPos : 0;
}

int ipow(int base, int exponent) {
	if (!exponent)
		return 1;
	int res = base;
	while (exponent-- > 1)
		res *= base;
	return res;
}

int str_getint(char* str) {
	int sign = 1;
	if (*str == '-') {
		sign = -1;
		str++;
	}
	unsigned char* digit = str;
	for (; *digit >= '0' && *digit <= '9'; digit++);
	digit--;
	int total = 0;
	for (int i = 0; digit >= str; i++, digit--)
		total += (*digit - '0') * ipow(10, i);
	return total * sign;
}

int str_gethex(char* str, int* digitCount) {
	char lookup[] = { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,-1,-1,-1,-1,-1,-1,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,0,-1,-1,-1,-1,-1,-1,-1,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 };
	unsigned char* digit = str;
	for (; lookup[*digit] >= 0; digit++);
	if (digitCount)
		*digitCount = (int)(digit - str);
	digit--;
	int total = 0;
	for (int i = 0; digit >= str; i++, digit--)
		total += lookup[*digit] * ipow(16, i);
	return total;
}

int uint_to_str(unsigned int num, char* str) {
	char tmp[16] = { 0 };
	tmp[14] = '0';
	int i = (num == 0);
	for (; num > 0; i++) {
		int b = num % ipow(10, i + 1);
		tmp[14 - i] = '0' + b / ipow(10,i);
		num -= b;
	}
	if(str)
		memcpy(str, tmp + 15 - i, i+1);
	return i;
}

void strrep(char* str, char* find, char* replace) {
	int findLen = (int)strlen(find);
	int repLen = (int)strlen(replace);
	for (char* found = str; found = strstr(found, find);) {
		memmove(found + findLen + (repLen-findLen), found + findLen, strlen(found + findLen) + 1);
		memcpy(found, replace, repLen);
		found += repLen;
	}
}

void strrepl(char* str, char* find, char* replace) {
	int findLen = (int)strlen(find);
	int repLen = (int)strlen(replace);
	for (char* found = str; found = strstrl(found, find);) {
		memmove(found + findLen + (repLen - findLen), found + findLen, strlen(found + findLen) + 1);
		memcpy(found, replace, repLen);
		found += repLen;
	}
}

char* lowercase(char* str) {
	for (char* c = str; *c; c++)
		*c = lowercaseLookup[(unsigned char)*c];
	/*for (char* c = str; *c; c++)
		if (*c >= 'A' && *c <= 'Z')
			*c += 32;*/
	return str;
}