/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.

*/

#include "Utils.hpp"

bool OMShared::SysPath::GetFolderPath(const FIDs FolderID, wchar_t* P, size_t PS) {
#ifdef _WIN32 
#ifdef WINXPMODE
	int csidl = 0;

	switch (FolderID) {
	case System:
		csidl = CSIDL_SYSTEM;
		break;
	case UserFolder:
		csidl = CSIDL_PROFILE;
		break;
	default:
		break;
	}

	WCHAR Dir[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathW(NULL, csidl, NULL, 0, Dir))) {
		memcpy(P, Dir, PS);
		return true;
	}
#else
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
		bool Success = SUCCEEDED(SGKFP);

		if (Success)
			StringCchPrintfW(P, PS, L"%ws", Dir);

		CoTaskMemFree((LPVOID)Dir);

		return true;
	}
#endif
#else
	// Code for Unix
#endif

	return false;
}