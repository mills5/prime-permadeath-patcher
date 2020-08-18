#ifndef WINGUI_H
#define WINGUI_H

typedef const char* LPCSTR;

namespace WinGUI {

	bool IsActive();

	void MsgboxInfo(LPCSTR Title, LPCSTR Body, const bool err);

	bool MsgboxYesNo(LPCSTR Title, LPCSTR Body);

	bool OpenFileSelectDialog(char*& outpFilename);

}

#endif WINGUI_H