/*
 * SPDX-License-Identifier: MIT
 *
 * OmniMIDI
 *
 * Copyright (c) 2024 Keppy's Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the MIT License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * MIT License for more details.
 *
 * You should have received a copy of the MIT License along with this
 * program.  If not, see <https://opensource.org/license/mit/>.
 */

#ifndef _SHSETTINGS_H
#define _SHSETTINGS_H

#pragma once

#ifdef _WIN32
#include <shlwapi.h>
#include <fstream>
#endif

#include "Common.hpp"
#include "ErrSys.hpp"
#include "synth/SynthModule.hpp"
#include <unordered_map>
#include <unordered_set>

namespace OmniMIDI {
	class HostSettings : public SettingsModule {
	public:
		std::vector<std::string> Blacklist;
		OMShared::Funcs Utils;

		struct EventOverride {
			bool ignore = false;
			bool modifyNoteLength = false;
			uint32_t forcedNoteLengthMs = 0;
			bool modifyVelocity = false;
			uint8_t velocityMultiplier = 100;
			bool setFixedVelocity = false;
			uint8_t fixedVelocityValue = 127;
			bool modifyChannel = false;
			uint8_t targetChannel = 0;
		};

		struct OverrideSettings {
			std::unordered_map<uint8_t, EventOverride> eventTypeOverrides;
			std::unordered_set<uint8_t> ignoredEventTypes;
			std::unordered_set<uint8_t> ignoredChannels;
			bool globalNoteLengthOverride = false;
			uint32_t globalForcedNoteLengthMs = 0;
			bool enabled = false;
		} eventOverrides; 

		HostSettings(ErrorSystem::Logger* PErr) : SettingsModule(PErr) {
			// When you initialize Settings(), load OM's own settings by default
			char userFolder[MAX_PATH_LONG] = { 0 };
			char OMPath[MAX_PATH_LONG] = { 0 };

#ifdef _WIN32
			char OMBPath[MAX_PATH_LONG] = { 0 };
			char OMDBPath[MAX_PATH_LONG] = { 0 };
#endif

			size_t szPath = sizeof(char) * MAX_PATH_LONG;

			if (Utils.GetFolderPath(OMShared::FIDs::UserFolder, userFolder, szPath)) {
				snprintf(OMPath, szPath, "%s/OmniMIDI/settings.json", userFolder);
				Utils.CreateFolder(OMPath, szPath);
				
				InitConfig(false, DUMMY_STR, sizeof(DUMMY_STR));
#ifdef _WIN32
				snprintf(OMBPath, sizeof(OMBPath), "%s/OmniMIDI/blacklist.json", OMBPath);
				snprintf(OMDBPath, sizeof(OMDBPath), "%s/OmniMIDI/defblacklist.json", OMDBPath);
				LoadBlacklist(OMBPath);
#endif
				LoadEventOverrides();
			}

		}

