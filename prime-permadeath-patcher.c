#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#define BUF_SIZE 2048

int detect_version(FILE* const pFile);
int find_binary_pattern(unsigned char* const pBuf, const size_t bufSize, unsigned char* const pPattern, const size_t patternSize, unsigned long* outValue);
int patch_file(FILE* const pFile, unsigned long* const pAOffsets, const int numAOffsets, unsigned long* const pBOffsets, const int numBOffsets, const int unpatch);
void print_help();
int read_bytes(FILE* const pFile, const unsigned long address, const size_t readSize, void* const outBuf);
int scan_file(FILE* const pFile, unsigned long* const outAOffsets, int* const outNumAOffsets, unsigned long* const outBOffsets, int* const outNumBOffsets);
int write_bytes(FILE* const pFile, const unsigned long address, unsigned long* const pBytes, const size_t bytesSize);

#ifdef _WIN32
static OPENFILENAMEA w32_OpenFilename;
#endif

enum
{
	PARAM_CHECK = 1 << 1,
	PARAM_PATCH = 1 << 2,
	PARAM_UNPATCH = 1 << 3
};

enum 
{
	VERSION_INVALID = -1,
	VERSION_USA_000,
	VERSION_USA_001,
	VERSION_USA_002,
	VERSION_EUR,
	VERSION_JAP,
	VERSION_KOR,
	VERSION_COUNT
};

const unsigned char filePatterns[2][20] = {
	{ 0x57, 0xE4, 0x06, 0x3E, 0x4B, 0xFC, 0x85, 0x59, 0xC0, 0x02, 0x85, 0x68, 0xFC, 0x1D, 0x00, 0x40, 0x4C, 0x40, 0x13, 0x82 },
	{ 0x80, 0x83, 0x00, 0x58, 0x28, 0x00, 0x00, 0x10, 0x80, 0xA4, 0x00, 0xC4 }
};

const unsigned long filePatternsSizesH[2] = {
	0x14,
	0xC
};

const unsigned char patchBytes[2][4] = {
	{ 0x60, 0x00, 0x00, 0x00 },
	{ 0x48, 0x00, 0x03, 0x6C }
};

const unsigned char unpatchBytes[2][4] = {
	{ 0x40, 0x82, 0x00, 0x7C },
	{ 0x41, 0x81, 0x03, 0x78 }
};

const char* validArgs[4] = { "-check", "-patch", "-unpatch" };

const char* versionStrs[VERSION_COUNT] = { "USA v1.0", "USA v1.1", "USA v1.2", "Europe", "Japan", "Korea" };

static char win32Filename[1024];

static int gUseWin32GUI = 0;

static int gVersion = VERSION_INVALID;

