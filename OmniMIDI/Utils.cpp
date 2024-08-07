/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.

*/

#include "Utils.hpp"

bool OMShared::SysPath::GetFolderPath(const FIDs FolderID, wchar_t* path, size_t szPath) {
#ifdef _WIN32 
	GUID id = GUID_NULL;

	switch (FolderID) {
	case System:
		id = FOLDERID_System;
		break;
	case UserFolder:
		id = FOLDERID_Profile;
		break;
	default:
		break;
	}

	if (id != GUID_NULL) {
		PWSTR Dir;
		HRESULT SGKFP = SHGetKnownFolderPath(id, 0, NULL, &Dir);
		bool succ = SUCCEEDED(SGKFP);

		if (succ)
			StringCchPrintfW(path, szPath, L"%ws", Dir);

		CoTaskMemFree((LPVOID)Dir);

		return succ;
	}
#else
	// Code for Unix
#endif

	return false;
}