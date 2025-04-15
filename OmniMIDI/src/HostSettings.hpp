/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#ifndef _SHSETTINGS_H
#define _SHSETTINGS_H

#pragma once

#ifdef _WIN32
#include <shlwapi.h>
#endif

#include <fstream>
#include "Common.hpp"
#include "ErrSys.hpp"
#include "SynthModule.hpp"
#include "KDMAPI.hpp"

namespace OmniMIDI {
	class HostSettings : public SettingsModule {
	public:
		std::vector<std::string> Blacklist;
		OMShared::Funcs Utils = OMShared::Funcs(ErrLog); 

		HostSettings(ErrorSystem::Logger* PErr) : SettingsModule(PErr) {
			// When you initialize Settings(), load OM's own settings by default
			char OMPath[MAX_PATH] = { 0 };

#ifdef _WIN32
			char OMBPath[MAX_PATH] = { 0 };
			char OMDBPath[MAX_PATH] = { 0 };
#endif

			if (Utils.GetFolderPath(OMShared::FIDs::UserFolder, OMPath, sizeof(OMPath))) {
				snprintf(OMPath, sizeof(OMPath), "%s/OmniMIDI/settings.json", OMPath);
				InitConfig(false, DUMMY_STR, sizeof(DUMMY_STR));
#ifdef _WIN32
				snprintf(OMBPath, sizeof(OMBPath), "%s/OmniMIDI/blacklist.json", OMBPath);
				snprintf(OMDBPath, sizeof(OMDBPath), "%s/OmniMIDI/defblacklist.json", OMDBPath);
				LoadBlacklist(OMBPath);
#endif
			}

		}

		void LoadBlacklist(char* Path) {
#ifdef _WIN32
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
				catch (const nlohmann::json::type_error& ex) {
					st.close();
					return;
				}
				st.close();
			}
#endif
		}

		bool IsBlacklistedProcess() {
			bool flag = false;

#ifdef _WIN32
			char* szFilePath = new char[MAX_PATH_LONG];
			char* szFileName = new char[MAX_PATH_LONG];

			if (!szFilePath || !szFileName) {
				if (szFilePath)
					delete[] szFilePath;

				if (szFileName)
					delete[] szFileName;

				return false;
			}

			if (!Blacklist.empty()) {
				auto len = GetModuleFileNameA(NULL, szFilePath, MAX_PATH_LONG);

				if (len) {
					strncpy(szFileName, PathFindFileNameA(szFilePath), len);

					for (size_t i = 0; i < Blacklist.size(); i++) {
						if (!_stricmp(szFilePath, Blacklist[i].c_str())) {
							flag = true;
							break;
						}

						if (!_stricmp(szFileName, Blacklist[i].c_str())) {
							flag = true;
							break;
						}
					}
				}

				flag = false;
			}

			delete[] szFilePath;
			delete[] szFileName;
#endif
			return flag;
		}
	};
}

#endif