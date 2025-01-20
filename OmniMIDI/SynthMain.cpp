/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.

*/

#include "SynthMain.hpp"

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

		MiscFuncs.uSleep(-1000000);
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

std::vector<OmniMIDI::SoundFont>* OmniMIDI::SoundFontSystem::LoadList(std::wstring list) {
	wchar_t OMPath[MAX_PATH_LONG] = { 0 };
	const char* path = nullptr;
	bool succeed = false;

	if (SoundFonts.size() > 0)
		return &SoundFonts;

	if (Utils.GetFolderPath(OMShared::FIDs::UserFolder, OMPath, sizeof(OMPath))) {
		swprintf_s(OMPath, L"%s\\OmniMIDI\\lists\\OmniMIDI_A.json\0", OMPath);

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

								if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
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

						ListPath = new wchar_t[MAX_PATH_LONG];

						if (ListPath)
							wcscpy(ListPath, OMPath);

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
				sfs.open(OMPath, std::fstream::out | std::fstream::trunc);
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