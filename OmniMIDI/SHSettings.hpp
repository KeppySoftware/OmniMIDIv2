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
#include "Common.hpp"
#include "ErrSys.hpp"
#include "SynthMain.hpp"
#include "KDMAPI.hpp"

namespace OmniMIDI {
	class SHSettings : public OMSettings {
	public:
		std::vector<std::string> Blacklist;

		SHSettings(ErrorSystem::Logger* PErr) : OMSettings(PErr) {
#ifdef _WIN32
			// When you initialize Settings(), load OM's own settings by default
			OMShared::SysPath Utils;
			wchar_t OMPath[MAX_PATH] = { 0 };
			wchar_t OMBPath[MAX_PATH] = { 0 };
			wchar_t OMDBPath[MAX_PATH] = { 0 };

			if (Utils.GetFolderPath(OMShared::FIDs::UserFolder, OMPath, sizeof(OMPath))) {
				wcscpy(OMBPath, OMPath);
				wcscpy(OMDBPath, OMPath);
				swprintf(OMPath, sizeof(OMPath), L"%s\\OmniMIDI\\settings.json\0", OMPath);
				swprintf(OMBPath, sizeof(OMBPath), L"%s\\OmniMIDI\\blacklist.json\0", OMBPath);
				swprintf(OMDBPath, sizeof(OMDBPath), L"%s\\OmniMIDI\\defblacklist.json\0", OMDBPath);
				InitConfig(false, DUMMY_STR, sizeof(DUMMY_STR));
				LoadBlacklist(OMBPath);
			}
#endif
		}

		void LoadBlacklist(wchar_t* Path) {
#ifdef _WIN32
			std::fstream st;
			st.open((char*)Path, std::fstream::in);

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
#endif
		}

		bool IsBlacklistedProcess() {
#ifdef _WIN32
			bool flag = false;
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

				GetModuleFileNameA(NULL, szFilePath, MAX_PATH_LONG);
				strncpy(szFileName, PathFindFileNameA(szFilePath), MAX_PATH_LONG);

				for (int i = 0; i < Blacklist.size(); i++) {
					if (!_stricmp(szFilePath, Blacklist[i].c_str())) {
						flag = true;
						break;
					}

					if (!_stricmp(szFileName, Blacklist[i].c_str())) {
						flag = true;
						break;
					}
				}
		flag = false;

			}
			delete[] szFilePath;
			delete[] szFileName;
			return flag;
#else
			return false;
#endif
		}
	};
}

#endif