/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#include "SynthMain.hpp"

bool OmniMIDI::SettingsModule::InitConfig(bool write, const char* pSynthName, size_t pSynthName_sz) {
	char* userProfile = new char[MAX_PATH_LONG] { 0 };
	SettingsPath = new char[MAX_PATH_LONG] { 0 };
	size_t szSetPath = sizeof(SettingsPath) * MAX_PATH_LONG;

	if (JSONStream && JSONStream->is_open())
		CloseConfig();

	if (SynthName == nullptr && pSynthName != nullptr) {
		if (strcmp(pSynthName, DUMMY_STR)) {
			if (pSynthName_sz > SYNTHNAME_SZ)
				pSynthName_sz = SYNTHNAME_SZ;

			SynthName = new char[pSynthName_sz];
			if (strcpy(SynthName, pSynthName) != SynthName) {
				NERROR("An error has occurred while parsing SynthName!", true);
				CloseConfig();
				return false;
			}
		}
	}
	else {
		NERROR("InitConfig called with no SynthName specified!", true);
		CloseConfig();
		return false;
	}

	if (Utils.GetFolderPath(OMShared::FIDs::UserFolder, userProfile, sizeof(userProfile) * MAX_PATH_LONG)) {
		LOG("Utils.GetFolderPath path: %s", userProfile);

		snprintf(SettingsPath, szSetPath, "%s/OmniMIDI/settings.json", userProfile);
		LOG("Configuration path: %s", SettingsPath);

		JSONStream->open((char*)SettingsPath, write ? (std::fstream::out | std::fstream::trunc) : std::fstream::in);
		if (JSONStream->is_open() && nlohmann::json::accept(*JSONStream, true)) {
			JSONStream->clear();
			JSONStream->seekg(0, std::ios::beg);
			jsonptr = nlohmann::json::parse(*JSONStream, nullptr, false, true);
		}

		if (jsonptr == nullptr || jsonptr["OmniMIDI"] == nullptr) {
			NERROR("No configuration file found! A new reference one will be created for you.", false);

			jsonptr["OmniMIDI"] = {
				ConfGetVal(Renderer),
				ConfGetVal(CustomRenderer),
				ConfGetVal(KDMAPIEnabled),
				ConfGetVal(DebugMode),
				ConfGetVal(Renderer),
				{ "SynthModules", nlohmann::json::object({})}
			};

			WriteConfig();
		}
			
		mainptr = jsonptr["OmniMIDI"];
		if (mainptr == nullptr) {
			NERROR("An error has occurred while parsing the settings for OmniMIDI!", true);
			CloseConfig();
			return false;
		}

		MainSetVal(int, Renderer);
		MainSetVal(bool, DebugMode);
		MainSetVal(bool, KDMAPIEnabled);
		MainSetVal(std::string, CustomRenderer);

		if (SynthName) {
			synptr = mainptr["SynthModules"][SynthName];
			if (synptr == nullptr) {
				NERROR("No configuration settings found for the selected renderer \"%s\"! A new reference one will be created for you.", false, SynthName);
				return false;
			}
		}

		return true;
	}
	else NERROR("An error has occurred while parsing the user profile path!", true);

	return false;
}

bool OmniMIDI::SettingsModule::WriteConfig() {
	JSONStream->close();

	JSONStream->open((char*)SettingsPath, std::fstream::out | std::fstream::trunc);
	std::string dump = jsonptr.dump(4);
	JSONStream->write(dump.c_str(), dump.length());
	JSONStream->close();

	return true;
}

bool OmniMIDI::SettingsModule::ReloadConfig() {
	JSONStream->close();
	return InitConfig();
}

void OmniMIDI::SettingsModule::CloseConfig() {
	if (jsonptr != nullptr && !jsonptr.is_null()) {
		jsonptr.clear();
	}

	if (mainptr != nullptr && !mainptr.is_null()) {
		mainptr.clear();
	}

	if (synptr != nullptr && !synptr.is_null()) {
		synptr.clear();
	}

	if (JSONStream->is_open())
		JSONStream->close();

	if (SettingsPath != nullptr) {
		delete SettingsPath;
		SettingsPath = nullptr;
	}

	if (SynthName != nullptr) {
		delete SynthName;
		SynthName = nullptr;
	}
}

void OmniMIDI::SettingsModule::OpenConsole() {
#if defined _WIN32 && !defined _DEBUG
	if (IsDebugMode()) {
		if (AllocConsole()) {
			OwnConsole = true;
			FILE* dummy;
			freopen_s(&dummy, "CONOUT$", "w", stdout);
			freopen_s(&dummy, "CONOUT$", "w", stderr);
			freopen_s(&dummy, "CONIN$", "r", stdin);
			std::cout.clear();
			std::clog.clear();
			std::cerr.clear();
			std::cin.clear();
		}
	}
#endif
}

void OmniMIDI::SettingsModule::CloseConsole() {
#if defined _WIN32 && !defined _DEBUG
	if (IsDebugMode() && OwnConsole) {
		FreeConsole();
		OwnConsole = false;
	}
#endif
}

bool OmniMIDI::SettingsModule::AppendToConfig(nlohmann::json &content) {
	if (content == nullptr)
		return false;

	if (jsonptr["OmniMIDI"]["SynthModules"][SynthName] != nullptr) {
		jsonptr["OmniMIDI"]["SynthModules"].erase(SynthName);
	}

	jsonptr["OmniMIDI"]["SynthModules"][SynthName] = content;
	mainptr = jsonptr["OmniMIDI"];
	synptr = mainptr["SynthModules"][SynthName];

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
	bool succeed = false;

	char tmpUtils[MAX_PATH_LONG] = { 0 };
	char OMPath[MAX_PATH_LONG] = { 0 };

	char* listPath = nullptr;
	size_t szListPath = 0;

	const char* soundfontPath = nullptr;

	if (SoundFonts.size() > 0)
		return &SoundFonts;

	if (Utils.GetFolderPath(OMShared::FIDs::UserFolder, tmpUtils, sizeof(tmpUtils))) {
		if (list.empty()) {
			snprintf(OMPath, sizeof(OMPath), "%s/OmniMIDI/lists/OmniMIDI_A.json", tmpUtils);
			listPath = OMPath;
			szListPath = sizeof(OMPath);
		}
		else {
			listPath = (char*)list.c_str();
			szListPath = list.size();
		}

		std::fstream sfs;
		sfs.open(listPath, std::fstream::in);

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

								soundfontPath = SF.path.c_str();

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
								else NERROR("The SoundFont \"%s\" could not be found!", false, soundfontPath);
							}

							// If it's not, then let's loop until the end of the JSON struct
						}

						ListPath = new char[szListPath];

						if (ListPath)
							strncpy(ListPath, listPath, szListPath);

						succeed = true;
					}
					else NERROR("\"%s\" does not contain a valid \"SoundFonts\" JSON structure.", false, listPath);
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
			NERRORV(L"SoundFonts JSON at path \"%s\" does not exist. The driver will create an example one for you to edit.", false, listPath);

			SoundFont eSF;
			auto example = eSF.GetExampleList();

			sfs.close();
			sfs.open((char*)OMPath, std::fstream::out | std::fstream::trunc);
			std::string dump = example.dump(4);
			sfs.write(dump.c_str(), dump.length());
			sfs.close();
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