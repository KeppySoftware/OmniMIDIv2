/*

	OmniMIDI v15+ (Rewrite)

	This file contains the required code to run the driver under any OS.

*/

#ifndef _SOUNDFONTSYSTEM_H
#define _SOUNDFONTSYSTEM_H

#pragma once

#include "Common.hpp"
#include "ErrSys.hpp"
#include "Utils.hpp"
#include "nlohmann\json.hpp"
#include <fstream>
#include <future>
#include <string>
#include <filesystem>

namespace OmniMIDI {
	class SoundFont {
	public:
		std::string path;

		bool enabled = true;
		bool xgdrums = false;
		bool linattmod = false;
		bool lindecvol = false;
		bool minfx = false;
		bool nolimits = false;
		bool norampin = false;

		int spreset = -1;
		int sbank = -1;
		int dpreset = -1;
		int dbank = 0;
		int dbanklsb = 0;
	};

	class SoundFontSystem {
	private:
		ErrorSystem::Logger SfErr;
		OMShared::SysPath Utils;
		wchar_t* ListPath = nullptr;
		std::filesystem::file_time_type ListLastEdit;
		std::vector<SoundFont> SoundFonts;

	public:
		std::vector<OmniMIDI::SoundFont>* LoadList(std::wstring list = L"");
		bool ClearList();
		bool ListModified(bool init = false);
	};
}

#endif