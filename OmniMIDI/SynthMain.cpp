/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.

*/

#include "SynthMain.hpp"

OmniMIDI::Lib::Lib(const char* pName, ErrorSystem::Logger* PErr, LibImport** pFuncs, size_t pFuncsCount) {
	Name = pName;
	Funcs = *pFuncs;
	FuncsCount = pFuncsCount;
	ErrLog = PErr;
}

OmniMIDI::Lib::~Lib() {
	UnloadLib();
}

bool OmniMIDI::Lib::LoadLib(char* CustomPath) {
	OMShared::Funcs Utils;

	char SysDir[MAX_PATH] = { 0 };
	char DLLPath[MAX_PATH] = { 0 };

	int swp = 0;

	if (Library == nullptr) {
#ifdef _WIN32
		if ((Library = getLib(Name)) != nullptr)
		{
			// (TODO) Make it so we can load our own version of it
			// For now, just make the driver try and use that instead
			LOG("%s already in memory.", Name);
			return (AppSelfHosted = true);
		}
		else {
#endif
			if (CustomPath != nullptr) {
#ifdef _WIN32
				swp = snprintf(DLLPath, sizeof(DLLPath), "%s/%s", CustomPath, Name);
#else
				swp = snprintf(DLLPath, sizeof(DLLPath), "%s/lib%s.so", CustomPath, Name);
#endif
				assert(swp != -1);

				if (swp != -1) {
					Library = loadLib(DLLPath);

					if (!Library)
						return false;
				}
				else return false;
			}
			else {
#ifdef _WIN32
				swp = snprintf(DLLPath, sizeof(DLLPath), "%s", Name);
#else
				swp = snprintf(DLLPath, sizeof(DLLPath), "lib%s", Name);
#endif
				assert(swp != -1);

				if (swp != -1) {
					Library = loadLib(Name);

					if (!Library)
					{
						if (Utils.GetFolderPath(OMShared::FIDs::CurrentDirectory, SysDir, sizeof(SysDir))) {
#ifdef _WIN32
							swp = snprintf(DLLPath, MAX_PATH, "%s/%s", SysDir, Name);
#else
							swp = snprintf(DLLPath, MAX_PATH, "%s/lib%s.so", SysDir, Name);
#endif
							assert(swp != -1);
							if (swp != -1) {
								Library = loadLib(DLLPath);
								assert(Library != 0);

								if (!Library) {
									NERROR("The required library \"%s\" could not be loaded or found. This is required for the synthesizer to work.", true, Name);
									return false;
								}
							}
							else return false;
						}
						else return false;
					}
				}
				else return false;

			}
#ifdef _WIN32
		}
#endif
	}

	LOG("%s --> 0x%08x", Name, Library);

	for (size_t i = 0; i < FuncsCount; i++)
	{
		if (Funcs[i].SetPtr(Library, Funcs[i].GetName()))
			LOG("%s --> 0x%08x", Funcs[i].GetName(), Funcs[i].GetPtr());
	}

	LOG("%s ready!", Name);

	Initialized = true;
	return true;
}

bool OmniMIDI::Lib::UnloadLib() {
	if (Library != nullptr) {
		if (AppSelfHosted)
		{
			AppSelfHosted = false;
		}
		else {
			bool r = freeLib(Library);
			assert(r == true);
			if (!r) {
				throw;
			}
			else LOG("%s unloaded.", Name);
		}

		Library = nullptr;
	}

	LoadFailed = false;
	Initialized = false;
	return true;
}

void OmniMIDI::SoundFontSystem::SoundFontThread() {
	while (StayAwake) {
		if (ListPath) {
			auto ftime = std::filesystem::last_write_time(ListPath);

			if (ListLastEdit != ftime) {
				ListLastEdit = ftime;

				if (SynthModule != nullptr)
					SynthModule->LoadSoundFonts();

				LOG("SoundFonts loaded.");
			}
		}

		Utils.MicroSleep(-1000000);
	}
}

bool OmniMIDI::SoundFontSystem::RegisterCallback(OmniMIDI::SynthModule* ptr) {
	if (ptr != nullptr) {
		SynthModule = ptr;
		StayAwake = true;

		_SFThread = std::jthread(&SoundFontSystem::SoundFontThread, this);
		if (!_SFThread.joinable()) {
			NERROR("_XSyThread failed. (ID: %x)", true, _SFThread.get_id());
			return false;
		}

		LOG("SoundFont autoloader running.");
		return true;
	}

	StayAwake = false;
	if (_SFThread.joinable())
		_SFThread.join();

	SynthModule = nullptr;
	LOG("Disposed of SoundFont autoloader.");
	return true;
}

