
typedef struct {
	char* key;
	char* value;
}inikeyvalue;

typedef struct {
	char* name;
	inikeyvalue keyvalues;
}inisection;

typedef struct {
	char* data;
	inisection** sections;
	char path[MAX_PATH];
}inifile;



/* inifile data member
sectionname1
key1
value1
key2
value2
sectionname2
key1
value1


# PTR name      <- sect1
PTR k1
PTR v1
PTR k2
PTR v2
NULLPTR
# PTR name      <- sect2
PTR k1
PTR v1
NULLPTR


PTR sect2
PTR sect1
NULLPTR
*/

// note: this adds a null to the end for convenience when reading text files
int readFileAlloc(char* filePath, char** dataOut, DWORD* sizeOut, size_t extraBytes) {
	HANDLE hFile;
	if ((hFile = CreateFileA(filePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
		return -1;
	*sizeOut = GetFileSize(hFile, NULL);
	*dataOut = malloc(*sizeOut + 1 + extraBytes);
	(*dataOut)[*sizeOut] = 0;
	DWORD numRead = 0;
	if (ReadFile(hFile, *dataOut, *sizeOut, &numRead, NULL) == FALSE)
		return -2;
	CloseHandle(hFile);
	return 0;
}

int iniParse(char* filePath, inifile* out) {
	DWORD fileSize;
	char* fileContents;
	if (readFileAlloc(filePath, &fileContents, &fileSize, 0) != 0)
		return -1;

	int lineCount = 0;
	for (char* c = fileContents; c = strchr(c + 1, '\n'); lineCount++);

	int dataSize = fileSize + (lineCount + 1) * sizeof(char*) * 4;
	out->data = malloc(dataSize);
	if (filePath != out->path) {
		out->path[0] = 0;
		str_cat(out->path, filePath);
	}

	char** ptrs = (char**)(out->data + fileSize);
	int ptrIndex = 0;
	char** sectionPtr = (char**)(out->data + dataSize);
	sectionPtr--;
	sectionPtr[0] = 0;
	char* writePos = out->data;

	char* line = fileContents;
	while (line) {
		char* nextLine = strchr(line, '\n');
		nextLine += nextLine != 0;
		int lineLen = nextLine ? (int)(nextLine - line) : fileSize - (int)(line - fileContents);
		//remove \r and \n from the end
		lineLen -= (line[lineLen - 2] == '\r') + (line[lineLen - 1] == '\n');
		line[lineLen] = 0;
		//remove comments
		char* comment = strchr(line, ';');
		if (comment) {
			*comment = 0;
			lineLen = (int)(comment - line);
		}
		//remove trailing spaces
		for (char* c = line + lineLen - 1; c >= line && *c == ' '; c--) {
			*c = 0;
			lineLen--;
		}
		//
		if (line[0] == '[') {
			line[lineLen - 1] = 0;
			memcpy(writePos, line + 1, lineLen - 1);
			ptrIndex++;
			ptrs[ptrIndex] = writePos;
			sectionPtr--;
			*sectionPtr = (char*)(&ptrs[ptrIndex]);
			ptrs[ptrIndex + 1] = 0;
			ptrIndex += 1;
			writePos += lineLen + 1;
		}
		else {
			char* separator = strchr(line, '=');
			if (separator) {
				char* value = separator + 1;
				for (; *value == ' '; value++);
				*separator = 0;
				for (char* c = separator - 1; c >= line && *c == ' '; c--, separator--)
					*c = 0;
				int keyLen = (int)(separator - line);
				int valueLen = lineLen - (int)(value - line);
				memcpy(writePos, line, keyLen + 1);
				ptrs[ptrIndex] = writePos;
				writePos += keyLen + 1;
				memcpy(writePos, value, valueLen + 1);
				ptrs[ptrIndex + 1] = writePos;
				ptrs[ptrIndex + 2] = 0;
				writePos += valueLen + 1;
				ptrIndex += 2;
			}
		}
		line = nextLine;
	}
	free(fileContents);
	out->sections = (inisection**)sectionPtr;
	return 0;
}

inikeyvalue* iniGetSection(inifile* ini, char* sectionName) {
	for (inisection** section = ini->sections; *section; section++)
		if (strcmp((*section)->name, sectionName) == 0)
			return &(*section)->keyvalues;
	return 0;
}

char* iniGetValue(inifile* ini, char* section, char* key) {
	inikeyvalue* kv = iniGetSection(ini, section);
	if (kv)
		for (; kv->key; kv++)
			if (strcmp(kv->key, key) == 0)
				return kv->value;
	return 0;
}

//this is mega jank but we rarely set values so it will work for now
int iniSetValue(inifile* ini, char* section, char* key, char* value) {
	size_t sectionLen = strlen(section);
	size_t keyLen = strlen(key);
	size_t valueLen = strlen(value);
	size_t newLineLen = *value ? (keyLen + 3 + valueLen + 2) : 0;
	DWORD fileSize;
	char* fileContents;
	if (readFileAlloc(ini->path, &fileContents, &fileSize, newLineLen) != 0)
		return -1;
	int sectionFound = 0;
	char* line = fileContents;
	while (line) {
		char* nextLine = strchr(line, '\n');
		nextLine += nextLine != 0;
		int lineLen = nextLine ? (int)(nextLine - line) : fileSize - (int)(line - fileContents);
		if (!sectionFound && line[0] == '[' && lineLen > sectionLen && memcmp(line + 1, section, sectionLen) == 0) {
			sectionFound = 1;
		}
		if (sectionFound) {
			char* insertStart = 0;
			char* insertEnd = 0;
			if (lineLen > keyLen && memcmp(line, key, keyLen) == 0 && (line[keyLen] == ' ' || line[keyLen] == '=')) {
				insertStart = line;
				insertEnd = line + lineLen;
			}
			else if (!nextLine || (unsigned char)nextLine[0] <= 32 || nextLine[0] == '[') {
				insertStart = line+lineLen;
				insertEnd = insertStart;
			}
			if (insertStart) {
				size_t lengthDifference = newLineLen - (int)(insertEnd - insertStart);
				memmove(insertStart+newLineLen, insertEnd, (fileContents+fileSize) - insertEnd);
				if (*value) {
					if (!nextLine) {
						*(short*)insertStart = *(short*)"\r\n";
						insertStart += 2;
					}
					memcpy(insertStart, key, keyLen);
					insertStart += keyLen;
					*(int*)insertStart = *(int*)" = ";
					insertStart += 3;
					memcpy(insertStart, value, valueLen);
					insertStart += valueLen;
					if (nextLine)
						*(short*)insertStart = *(short*)"\r\n";
				}
				HANDLE hFile;
				if ((hFile = CreateFileA(ini->path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
					free(fileContents);
					return -1;
				}
				DWORD numWritten;
				WriteFile(hFile, fileContents, fileSize+(DWORD)lengthDifference, &numWritten, NULL);
				CloseHandle(hFile);
				free(fileContents);
				free(ini->data);
				return iniParse(ini->path, ini);
			}
		}
		line = nextLine;
	}
	free(fileContents);
	return -1;
}


