/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.

*/

#include "Utils.hpp"

bool OMShared::LibImport::SetPtr(void* lib, const char* ptrname) {
	void* ptr = nullptr;

	if (lib == nullptr && ptrname == nullptr)
	{
		if (funcptr)
			*(funcptr) = nullptr;

		return true;
	}

	if (lib == nullptr)
		return false;

	ptr = (void*)LibFuncs::GetFuncAddress(lib, ptrname);

	if (!ptr) {
		return false;
	}

	if (ptr != *(funcptr))
		*(funcptr) = ptr;

	return true;
}

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
		auto _ = LibFuncs::Load(outPath);
		if (_ != nullptr) {
			LibFuncs::Free(_);
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

		Library = LibFuncs::Load(libPath == nullptr ? Name : libPath);
		if (Library == nullptr) {
			libPath = new char[MAX_PATH_LONG] { 0 };

			if (GetLibPath(libPath)) {
				Library = LibFuncs::Load(libPath);
			}
		}
	}

	if (Library != nullptr) {		
		finalPath = (libPath == nullptr) ? (char*)Name : libPath;
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
	else Error("The required library \"%s\" could not be found. This is required for the synthesizer to work.", true, Name);

	if (libPath != nullptr)
		delete[] libPath;

	return Initialized;
}

bool OMShared::Lib::UnloadLib() {
	if (Funcs != nullptr && FuncsCount > 0) {
		for (size_t i = 0; i < FuncsCount; i++)
			Funcs[i].SetPtr();
	}

	if (Library != nullptr) {
		if (!AppSelfHosted)
		{
			bool success = LibFuncs::Free(Library);

			assert(success == true);
			if (!success) {
				Error("A fatal error has occurred while unloading %s!!!", Name);
			}
			else Message("%s unloaded.", Name);
		}
		else AppSelfHosted = false;
	}

	Library = nullptr;
	LoadFailed = false;
	Initialized = false;
	return true;
}

bool OMShared::Lib::IsSupported(uint32_t loaded, uint32_t minimum) {
	auto libVer = LibVersion(loaded);
	auto libReqVer = LibVersion(minimum);

	if (libVer.GetHiWord() != libReqVer.GetHiWord()) {
		Error("OmniMIDI uses %s %d.%d bindings that are not compatible with the older %d.%d. Please update the library and try again.", true, Name, 
			libReqVer.Build, libReqVer.Major, libVer.Build, libVer.Major);
		return false;
	}
	if (libVer.GetLoWord() < libReqVer.GetLoWord()) {
		Error("OmniMIDI requires %s %d.%d.%d.%d minimum, but the loaded library reported %d.%d.%d.%d. Please update the library and try again.", true, Name, 
			libReqVer.Build, libReqVer.Major, libReqVer.Minor, libReqVer.Rev, 
			libVer.Build, libVer.Major, libVer.Minor, libVer.Rev);
		return false;
	}

	return true;
}

void* OMShared::LibFuncs::Load(const char* path) {
	if (path == nullptr)
		return nullptr;

#ifdef _WIN32
	return LoadLibraryA(path);
#else
	return dlopen(path, RTLD_NOW);
#endif
}

bool OMShared::LibFuncs::Free(void* ptr) {
	if (ptr == nullptr)
		return false;

#ifdef _WIN32
	return FreeLibrary((HMODULE)ptr);
#else
	return dlclose(ptr) == 0;
#endif
}

void* OMShared::LibFuncs::GetLibraryAddress(const char* libName) {
	if (libName == nullptr)
		return nullptr;

#ifdef _WIN32
	return GetModuleHandleA(libName);
#else
	return dlopen(libName, RTLD_NOLOAD | RTLD_GLOBAL);
#endif
}

void* OMShared::LibFuncs::GetFuncAddress(void* ptr, const char* funcName) {
	if (ptr == nullptr || funcName == nullptr)
		return nullptr;

#ifdef _WIN32
	return (void*)GetProcAddress((HMODULE)ptr, funcName);
#else
	return dlsym(ptr, funcName);
#endif
}

OMShared::Funcs::Funcs() {
#ifdef _WIN32
	// There is no equivalent to this in Linux/macOS
	ntdll = LibFuncs::GetLibraryAddress("ntdll");

	if (!ntdll) {
		LL = true;
		ntdll = LibFuncs::Load("ntdll");
	}

	assert(ntdll != 0);
	if (!ntdll)
		return;

	auto v1 = (uint32_t (WINAPI*)(uint8_t, int64_t*))LibFuncs::GetFuncAddress(ntdll, "NtDelayExecution");
	assert(v1 != 0);

	if (v1 == nullptr)
		return;

	pNtDelayExecution = v1;

	auto v2 = (uint32_t (WINAPI*)(int64_t*))LibFuncs::GetFuncAddress(ntdll, "NtQuerySystemTime");
	assert(v2 != 0);

	if (v2 == nullptr)
		return;

	pNtQuerySystemTime = v2;
#endif
}

OMShared::Funcs::~Funcs() {
#ifdef _WIN32
	if (LL) {
		if (!LibFuncs::Free(ntdll))
			exit(GetLastError());

		ntdll = nullptr;
	}
#endif
}

void OMShared::Funcs::MicroSleep(int64_t v) {
	if (v == 0)
		return;

#ifdef _WIN32
	pNtDelayExecution(0, &v);
#else
	std::this_thread::sleep_for(std::chrono::nanoseconds(v * 100));
#endif
}

uint32_t OMShared::Funcs::QuerySystemTime(int64_t* v) {
#ifdef _WIN32
	return pNtQuerySystemTime(v);
#else
	struct timeval tnow;
	gettimeofday(&tnow, 0);
	*v = tnow.tv_sec * (uint64_t)10000000 + (((369 * 365 + 89) * (uint64_t)86400) * 10000000);
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
		envPath = std::getenv("XDG_CONFIG_HOME");
		if (envPath == nullptr || envPath[0] == 0) {
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

bool OMShared::Funcs::CreateFolder(char* folderPath, size_t szFolderPath) {
	// try again
	fs::path fp = folderPath;

	if (fp.has_filename())
		fp = fp.remove_filename();

	if (!fs::is_directory(fp)) {
		if (!fs::create_directory(fp))
			return false;
	}

	return true;
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

bool OMShared::Funcs::OpenFile(std::string filePath) {
	if (!DoesFileExist(filePath))
		return false;
	
	char* cStr = (char*)filePath.c_str();

#ifdef _WIN32
	wchar_t* wStr = GetUTF16(cStr);
	auto ret = ShellExecuteW(NULL, L"open", wStr, NULL, NULL, SW_SHOW);

	if (ret > (HINSTANCE)32)
		return true;
#else
	char fStr[MAX_PATH_LONG] { 0 };
	sprintf(fStr, "xdg-open %s &", cStr);
	auto ret = system(fStr);

	if (!ret)
		return true;
#endif

	return false;
}