std::vector<OmniMIDI::SoundFont>* OmniMIDI::SoundFontSystem::LoadList(std::string list) {
	char tmpUtils[MAX_PATH] = { 0 };
	char OMPath[MAX_PATH_LONG] = { 0 };
	const char* path = nullptr;
	bool succeed = false;

	if (SoundFonts.size() > 0)
		return &SoundFonts;

	if (Utils.GetFolderPath(OMShared::FIDs::UserFolder, tmpUtils, sizeof(tmpUtils))) {
		snprintf(OMPath, sizeof(OMPath), "%s/OmniMIDI/lists/OmniMIDI_A.json", tmpUtils);

		std::fstream sfs;
		sfs.open(!list.empty() ? list.c_str() : OMPath, std::fstream::in);

		if (sfs.is_open()) {
			try {
				// Read the JSON data from there
				auto json = nlohmann::json::parse(sfs, nullptr, false, true);

				if (json != nullptr) {
					auto& JsonData = json["SoundFonts"];

					if (!(JsonData == nullptr || JsonData.size() < 1)) {
						SoundFonts.clear();

						for (int i = 0; i < JsonData.size(); i++) {
							SoundFont SF;
							nlohmann::json subitem = JsonData[i];

							// Is item valid?
							if (subitem != nullptr) {
								SF.path = subitem["path"].is_null() ? "\0" : subitem["path"];
								SF.enabled = subitem["enabled"].is_null() ? SF.enabled : (bool)subitem["enabled"];

								if (SF.path.empty())
									continue;

								path = SF.path.c_str();

								if (Utils.DoesFileExist(SF.path))
								{
									SF.xgdrums = subitem["xgdrums"].is_null() ? SF.xgdrums : (bool)subitem["xgdrums"];
									SF.linattmod = subitem["linattmod"].is_null() ? SF.linattmod : (bool)subitem["linattmod"];
									SF.lindecvol = subitem["lindecvol"].is_null() ? SF.lindecvol : (bool)subitem["lindecvol"];
									SF.minfx = subitem["minfx"].is_null() ? SF.minfx : (bool)subitem["minfx"];
									SF.nolimits = subitem["nolimits"].is_null() ? SF.nolimits : (bool)subitem["nolimits"];
									SF.norampin = subitem["norampin"].is_null() ? SF.norampin : (bool)subitem["norampin"];

									SF.spreset = subitem["spreset"].is_null() ? SF.spreset : (int)subitem["spreset"];
									SF.sbank = subitem["sbank"].is_null() ? SF.sbank : (int)subitem["sbank"];
									SF.dpreset = subitem["dpreset"].is_null() ? SF.dpreset : (int)subitem["dpreset"];
									SF.dbank = subitem["dbank"].is_null() ? SF.dbank : (int)subitem["dbank"];
									SF.dbanklsb = subitem["dbanklsb"].is_null() ? SF.dbanklsb : (int)subitem["dbanklsb"];

									SoundFonts.push_back(SF);
								}
								else NERROR("The SoundFont \"%s\" could not be found!", false, path);
							}

							// If it's not, then let's loop until the end of the JSON struct
						}

						ListPath = new char[MAX_PATH_LONG];

						if (ListPath)
							strncpy(ListPath, OMPath, sizeof(OMPath));

						succeed = true;
					}
					else NERROR("\"%s\" does not contain a valid \"SoundFonts\" JSON structure.", false, !list.empty() ? list.c_str() : OMPath);
				}
				else NERROR("Invalid JSON structure!", false);
			}
			catch (nlohmann::json::parse_error ex) {
				NERROR("An error has occurred while parsing the SoundFont JSON. %s.", true, ex.what());
			}
			catch (nlohmann::json::type_error ex) {
				// Common error, just return nullptr and we'll retry
				LOG("The SoundFont JSON contains a value whose type does not match the expected semantics. %s.", ex.what());
			}
			catch (nlohmann::json::other_error ex) {
				NERROR("An unknown error has occurred while reading the SoundFont JSON. %s.", true, ex.what());
			}

			sfs.close();
			return succeed ? &SoundFonts : nullptr;
		}
		else {
			if (list.empty()) {
				NERRORV(L"SoundFonts JSON at path \"%s\" does not exist. The driver will create an example one for you to edit.", false, OMPath);

				SoundFont eSF;
				auto example = eSF.GetExampleList();

				sfs.close();
				sfs.open((char*)OMPath, std::fstream::out | std::fstream::trunc);
				std::string dump = example.dump(4);
				sfs.write(dump.c_str(), dump.length());
				sfs.close();
			}
			else NERRORV(L"SoundFonts JSON at path \"%s\" does not exist!", true, list.c_str());
		}
	}

	return nullptr;
}

bool OmniMIDI::SoundFontSystem::ClearList() {
	if (SoundFonts.size() > 0)
		SoundFonts.clear();

	if (ListPath)
	{
		delete[] ListPath;
		ListPath = nullptr;
	}

	return true;
}