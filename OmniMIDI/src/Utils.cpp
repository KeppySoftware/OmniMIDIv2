/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.

*/

#include "Utils.hpp"

OMShared::Lib::Lib(const char* pName, const char* pSuffix, ErrorSystem::Logger* PErr, LibImport** pFuncs, size_t pFuncsCount) {
	Name = pName;
	Suffix = pSuffix != nullptr ? pSuffix : "";
	ErrLog = PErr;

	if (pFuncs != nullptr)
		Funcs = *pFuncs;
		
	FuncsCount = pFuncsCount;
}

OMShared::Lib::~Lib() {
	UnloadLib();
}

bool OMShared::Lib::IteratePath(char* outPath, OMShared::FIDs fid) {
	OMShared::Funcs Utils;
	char buf[MAX_PATH_LONG] = { 0 };
	int printStatus = 0;

	if (Utils.GetFolderPath(fid, buf, MAX_PATH_LONG)) {
		switch (fid) {
			case OMShared::FIDs::UserFolder:
				printStatus = snprintf(outPath, MAX_PATH_LONG, "%s/OmniMIDI/SupportLibraries/" LIBEXP(TYPE) "%s", buf, Name, Suffix);
				break;
			
			default:
				printStatus = snprintf(outPath, MAX_PATH_LONG, "%s/" LIBEXP(TYPE) "%s", buf, Name, Suffix);
				break;
		}

		if (printStatus == -1)
			return false;
		
		LOG(outPath);
		return Utils.DoesFileExist(outPath);
	}

	return false;
}

bool OMShared::Lib::GetLibPath(char* outPath) {
	bool skip = false;
	bool gotLib = false;

	if (outPath == nullptr)
		return false;

	int st = snprintf(outPath, MAX_PATH_LONG, LIBEXP(TYPENO) "%s", Name, Suffix);
	if (st == -1)
		skip = true;

	if (!skip) {
		auto _ = loadLib(outPath);
		if (_ != nullptr) {
			freeLib(_);
			gotLib = true;
		}
	}

	if (IteratePath(outPath, OMShared::FIDs::CurrentDirectory))
		return true;

	if (IteratePath(outPath, OMShared::FIDs::UserFolder))
		return true;

	if (IteratePath(outPath, OMShared::FIDs::LibGeneric))
		return true;

	if (IteratePath(outPath, OMShared::FIDs::Libi386))
		return true;

	if (IteratePath(outPath, OMShared::FIDs::LibAMD64))
		return true;

	if (IteratePath(outPath, OMShared::FIDs::LibAArch64))
		return true;

	return false;
}

bool OMShared::Lib::LoadLib(char* CustomPath) {
	char* libPath = nullptr;
	char* finalPath= nullptr;

	if (Funcs == nullptr || FuncsCount == 0)
		return true;

	if (Library == nullptr) {
		OMShared::Funcs Utils;
		
		Initialized = false;

		if (CustomPath != nullptr) {
			libPath = new char[MAX_PATH_LONG] { 0 };
			snprintf(libPath, MAX_PATH_LONG, "%s/%s", CustomPath, Name);
		}

		Library = loadLib(libPath == nullptr ? Name : libPath);
		if (Library == nullptr) {
			libPath = new char[MAX_PATH_LONG] { 0 };

			if (GetLibPath(libPath)) {
				Library = loadLib(libPath);
				if (Library == nullptr)
					NERROR("The required library \"%s\" could not be loaded. This is required for the synthesizer to work.", true, Name);
			}
			else NERROR("The required library \"%s\" could not be found. This is required for the synthesizer to work.", true, Name);
		}
	}

	finalPath = (libPath == nullptr) ? (char*)Name : libPath;

	if (Library != nullptr) {
		LOG("%s --> 0x%08x (FROM %s)", Name, Library, finalPath);

		for (size_t i = 0; i < FuncsCount; i++)
		{
			if (Funcs[i].SetPtr(Library, Funcs[i].GetName()))
				LOG("%s --> 0x%08x", Funcs[i].GetName(), Funcs[i].GetPtr());
		}
	
		LOG("%s ready!", Name);
	
		Initialized = true;
	}

	if (libPath != nullptr)
		delete[] libPath;

	return Initialized;
}

bool OMShared::Lib::UnloadLib() {
	if (Funcs == nullptr || FuncsCount == 0)
		return true;

	if (Library != nullptr) {
		if (AppSelfHosted)
		{
			AppSelfHosted = false;
		}
		else {
			bool r = freeLib(Library);

#ifndef _WIN32
			// flip the boolean for non Win32 OSes
			r = !r;
#endif

			assert(r == true);
			if (!r) {
				NERROR("A fatal error has occurred while unloading %s!!!", Name);
			}
			else LOG("%s unloaded.", Name);
		}

		Library = nullptr;
	}

	LoadFailed = false;
	Initialized = false;
	return true;
}

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
	if (v < 0) v *= -1;
	return usleep((useconds_t)(v / 10));
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
	if (path == nullptr)
		return false;
		
#if defined(_WIN32)
	int csidl = 0;

	switch (FolderID) {
	case CurrentDirectory:
	case LibGeneric:
	case Libi386:
	case LibAMD64:
	case LibAArch64:
		csidl = CSIDL_SYSTEM;
		break;
	case UserFolder:
		csidl = CSIDL_PROFILE;
		break;
	default:
		break;
	}

	return !(bool)SHGetFolderPathA(nullptr, csidl, NULL, SHGFP_TYPE_CURRENT, path);
#else
	const char* envPath = nullptr;

	strncpy(path, "\0", szPath);

	switch (FolderID) {
	case LibGeneric:
		envPath = "/usr/lib";
		break;
	case Libi386:
		envPath = "/usr/lib32";
		break;
	case LibAMD64:
		envPath = "/usr/lib64";
		break;
	case LibAArch64:
		envPath = "/usr/lib/aarch64-linux-gnu";
		break;
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
	wchar_t* buf = nullptr;

	int cc = 0;
	int count = strlen(utf8);
	// get length (cc) of the new widechar excluding the \0 terminator first
	if ((cc = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0)) > 0)
	{
		// convert
		buf = new wchar_t[cc];
		if (buf) {
			if (!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buf, cc)) {
				delete[] buf;
				buf = nullptr;
			}
		}
	}

	return buf;
}
#endif

bool OMShared::Funcs::DoesFileExist(std::string filePath) {
	bool exists = false;
#ifdef _WIN32
	// I SURE LOVE UTF-16!!!!!!!!!!
	const char* fPath = filePath.c_str();
	wchar_t* fwPath = GetUTF16((char*)fPath);

	if (fwPath != nullptr) {
		if (GetFileAttributesW(fwPath) != INVALID_FILE_ATTRIBUTES)
			exists = true;
	}
	else std::cout << "L!" << std::endl;

	if (fwPath != nullptr)
		delete[] fwPath;

	return exists;
#else
	std::ifstream fileCheck(filePath);

	exists = fileCheck.good();
	if (exists)
		fileCheck.close();

	return exists;
#endif
}