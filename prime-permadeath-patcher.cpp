#include <algorithm>
#include <functional>
#include <vector>
#include <stdio.h>
#include <string.h>
#include "wingui.h"

#define BUF_SIZE 2048

struct SPrimePatchStatus;
bool FindBinaryPattern(unsigned char* pData, const size_t szData, unsigned char* pNeedle, const size_t szNeedle, unsigned long& outValue);
int IsValidGameVersion(int& outVersion);
bool PatchFile(const SPrimePatchStatus& patchStatus);
void PrintHelp();
bool ReadBytes(const unsigned long address, const size_t readSize, void* const outBuf);
void ScanImage(SPrimePatchStatus& outPatchStatus);
bool WriteBytes(const unsigned long address, const unsigned char* const pBytes, const size_t szBytes);

enum
{
	PARAM_INSTALL_PERMADEATH = 1 << 1,
	PARAM_INSTALL_NOSAVE = 1 << 2,
	PARAM_CHECK_ONLY = 1 << 3,
	PARAM_REMOVE_PERMADEATH = 1 << 4,
	PARAM_REMOVE_NOSAVE = 1 << 5
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

struct SPrimePattern
{
	unsigned long offset;
	bool patched;
};

struct SPrimePatchStatus 
{
	std::vector<SPrimePattern> vecAPatterns;
	std::vector<SPrimePattern> vecBPatterns;
};

static constexpr const unsigned long gFilePatternsSizes[2] = { 0x14, 0xC };

static constexpr const unsigned char gPatchBytes[2][4] = {
	{ 0x60, 0x00, 0x00, 0x00 },
	{ 0x48, 0x00, 0x03, 0x6C }
};

static constexpr const unsigned char gUnpatchBytes[2][4] = {
	{ 0x40, 0x82, 0x00, 0x7C },
	{ 0x41, 0x81, 0x03, 0x78 }
};

static unsigned char gParams;

static bool gPatchAInstalled, gPatchBInstalled;

static FILE* gpFile;

static char* gpFileStr;

static bool gUseGUI;

int main(int argc, char* argv[]) {

	gParams = 0;
	gPatchAInstalled = true;
	gPatchBInstalled = true;
	gUseGUI = WinGUI::IsActive();

	if (!gUseGUI) {

		switch (argc) {
		case 2:
			gParams |= PARAM_CHECK_ONLY;
			break;
		case 4:
		{
			for (int i = 2; i < 4; i++) {
				if (!strcmp(argv[i], "1")) {
					if (i == 2) {
						gParams |= PARAM_INSTALL_PERMADEATH;
					}
					else if (i == 3) {
						gParams |= PARAM_INSTALL_NOSAVE;
					}
				}
				else if (!strcmp(argv[i], "0")) {
					if (i == 2) {
						gParams |= PARAM_REMOVE_PERMADEATH;
					}
					else if (i == 3) {
						gParams |= PARAM_REMOVE_NOSAVE;
					}
				}
			}

			if ((!(gParams & PARAM_INSTALL_PERMADEATH)) && (!(gParams & PARAM_INSTALL_NOSAVE))
				&& (!(gParams & PARAM_REMOVE_PERMADEATH)) && (!(gParams & PARAM_REMOVE_NOSAVE))) {
				PrintHelp();
				return 0;
			}
			
			break;
		}
		default:
			PrintHelp();
			return 0;
		}

		gpFileStr = argv[1];

	}
	else {

		char* pWin32Filename;

		if (WinGUI::OpenFileSelectDialog(pWin32Filename)) {
			gpFileStr = pWin32Filename;
		}
		else {
			return -1;
		}

	}

	gpFile = fopen(gpFileStr, "r+b");

	if (!gpFile) {
		printf("Error opening file \"%s\" for patching\n", gpFileStr);
		return -1;
	}

	printf("Opened \"%s\"\n", gpFileStr);

	static constexpr const char* versionStrs[VERSION_COUNT] = { "USA v1.0", "USA v1.1", "USA v1.2", "Europe", "Japan", "Korea" };

	int ver;
	
	switch (IsValidGameVersion(ver)) {
	case -1:
		return -1;

	case 0:
		printf("Error: Invalid ISO or Game Version\n");
		if (gUseGUI) { WinGUI::MsgboxInfo("Patcher", "Invalid ISO or Game Version", true); }
		return -1;

	case 1:
		printf("Version Detected: %s\n", versionStrs[ver]);
		break;
	}

	switch (ver) {
	case VERSION_USA_000:
	case VERSION_USA_001:
		break;
	default:
		if (gUseGUI) { WinGUI::MsgboxInfo("Error", "Error: Unsupported game version", true); }
		printf("Error: Unsupported game version. Current supported versions are %s and %s\n", versionStrs[VERSION_USA_000], versionStrs[VERSION_USA_001]);
		return -1;
	}

	printf("Scanning image, please wait...");

	SPrimePatchStatus patchStatus;

	ScanImage(patchStatus);

	if (!patchStatus.vecAPatterns.size() || !patchStatus.vecBPatterns.size()) {
		if (gUseGUI) { WinGUI::MsgboxInfo("Error", "Error: Invalid image (no patterns found)", true); }
		printf("\nError: Invalid image (no patterns found)\n");
		return -1;
	}
	else if (patchStatus.vecAPatterns.size() > 2 || patchStatus.vecBPatterns.size() > 2) {
		if (gUseGUI) { WinGUI::MsgboxInfo("Error", "Error: Invalid image (too many patterns found)", true); }
		printf("\nError: Invalid image (too many patterns found)\n");
		return -1;
	}

	printf(" DONE\n");

	for (ver = 0; ver < patchStatus.vecAPatterns.size(); ver++) {

		if (!patchStatus.vecAPatterns.at(ver).patched) {
			gPatchAInstalled = false;
			break;
		}

	}
	for (ver = 0; ver < patchStatus.vecBPatterns.size(); ver++) {

		if (!patchStatus.vecBPatterns.at(ver).patched) {
			gPatchBInstalled = false;
			break;
		}

	}

	printf("ISO STATUS:\n");
	printf("Permadeath	=	%s\n", gPatchAInstalled ? "Enabled  (Modded)" : "Disabled (Vanilla)");
	printf("Saving		=	%s\n", gPatchBInstalled ? "Disabled (Modded)" : "Enabled  (Vanilla)");

	if (gUseGUI) {

		static constexpr const char* guiStrs[6] = { "Permadeath", "Saving", "Enable", "Disable", "Enabled", "Disabled" };

		char buf[128];

		sprintf(buf, "%s is currently %s.\n%s %s?", guiStrs[0], gPatchAInstalled ? guiStrs[4] : guiStrs[5], gPatchAInstalled ? guiStrs[3] : guiStrs[2], guiStrs[0]);

		if (WinGUI::MsgboxYesNo("Patcher", buf)) {

			if (gPatchAInstalled) {
				gParams |= PARAM_REMOVE_PERMADEATH; 
			}
			else {
				gParams |= PARAM_INSTALL_PERMADEATH;
			}

		}

		sprintf(buf, "%s is currently %s.\n%s %s?", guiStrs[1], gPatchBInstalled ? guiStrs[5] : guiStrs[4], gPatchBInstalled ? guiStrs[2] : guiStrs[3], guiStrs[1]);

		if (WinGUI::MsgboxYesNo("Patcher", buf)) {

			if (gPatchBInstalled) {
				gParams |= PARAM_REMOVE_NOSAVE;
			}
			else {
				gParams |= PARAM_INSTALL_NOSAVE;
			}

		}

	}

	if (!(gParams & PARAM_CHECK_ONLY)) {

		if (!PatchFile(patchStatus)) {
			printf("Patching failed, terminating\n");
			if (gUseGUI) { WinGUI::MsgboxInfo("Patcher", "An error occurred while patching", true); }
			return -1;
		}

	}

	if ((gParams & PARAM_INSTALL_PERMADEATH) || (gParams & PARAM_REMOVE_PERMADEATH)
		|| (gParams & PARAM_INSTALL_NOSAVE) || (gParams & PARAM_REMOVE_NOSAVE)) {

		printf("Operation Complete\n");
		if (gUseGUI) { WinGUI::MsgboxInfo("Patcher", "Operation Completed!", false); }
	}
	else if (gUseGUI) { WinGUI::MsgboxInfo("Patcher", "No operations were selected.", false); }

	return 1;
}

bool FindBinaryPattern(unsigned char* pData, const size_t szData, unsigned char* pNeedle, const size_t szNeedle, unsigned long& outValue) {

	auto result = std::search(pData, pData + szData, std::boyer_moore_searcher(pNeedle, pNeedle + szNeedle));

	if (result != (pData + szData)) {
		outValue = result - pData;
		return true;
	}

	return false;
}

int IsValidGameVersion(int& outVersion) {

	static constexpr const unsigned char fileGamecodeBytes[3][6] = {

		{ 0x47, 0x4D, 0x38, 0x45, 0x30, 0x31 },
		{ 0x47, 0x4D, 0x38, 0x50, 0x30, 0x31 },
		{ 0x47, 0x4D, 0x38, 0x4A, 0x30, 0x31 }

	};

	static constexpr const unsigned char fileVersionByte[6] = { 0x01, 0x02, 0x30 };

	unsigned char versionBuf;
	unsigned char gamecodeBuf[6];

	if (!(ReadBytes(0x07, sizeof(unsigned char), &versionBuf)) || !(ReadBytes(0x00, sizeof(gamecodeBuf), &gamecodeBuf))) {
		printf("Error reading file: ferror %d\n", ferror(gpFile));
		return -1;
	}

	outVersion = VERSION_INVALID;

	if (!memcmp(&versionBuf, &fileVersionByte[0], sizeof(unsigned char))) {
		outVersion = VERSION_USA_001;
		return 1;
	}

	if (!memcmp(&versionBuf, &fileVersionByte[1], sizeof(unsigned char))) {
		outVersion = VERSION_USA_002;
		return 1;
	}

	if (!memcmp(&gamecodeBuf, &fileVersionByte[2], sizeof(gamecodeBuf))) {
		outVersion = VERSION_KOR;
		return 1;
	}

	if (!memcmp(&gamecodeBuf, &fileGamecodeBytes[0], sizeof(gamecodeBuf))) {
		outVersion = VERSION_USA_000;
		return 1;
	}

	if (!memcmp(&gamecodeBuf, &fileGamecodeBytes[1], sizeof(gamecodeBuf))) {
		outVersion = VERSION_EUR;
		return 1;
	}

	if (!memcmp(&gamecodeBuf, &fileGamecodeBytes[2], sizeof(gamecodeBuf))) {
		outVersion = VERSION_JAP;
		return 1;
	}

	return 0;
}

bool PatchFile(const SPrimePatchStatus& patchStatus) {

	const bool installA = gParams & PARAM_INSTALL_PERMADEATH;
	const bool installB = gParams & PARAM_INSTALL_NOSAVE;
	const bool removeA = gParams & PARAM_REMOVE_PERMADEATH;
	const bool removeB = gParams & PARAM_REMOVE_NOSAVE;

	bool result = true;
	unsigned long writeAddr;

	if (installA || removeA) {

		if (!(!gPatchAInstalled && removeA)) {
			printf("%sing Permadeath patch...\n", installA ? "Install" : "Remov");
		}

		for (size_t i = 0; i < patchStatus.vecAPatterns.size(); i++) {

			writeAddr = patchStatus.vecAPatterns.at(i).offset + gFilePatternsSizes[0];

			if (installA && !gPatchAInstalled) {
				result = WriteBytes(writeAddr, (unsigned char*)gPatchBytes[0], 4U);
			}
			else if (removeA && gPatchAInstalled) {
				result = WriteBytes(writeAddr, (unsigned char*)gUnpatchBytes[0], 4U);
			}

			if (!result) {
				printf("fwrite to 0x%x in file failed\n", writeAddr);
				return false;
			}

		}

	}

	if (installB || removeB) {

		if (!(!gPatchBInstalled && removeB)) {
			printf("%sing No-Saving patch...\n", installB ? "Install" : "Remov");
		}

		for (size_t i = 0; i < patchStatus.vecBPatterns.size(); i++) {

			writeAddr = patchStatus.vecBPatterns.at(i).offset + gFilePatternsSizes[1];

			if (installB && !gPatchBInstalled) {
				result = WriteBytes(writeAddr, (unsigned char*)gPatchBytes[1], 4U);
			}
			else if (removeB && gPatchBInstalled) {
				result = WriteBytes(writeAddr, (unsigned char*)gUnpatchBytes[1], 4U);
			}

			if (!result) {
				printf("fwrite to 0x%x in file failed\n", writeAddr);
				return false;
			}

		}

	}

	return true;
}

void PrintHelp() {
	printf("Metroid Prime Permadeath Patcher v2.0 - github.com/mills5\nUsage: prime-permadeath-patcher <.iso file> <install/remove permadeath... 1|0> <install/remove no-saving... 1|0>\nExamples:\nInstall Permadeath Only:                   prime-permadeath-patcher metroid_prime.iso 1 0\nInstall Permadeath AND Disable Saves:      prime-permadeath-patcher metroid_prime.iso 1 1\nUninstall All Patches (Revert To Vanilla): prime-permadeath-patcher metroid_prime.iso 0 0\n");
}

bool ReadBytes(const unsigned long address, const size_t readSize, void* const outBuf) {

	if (!gpFile) { return false; }

	fseek(gpFile, address, 0);

	if (fread(outBuf, 1, readSize, gpFile) != readSize) { return false; }

	return true;
}

void ScanImage(SPrimePatchStatus& outPatchStatus) {

	static unsigned char filePatterns[2][20] = {

	{ 0x57, 0xE4, 0x06, 0x3E, 0x4B, 0xFC, 0x85, 0x59, 0xC0, 0x02, 0x85, 0x68, 0xFC, 0x1D, 0x00, 0x40, 0x4C, 0x40, 0x13, 0x82 },
	{ 0x80, 0x83, 0x00, 0x58, 0x28, 0x00, 0x00, 0x10, 0x80, 0xA4, 0x00, 0xC4 }

	};

	unsigned char readBuf[BUF_SIZE];
	size_t readBytes;
	unsigned int mult = 0;
	unsigned long outValue;

	fseek(gpFile, 0, 0);

	SPrimePattern temp = { 0 };

	while ((readBytes = fread(&readBuf, 1, sizeof(readBuf), gpFile)) > 0) {

		if (FindBinaryPattern(readBuf, BUF_SIZE, filePatterns[0], gFilePatternsSizes[0], outValue)) {
			temp.offset = outValue + (readBytes * mult);
			outPatchStatus.vecAPatterns.push_back(temp);
		}

		if (FindBinaryPattern(readBuf, BUF_SIZE, filePatterns[1], gFilePatternsSizes[1], outValue)) {
			temp.offset = outValue + (readBytes * mult);
			outPatchStatus.vecBPatterns.push_back(temp);
		}

		mult++;
	}

	for (mult = 0; mult < outPatchStatus.vecAPatterns.size(); mult++) {

		SPrimePattern& primepattern = outPatchStatus.vecAPatterns.at(mult);

		outValue = primepattern.offset + gFilePatternsSizes[0];

		ReadBytes(outValue, 4U, &readBuf);

		primepattern.patched = (!memcmp(readBuf, gPatchBytes[0], 4U));
	}

	for (mult = 0; mult < outPatchStatus.vecBPatterns.size(); mult++) {

		SPrimePattern& primepattern = outPatchStatus.vecBPatterns.at(mult);

		outValue = primepattern.offset + gFilePatternsSizes[1];

		ReadBytes(outValue, 4U, &readBuf);

		primepattern.patched = (!memcmp(readBuf, gPatchBytes[1], 4U));
	}

}

bool WriteBytes(const unsigned long address, const unsigned char* const pBytes, const size_t szBytes) {

	fseek(gpFile, address, 0);

	if (fwrite(pBytes, 1, szBytes, gpFile) != szBytes) { return false; }

	return true;
}