int main(int argc, char* argv[]) {

	unsigned char flgParams = 0;
	for (int i = 0; i < argc; i++) {

		for (int j = 0; j < 3; j++) {

			if (!strcmp(argv[i], validArgs[j])) {
				flgParams |= 1 << (j + 1);
				break;
			}

		}

	}

#ifdef _WIN32

	DWORD bufProcessList[2];
	if (GetConsoleProcessList(&bufProcessList, 2) == 1) { 
		
		gUseWin32GUI = 1;

		flgParams |= PARAM_CHECK;

		printf("NOTE: This program can also be run from console\n");
	}

#endif

	if (!gUseWin32GUI && (argc < 3 || !flgParams)) {
		print_help();
		return 0;
	}

#ifdef _WIN32

	if (gUseWin32GUI) {
		memset(&win32Filename, 0, sizeof(win32Filename));
		win32Filename[0] = "\0";

		memset(&w32_OpenFilename, 0, sizeof(OPENFILENAMEA));
		w32_OpenFilename.lStructSize = sizeof(OPENFILENAMEA);
		w32_OpenFilename.hwndOwner = 0;
		w32_OpenFilename.hInstance = 0;
		w32_OpenFilename.lpstrFilter = "ISO Files (*.iso)\0*.iso\0\0";
		w32_OpenFilename.lpstrCustomFilter = 0;
		w32_OpenFilename.nMaxCustFilter = 0;
		w32_OpenFilename.nFilterIndex = 0;
		w32_OpenFilename.lpstrFile = win32Filename;
		w32_OpenFilename.nMaxFile = sizeof(win32Filename);
		w32_OpenFilename.lpstrTitle = "Select Metroid Prime ISO";
		w32_OpenFilename.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
		w32_OpenFilename.lpstrDefExt = "iso";

		printf("Select Metroid Prime .iso file...\n");

		if (!GetOpenFileNameA(&w32_OpenFilename)) {
			MessageBoxA(GetActiveWindow(), "Error opening file, or none selected", "Patcher Error", MB_ICONERROR);
			return -1;
		}
	}

#endif

	FILE* pFile = fopen(gUseWin32GUI ? &win32Filename : argv[1], "r+b");

	if (!pFile) {
		printf("Error opening file \"%s\" for patching\n", argv[1]);
		return -1;
	}

	gVersion = detect_version(pFile);

	if (gVersion == VERSION_INVALID) {
		printf("Error: Invalid file (could not detect version)\n");
		return -1;
	}
	
	printf("Version Detected: %s\n", versionStrs[gVersion]);
	
	switch (gVersion) {
	case VERSION_USA_000:
	case VERSION_USA_001:
	case VERSION_KOR:
		break;
	default:
		printf("Error: Unsupported game version. Current supported versions are %s, %s, %s\n", versionStrs[VERSION_USA_000], versionStrs[VERSION_USA_001], versionStrs[VERSION_KOR]);
		return -1;
	}

	unsigned long patternAOffsets[10];
	unsigned long patternBOffsets[10];
	int numAOffsets, numBOffsets;

	printf("Scanning image, please wait...\n");
	const int scanStatus = scan_file(pFile, &patternAOffsets, &numAOffsets, &patternBOffsets, &numBOffsets);

	if (scanStatus == -1) {
		printf("Error: Invalid file (too many pattern matches)\n");
		return -1;
	}

	int opSuccess = 0;

	if (flgParams & PARAM_CHECK) {

		printf("Patch is ");

		switch (scanStatus) {
		case 0:
			printf("installed.\n");
			break;
		case 1:
			printf("partially installed, or file is invalid.\n");
			break;
		case 2:
			printf("not installed.\n");
			break;
		default:
			printf("<error>\n");
			break;
		}

	}

#ifdef _WIN32

	if (gUseWin32GUI) {

		if (MessageBoxA(NULL, scanStatus ? "Patch is not installed.\nInstall patch?" : "Patch is installed.\nRemove patch?", "Patcher", MB_YESNO) == IDYES) {

			if (scanStatus) {
				flgParams |= PARAM_PATCH;
			}
			else {
				flgParams |= PARAM_UNPATCH;
			}

		}
	}

#endif
	
	if (flgParams & PARAM_PATCH) {

		printf("Patching image...\n");

		opSuccess = patch_file(pFile, &patternAOffsets, numAOffsets, &patternBOffsets, numBOffsets, 0);

		if (!opSuccess) {

#ifdef _WIN32
			if (gUseWin32GUI) { MessageBoxA(NULL, "An error occurred while patching the image.", "Patcher Error", MB_ICONERROR); }
#endif
			printf("Error while patching image: ferror %d\nAborting...\n", ferror(pFile));
			fclose(pFile);
			return -1;
		}

		printf("Patch successful!\n");

	}
	else if (flgParams & PARAM_UNPATCH) {

		printf("Removing patch...\n");

		opSuccess = patch_file(pFile, &patternAOffsets, numAOffsets, &patternBOffsets, numBOffsets, 1);

		if (!opSuccess) {

#ifdef _WIN32
			if (gUseWin32GUI) { MessageBoxA(NULL, "An error occurred while removing the patch.", "Patcher Error", MB_ICONERROR); }
#endif
			printf("Error while un-patching image: ferror %d\nAborting...\n", ferror(pFile));
			fclose(pFile);
			return -1;
		}

		printf("Patch removed!\n");
	}

#ifdef _WIN32

		if (gUseWin32GUI && opSuccess) {
			MessageBoxA(NULL, "Operation Successful!", "Patcher", MB_ICONASTERISK);
		}

#endif

	printf("Done\n");

	fclose(pFile);

	return 1;
}

