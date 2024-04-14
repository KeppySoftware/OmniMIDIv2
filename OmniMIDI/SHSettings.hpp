/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifndef _SHSETTINGS_H
#define _SHSETTINGS_H

#pragma once

#ifdef _WIN32
#include <shlwapi.h>
#endif

#include <fstream>
#include "ErrSys.hpp"
#include "SynthMain.hpp"
#include "KDMAPI.hpp"

namespace OmniMIDI {
	class SHSettings : public OMSettings {
	public:
		// Global settings
		char Renderer = BASSMIDI;
		bool KDMAPIEnabled = true;
		bool DebugMode = false;
		std::string CustomRenderer = "empty";
		std::vector<std::string> Blacklist;

		SHSettings() {
			// When you initialize Settings(), load OM's own settings by default
			OMShared::SysPath Utils;
			wchar_t OMPath[MAX_PATH] = { 0 };
			wchar_t OMBPath[MAX_PATH] = { 0 };
			wchar_t OMDBPath[MAX_PATH] = { 0 };

			if (Utils.GetFolderPath(OMShared::FIDs::UserFolder, OMPath, sizeof(OMPath))) {
				wcscpy_s(OMBPath, OMPath);
				wcscpy_s(OMDBPath, OMPath);
				swprintf_s(OMPath, L"%s\\OmniMIDI\\settings.json\0", OMPath);
				swprintf_s(OMBPath, L"%s\\OmniMIDI\\blacklist.json\0", OMBPath);
				swprintf_s(OMDBPath, L"%s\\OmniMIDI\\defblacklist.json\0", OMBPath);
				LoadJSON(OMPath);
				LoadBlacklist(OMBPath);
			}
		}

		void CreateJSON(wchar_t* Path) {
			std::fstream st;
			st.open(Path, std::fstream::out | std::ofstream::trunc);
			if (st.is_open()) {
				nlohmann::json defset = {
					{ "WDMInit", {
						JSONGetVal(Renderer),
						JSONGetVal(DebugMode),
						JSONGetVal(CustomRenderer),
						JSONGetVal(KDMAPIEnabled)
					}}
				};

				std::string dump = defset.dump(1);
				st.write(dump.c_str(), dump.length());
				st.close();
			}
		}

		// Here you can load your own JSON, it will be tied to ChangeSetting()
		void LoadJSON(wchar_t* Path) {
			std::fstream st;
			st.open(Path, std::fstream::in);

			if (st.is_open()) {
				try {
					// Read the JSON data from there
					auto json = nlohmann::json::parse(st, nullptr, false, true);

					if (json != nullptr) {
						auto& JsonData = json["WDMInit"];

						if (!(JsonData == nullptr)) {
							JSONSetVal(int, Renderer);
							JSONSetVal(bool, DebugMode);
							JSONSetVal(bool, KDMAPIEnabled);
							JSONSetVal(std::string, CustomRenderer);
						}
					}
					else throw nlohmann::json::type_error::create(667, "json structure is not valid", nullptr);
				}
				catch (nlohmann::json::type_error ex) {
					st.close();
					LOG(SetErr, "The JSON is corrupted or malformed! nlohmann::json says: %s", ex.what());
					CreateJSON(Path);
					return;
				}
				st.close();
			}
		}

		void LoadBlacklist(wchar_t* Path) {
			std::fstream st;
			st.open(Path, std::fstream::in);

			if (st.is_open()) {
				try {
					// Read the JSON data from there
					auto JsonData = nlohmann::json::parse(st, nullptr, false, true);

					if (JsonData != nullptr) {
						JSONSetVal(std::vector<std::string>, Blacklist);
					}
					else throw nlohmann::json::type_error::create(667, "json structure is not valid", nullptr);
				}
				catch (nlohmann::json::type_error ex) {
					st.close();
					return;
				}
				st.close();
			}
		}

		bool IsBlacklistedProcess() {
			char szFilePath[MAX_PATH];
			char szFileName[MAX_PATH];

			if (!Blacklist.empty())
			{
				GetModuleFileNameA(NULL, szFilePath, MAX_PATH);
				strncpy_s(szFileName, PathFindFileNameA(szFilePath), MAX_PATH);

				for (int i = 0; i < Blacklist.size(); i++) {
					if (!_stricmp(szFilePath, Blacklist[i].c_str())) {
						return true;
					}

					if (!_stricmp(szFileName, Blacklist[i].c_str())) {
						return true;
					}
				}
			}

			return false;
		}
	};
}

#endif