		void LoadEventOverrides() {
			if (mainptr != nullptr && !mainptr["EventOverrides"].is_null()) {
				auto evOverrides = mainptr["EventOverrides"];
				
				if (!evOverrides["enabled"].is_null())
					eventOverrides.enabled = evOverrides["enabled"].get<bool>();
					
				if (!evOverrides["globalNoteLengthOverride"].is_null())
					eventOverrides.globalNoteLengthOverride = evOverrides["globalNoteLengthOverride"].get<bool>();
					
				if (!evOverrides["globalForcedNoteLengthMs"].is_null())
					eventOverrides.globalForcedNoteLengthMs = evOverrides["globalForcedNoteLengthMs"].get<uint32_t>();
				
				if (!evOverrides["ignoredEventTypes"].is_null() && evOverrides["ignoredEventTypes"].is_array()) {
					eventOverrides.ignoredEventTypes.clear();
					for (const auto& item : evOverrides["ignoredEventTypes"]) {
						eventOverrides.ignoredEventTypes.insert(item.get<uint8_t>());
					}
				}
				
				if (!evOverrides["ignoredChannels"].is_null() && evOverrides["ignoredChannels"].is_array()) {
					eventOverrides.ignoredChannels.clear();
					for (const auto& item : evOverrides["ignoredChannels"]) {
						eventOverrides.ignoredChannels.insert(item.get<uint8_t>());
					}
				}
				
				if (!evOverrides["eventTypeOverrides"].is_null() && evOverrides["eventTypeOverrides"].is_object()) {
					eventOverrides.eventTypeOverrides.clear();
					for (const auto& [key, value] : evOverrides["eventTypeOverrides"].items()) {
						uint8_t eventType = std::stoul(key);
						EventOverride override;
						
						if (!value["ignore"].is_null()) override.ignore = value["ignore"].get<bool>();
						if (!value["modifyNoteLength"].is_null()) override.modifyNoteLength = value["modifyNoteLength"].get<bool>();
						if (!value["forcedNoteLengthMs"].is_null()) override.forcedNoteLengthMs = value["forcedNoteLengthMs"].get<uint32_t>();
						if (!value["modifyVelocity"].is_null()) override.modifyVelocity = value["modifyVelocity"].get<bool>();
						if (!value["velocityMultiplier"].is_null()) override.velocityMultiplier = value["velocityMultiplier"].get<uint8_t>();
						if (!value["setFixedVelocity"].is_null()) override.setFixedVelocity = value["setFixedVelocity"].get<bool>();
						if (!value["fixedVelocityValue"].is_null()) override.fixedVelocityValue = value["fixedVelocityValue"].get<uint8_t>();
						if (!value["modifyChannel"].is_null()) override.modifyChannel = value["modifyChannel"].get<bool>();
						if (!value["targetChannel"].is_null()) override.targetChannel = value["targetChannel"].get<uint8_t>();
						
						eventOverrides.eventTypeOverrides[eventType] = override;
					}
				}
			}
		}

		void SaveEventOverrides() {
			if (mainptr != nullptr) {
				nlohmann::json evOverrides;
				
				evOverrides["enabled"] = eventOverrides.enabled;
				evOverrides["globalNoteLengthOverride"] = eventOverrides.globalNoteLengthOverride;
				evOverrides["globalForcedNoteLengthMs"] = eventOverrides.globalForcedNoteLengthMs;
				
				evOverrides["ignoredEventTypes"] = nlohmann::json::array();
				for (const auto& eventType : eventOverrides.ignoredEventTypes) {
					evOverrides["ignoredEventTypes"].push_back(eventType);
				}
				
				evOverrides["ignoredChannels"] = nlohmann::json::array();
				for (const auto& channel : eventOverrides.ignoredChannels) {
					evOverrides["ignoredChannels"].push_back(channel);
				}
				
				evOverrides["eventTypeOverrides"] = nlohmann::json::object();
				for (const auto& [eventType, override] : eventOverrides.eventTypeOverrides) {
					nlohmann::json overrideJson;
					overrideJson["ignore"] = override.ignore;
					overrideJson["modifyNoteLength"] = override.modifyNoteLength;
					overrideJson["forcedNoteLengthMs"] = override.forcedNoteLengthMs;
					overrideJson["modifyVelocity"] = override.modifyVelocity;
					overrideJson["velocityMultiplier"] = override.velocityMultiplier;
					overrideJson["setFixedVelocity"] = override.setFixedVelocity;
					overrideJson["fixedVelocityValue"] = override.fixedVelocityValue;
					overrideJson["modifyChannel"] = override.modifyChannel;
					overrideJson["targetChannel"] = override.targetChannel;
					
					evOverrides["eventTypeOverrides"][std::to_string(eventType)] = overrideJson;
				}
				
				mainptr["EventOverrides"] = evOverrides;
				jsonptr["OmniMIDI"] = mainptr;
				WriteConfig();
			}
		}

		const OverrideSettings& GetEventOverrides() const { return eventOverrides; }
		void SetEventOverrides(const OverrideSettings& overrides) { 
			eventOverrides = overrides; 
			SaveEventOverrides();
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