int detect_version(FILE* const pFile) {

	static const unsigned char fileGamecodeBytes[3][6] = {

		{ 0x47, 0x4D, 0x38, 0x45, 0x30, 0x31 },
		{ 0x47, 0x4D, 0x38, 0x50, 0x30, 0x31 },
		{ 0x47, 0x4D, 0x38, 0x4A, 0x30, 0x31 }

	};

	static const unsigned char fileVersionByte[6] = { 0x00, 0x01, 0x02, 0x00, 0x00, 0x30 };

	unsigned char versionBuf;
	if (!read_bytes(pFile, 0x07, sizeof(unsigned char), &versionBuf)) {
		printf("Error reading file: ferror %d\n", ferror(pFile));
		return -1;
	}

	if (!memcmp(&versionBuf, &fileVersionByte[VERSION_KOR], sizeof(unsigned char))) {
		return VERSION_KOR;
	}
	if (!memcmp(&versionBuf, &fileVersionByte[VERSION_USA_001], sizeof(unsigned char))) {
		return VERSION_USA_001;
	}
	if (!memcmp(&versionBuf, &fileVersionByte[VERSION_USA_002], sizeof(unsigned char))) {
		return VERSION_USA_002;
	}

	unsigned char gamecodeBuf[6];
	if (!read_bytes(pFile, 0x00, sizeof(gamecodeBuf), &gamecodeBuf)) {
		printf("Error reading file: ferror %d\n", ferror(pFile));
		return -1;
	}

	if (!memcmp(&gamecodeBuf, &fileGamecodeBytes[0], sizeof(gamecodeBuf))) {
		return VERSION_USA_000;
	}
	if (!memcmp(&gamecodeBuf, &fileGamecodeBytes[1], sizeof(gamecodeBuf))) {
		return VERSION_EUR;
	}
	if (!memcmp(&gamecodeBuf, &fileGamecodeBytes[2], sizeof(gamecodeBuf))) {
		return VERSION_JAP;
	}

	return VERSION_INVALID;
}

int find_binary_pattern(unsigned char* const pBuf, const size_t bufSize, unsigned char* const pPattern, const size_t patternSize, unsigned long* outValue) {

	int c = 0;
	int lps[64];
	for (size_t i = 0; i < 64; i++) { lps[i] = 0; }

	for (size_t i = 1; i < patternSize; i++) {

		if (pPattern[i] == pPattern[c]) {
			c++;
			i++;
			lps[i] = c;
		}
		else {

			if (c) {
				c = lps[c - 1];
			}
			else {
				lps[i] = 0;
				i++;
			}
		}
		
	}

	int i = 0, j = 0;

	while (i < bufSize) {

		if (pPattern[j] == pBuf[i]) {
			i++;
			j++;
		}

		if (j == patternSize) {
			*outValue = (unsigned long)(i - j);
			return 1;
		}
		else if (pPattern[j] != pBuf[i]) {
			if (j) {
				j = lps[j - 1];
			}
			else {
				i++;
			}
		}
	}

	return 0;
}

