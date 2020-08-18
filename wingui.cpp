#include "wingui.h"

#ifdef _WIN32
#include <Windows.h>

bool WinGUI::IsActive() {

	DWORD buf[2];

	return GetConsoleProcessList((LPDWORD)&buf, 2) == 1;
}

void WinGUI::MsgboxInfo(LPCSTR Title, LPCSTR Body, const bool err) {
	MessageBoxA(NULL, Body, Title, err ? MB_ICONERROR : MB_ICONINFORMATION);
}

bool WinGUI::MsgboxYesNo(LPCSTR Title, LPCSTR Body) {
	return MessageBoxA(NULL, Body, Title, MB_ICONQUESTION | MB_YESNO) == IDYES;
}

bool WinGUI::OpenFileSelectDialog(char*& outpFilename) {

	static char win32Filename[1024];

	OPENFILENAMEA win32OpenFilename;

	memset(&win32Filename, 0, sizeof(win32Filename));
	memset(&win32OpenFilename, 0, sizeof(OPENFILENAMEA));
	win32OpenFilename.lStructSize = sizeof(OPENFILENAMEA);
	win32OpenFilename.hwndOwner = 0;
	win32OpenFilename.hInstance = 0;
	win32OpenFilename.lpstrFilter = "ISO Files (*.iso)\0*.iso\0\0";
	win32OpenFilename.lpstrCustomFilter = 0;
	win32OpenFilename.nMaxCustFilter = 0;
	win32OpenFilename.nFilterIndex = 0;
	win32OpenFilename.lpstrFile = win32Filename;
	win32OpenFilename.nMaxFile = sizeof(win32Filename);
	win32OpenFilename.lpstrTitle = "Select Metroid Prime ISO";
	win32OpenFilename.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	win32OpenFilename.lpstrDefExt = "iso";

	if (!GetOpenFileNameA(&win32OpenFilename)) {
		MessageBoxA(GetActiveWindow(), "Error opening file, or none selected", "Error", MB_ICONERROR);
		return false;
	}

	outpFilename = win32Filename;

	return true;
}

#else

bool WinGUI::IsActive() { return false; }

void WinGUI::MsgboxInfo(LPCSTR Title, LPCSTR Body, const bool err) { return; }

bool WinGUI::MsgboxYesNo(LPCSTR Title, LPCSTR Body) { return false; }

bool WinGUI::OpenFileSelectDialog(const char*& outpFilename) { return false; }

#endif