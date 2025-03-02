/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.

*/

#include "Utils.hpp"

OMShared::Funcs::Funcs() {
#ifdef _WIN32
	// There is no equivalent to this in Linux/macOS
	ntdll = getLib("ntdll");

	if (!ntdll) {
		LL = true;
		ntdll = loadLib("ntdll");
	}

	assert(ntdll != 0);
	if (!ntdll)
		return;

	auto v1 = (unsigned int (WINAPI*)(unsigned char, signed long long*))getAddr(ntdll, "NtDelayExecution");
	assert(v1 != 0);

	if (v1 == nullptr)
		return;

	pNtDelayExecution = v1;

	auto v2 = (unsigned int (WINAPI*)(signed long long*))getAddr(ntdll, "NtQuerySystemTime");
	assert(v2 != 0);

	if (v2 == nullptr)
		return;

	pNtQuerySystemTime = v2;
#endif
}

OMShared::Funcs::~Funcs() {
#ifdef _WIN32
	if (LL) {
		if (!freeLib(ntdll))
			exit(GetLastError());

		ntdll = nullptr;
	}
#endif
}

unsigned int OMShared::Funcs::MicroSleep(signed long long v) {
	if (!v)
		return 0;

#ifdef _WIN32
	return pNtDelayExecution(0, &v);
#else
	return usleep((useconds_t)((v / 10) * -1));
#endif
}

unsigned int OMShared::Funcs::QuerySystemTime(signed long long* v) {
#ifdef _WIN32
	return pNtQuerySystemTime(v);
#else
	struct timeval tnow;
	gettimeofday(&tnow, 0);
	*v = tnow.tv_sec * (unsigned long long)10000000 + (((369 * 365 + 89) * (unsigned long long)86400) * 10000000);
	*v += tnow.tv_usec * 10;
	return 0;
#endif
}

bool OMShared::Funcs::GetFolderPath(const FIDs FolderID, char* path, size_t szPath) {
#ifdef _WIN32 
	GUID id = GUID_NULL;

	switch (FolderID) {
	case CurrentDirectory:
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
			wcstombs(path, Dir, szPath);

		CoTaskMemFree((LPVOID)Dir);

		return succ;
	}
#else
	const char* envPath = nullptr;

	switch (FolderID) {
	case UserFolder:
		envPath = std::getenv("HOME");
		break;
	case CurrentDirectory:
		return getcwd(path, szPath) != 0;
	default:
		break;
	}

	if (envPath != nullptr) {
		strncpy(path, envPath, strlen(envPath));
		return true;
	}
#endif

	return false;
}

#ifdef _WIN32
wchar_t* OMShared::Funcs::GetUTF16(char* utf8) {
	int cc = 0;
	int count = strlen(utf8);
	// get length (cc) of the new widechar excluding the \0 terminator first
	if ((cc = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0)) > 0)
	{
		// convert
		wchar_t* buf = new wchar_t[cc];
		if (buf) {
			if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buf, cc)) {
				return buf;
			}
		}
	}

	return nullptr;
}
#endif

bool OMShared::Funcs::DoesFileExist(std::string filePath) {
	bool doesIt = false;
	char* fPath = filePath.data();

#ifdef _WIN32
	// I SURE LOVE UTF-16!!!!!!!!!!
	wchar_t* buf = GetUTF16(fPath);
	if (buf) {
		if (GetFileAttributesW(buf) != INVALID_FILE_ATTRIBUTES)
			doesIt = true;

		delete[] buf;
	}

	return doesIt;
#else
	std::ifstream fileCheck(filePath);

	doesIt = fileCheck.good();

	if (doesIt)
		fileCheck.close();

	return doesIt;
#endif
}