int patch_file(FILE* const pFile, unsigned long* const pAOffsets, const int numAOffsets, unsigned long* const pBOffsets, const int numBOffsets, const int unpatch) {

	int successA, successB;
	unsigned long writeAddrA, writeAddrB;

	for (int i = 0; i < ((numAOffsets >= numBOffsets) ? numAOffsets : numBOffsets); i++) {

		writeAddrA = pAOffsets[i] + filePatternsSizesH[0];
		writeAddrB = pBOffsets[i] + filePatternsSizesH[1];

		if (i < numAOffsets) {
			successA = write_bytes(pFile, writeAddrA, unpatch ? unpatchBytes[0] : patchBytes[0], 4U);
		}
		else {
			successA = 1;
		}

		if (i < numBOffsets) {
			successB = write_bytes(pFile, writeAddrB, unpatch ? unpatchBytes[1] : patchBytes[1], 4U);
		}
		else {
			successB = 1;
		}

		if (!(successA && successB)) {

			if (!successA) {
				printf("fwrite to 0x%x in file failed\n", writeAddrA);
			}

			if (!successB) {
				printf("fwrite to 0x%x in file failed\n", writeAddrB);
			}

			return 0;
		}
	}

	return 1;
}

void print_help() {

	printf("Metroid Prime Permadeath Patcher v1.0 - by mills5\n");
	printf("Usage:			prime-permadeath-patcher <input .iso filename> <option>\n");
	printf("Options:		-check   | Check if permadeath patch is installed.\n");
	printf("			-patch   | Install the permadeath patch.\n");
	printf("			-unpatch | Remove the permadeath patch.\n");

}

int read_bytes(FILE* const pFile, const unsigned long address, const size_t readSize, void* const outBuf) {

	fseek(pFile, address, 0);

	if (fread(outBuf, 1, readSize, pFile) != readSize) { return 0; }

	return 1;
}

int scan_file(FILE* const pFile, unsigned long* const outAOffsets, int* const outNumAOffsets, unsigned long* const outBOffsets, int* const outNumBOffsets) {

	unsigned char readBuf[BUF_SIZE];
	size_t readBytes;
	int patternAFound = 0, patternBFound = 0;
	unsigned int mult = 0;
	unsigned long outValue;

	fseek(pFile, 0, 0);

	while ((readBytes = fread(&readBuf, 1, sizeof(readBuf), pFile)) > 0) {

		if (find_binary_pattern(readBuf, BUF_SIZE, filePatterns[0], sizeof(filePatterns[0]), &outValue)) {
			outAOffsets[patternAFound] = outValue + (readBytes * mult);
			patternAFound++;
		}

		if (find_binary_pattern(readBuf, BUF_SIZE, filePatterns[1], 12U, &outValue)) {
			outBOffsets[patternBFound] = outValue + (readBytes * mult);
			patternBFound++;
		}

		if (patternAFound > 10 || patternBFound > 10) { return -1; }

		mult++;
	}

	int patchAInstalled = 0, patchBInstalled = 0;

	for (mult = 0; mult < patternAFound; mult++) {

		outValue = outAOffsets[mult] + filePatternsSizesH[0];

		read_bytes(pFile, outValue, 4U, &readBuf);

		if (!memcmp(readBuf, patchBytes[0], 4U)) {
			patchAInstalled++;
		}
	}

	for (mult = 0; mult < patternBFound; mult++) {

		outValue = outBOffsets[mult] + filePatternsSizesH[1];

		read_bytes(pFile, outValue, 4U, &readBuf);

		if (!memcmp(readBuf, patchBytes[1], 4U)) {
			patchBInstalled++;
		}
	}

	*outNumAOffsets = patternAFound;
	*outNumBOffsets = patternBFound;

	if (patchAInstalled == patternAFound && patchBInstalled == patternBFound) { return 0; }

	if (((patternAFound && !patternBFound) || (!patternAFound && patternBFound)) ||
		((patchAInstalled && !patchBInstalled) || (!patchAInstalled && patchBInstalled))) { return 1; }
	
	return 2;
}

int write_bytes(FILE* const pFile, const unsigned long address, unsigned long* const pBytes, const size_t bytesSize) {

	fseek(pFile, address, 0);

	if (fwrite(pBytes, 1, bytesSize, pFile) != bytesSize) { return 0; }

	return 1;
}
