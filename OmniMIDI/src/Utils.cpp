/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.

*/

#include "Utils.hpp"

OMShared::Lib::Lib(const char* pName, const char* pSuffix, ErrorSystem::Logger* PErr, LibImport** pFuncs, size_t pFuncsCount) {
	ErrLog = PErr;
	Name = pName;
	Suffix = pSuffix != nullptr ? pSuffix : "";

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
	int32_t printStatus = 0;

	if (Utils.GetFolderPath(fid, buf, MAX_PATH_LONG)) {
		switch (fid) {
			case OMShared::FIDs::UserFolder:
				printStatus = snprintf(outPath, MAX_PATH_LONG, "%s/OmniMIDI/SupportLibraries/" LIBEXP(TYPE) "%s", buf, Name, Suffix);
				break;

			case OMShared::FIDs::PluginFolder:
				printStatus = snprintf(outPath, MAX_PATH_LONG, "%s/OmniMIDI/Plugins/" LIBEXP(TYPE) "%s", buf, Name, Suffix);
				break;

			default:
				printStatus = snprintf(outPath, MAX_PATH_LONG, "%s/" LIBEXP(TYPE) "%s", buf, Name, Suffix);
				break;
		}

		if (printStatus == -1)
			return false;
		
		Message(outPath);
		return Utils.DoesFileExist(outPath);
	}

	return false;
}

bool OMShared::Lib::GetLibPath(char* outPath) {
	bool skip = false;

	if (outPath == nullptr)
		return false;

	int32_t st = snprintf(outPath, MAX_PATH_LONG, LIBEXP(TYPENO) "%s", Name, Suffix);
	if (st == -1)
		skip = true;

	if (!skip) {
		auto _ = loadLib(outPath);
		if (_ != nullptr) {
			freeLib(_);
		}
	}

	if (Funcs) {
		if (IteratePath(outPath, OMShared::FIDs::CurrentDirectory))
			return true;

		if (IteratePath(outPath, OMShared::FIDs::LibGeneric))
			return true;

#ifndef _WIN32
		if (IteratePath(outPath, OMShared::FIDs::LibLocal))
			return true;

		if (IteratePath(outPath, OMShared::FIDs::Libi386))
			return true;

		if (IteratePath(outPath, OMShared::FIDs::LibAMD64))
			return true;

		if (IteratePath(outPath, OMShared::FIDs::LibAArch64))
			return true;
#endif
	}
	else {
		if (IteratePath(outPath, OMShared::FIDs::CurrentDirectory))
			return true;

		if (IteratePath(outPath, OMShared::FIDs::PluginFolder))
			return true;
		
		if (IteratePath(outPath, OMShared::FIDs::UserFolder))
			return true;
	}

	return false;
}

bool OMShared::Lib::LoadLib(char* CustomPath) {
	char* libPath = nullptr;
	char* finalPath= nullptr;

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
					Error("The required library \"%s\" could not be loaded. This is required for the synthesizer to work.", true, Name);
			}
			else Error("The required library \"%s\" could not be found. This is required for the synthesizer to work.", true, Name);
		}
	}

	finalPath = (libPath == nullptr) ? (char*)Name : libPath;

	if (Library != nullptr) {
		Message("%s --> 0x%08x (FROM %s)", Name, Library, finalPath);

		if (Funcs != nullptr && FuncsCount != 0) {
			for (size_t i = 0; i < FuncsCount; i++)
			{
				if (Funcs[i].SetPtr(Library, Funcs[i].GetName()))
					Message("%s --> 0x%08x", Funcs[i].GetName(), Funcs[i].GetPtr());
			}	
		}

		Message("%s ready!", Name);
	
		Initialized = true;
	}

	if (libPath != nullptr)
		delete[] libPath;

	return Initialized;
}

bool OMShared::Lib::UnloadLib() {
	if (Funcs != nullptr && FuncsCount != 0) {
		for (size_t i = 0; i < FuncsCount; i++)
			Funcs[i].SetPtr();
	}

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
				Error("A fatal error has occurred while unloading %s!!!", Name);
			}
			else Message("%s unloaded.", Name);
		}

		Library = nullptr;
	}

	LoadFailed = false;
	Initialized = false;
	return true;
}

bool OMShared::Lib::IsSupported(uint32_t loaded, uint32_t minimum) {
	auto libVer = LibVersion(loaded);
	auto libReqVer = LibVersion(minimum);

	if (libVer.GetHiWord() != libReqVer.GetHiWord()) {
		Error("OmniMIDI only supports %s %d.%d, but we got %d.%d.", true, Name, 
			libVer.Build, libVer.Major, libReqVer.Build, libReqVer.Major);
		return false;
	}
	if (libVer.GetLoWord() < libReqVer.GetLoWord()) {
		Error("OmniMIDI requires %s %d.%d.%d.%d minimum, but we got %2$d.%3$d.%d.%d.", true, Name, 
			libReqVer.Build, libReqVer.Major, libReqVer.Minor, libReqVer.Rev, libVer.Minor, libVer.Rev);
		return false;
	}

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

void OMShared::Funcs::MicroSleep(signed long long v) {
	if (v == 0)
		return;

#ifdef _WIN32
	pNtDelayExecution(0, &v);
#else
	std::this_thread::sleep_for(std::chrono::nanoseconds(v * 100));
#endif
}

uint32_t OMShared::Funcs::QuerySystemTime(signed long long* v) {
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
	int32_t csidl = 0;

	switch (FolderID) {
	case CurrentDirectory:
		return GetModuleFileNameA( NULL, path, MAX_PATH ) > 0;
		break;
	
	case LibGeneric:
		csidl = CSIDL_SYSTEM;
		break;

	case UserFolder:
	case PluginFolder:
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
	case PluginFolder:
		if (std::getenv("XDG_CONFIG_HOME") != nullptr) {
			envPath = std::getenv("XDG_CONFIG_HOME");
		}
		else {
			// Default to ~/.config
			static std::string configPath = std::format("{0}/.config", std::getenv("HOME"));
			envPath = configPath.c_str();
		}

		break;
	case CurrentDirectory:
		return getcwd(path, szPath) != 0;
	default:
		break;
	}

	if (envPath != nullptr) {
		strncpy(path, envPath, strlen(envPath) + 1);
		return true;
	}
#endif

	return false;
}

#ifdef _WIN32
wchar_t* OMShared::Funcs::GetUTF16(char* utf8) {
	wchar_t* buf = nullptr;

	int32_t cc = 0;
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