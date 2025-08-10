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
 *
 * This file contains the required code to run the driver under Windows
 * 7 SP1 and later. This file is useful only if you want to compile the
 * driver under Windows, it's not needed for Linux/macOS porting.
 */

#ifdef _WIN32

#include "ShakraPipe.hpp"

std::wstring OmniMIDI::ShakraPipe::GenerateID() {
	auto randchar = []() -> char
		{
			const char charset[] =
				"0123456789"
				"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				"abcdefghijklmnopqrstuvwxyz";
			const size_t max_index = (sizeof(charset) - 1);
			return charset[rand() % max_index];
		};
	std::wstring str(32, 0);
	std::generate_n(str.begin(), 32, randchar);
	return str;
}

bool OmniMIDI::ShakraPipe::StartSynthModule() {
	_wspConfig= LoadSynthConfig<WSPSettings>();

	void* temp = nullptr;
	std::wstring TempID = GenerateID();
	wchar_t SBName[MAX_PATH] = { 0 }, LBName[MAX_PATH] = { 0 };

	swprintf_s(SBName, MAX_PATH, FileMappingTemplate, L"Short", TempID.c_str());
	swprintf_s(LBName, MAX_PATH, FileMappingTemplate, L"Long", TempID.c_str());

	if ((PShortEvents = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | SEC_COMMIT, 0, sizeof(EvBuf), SBName)) != 0) {
		temp = MapViewOfFile(PShortEvents, FILE_MAP_ALL_ACCESS, 0, 0, 0);

		if (temp) {
			ShortEvents = (EvBuf*)temp;
			ShortEvents->Allocate(_wspConfig->EvBufSize);
			Message("ShortEvents mapping worked!");
		}
		else Error("An error occurred while mapping the view for the PShortEvents file mapping!", true);
	}
	else Error("An error occurred while creating the PShortEvents file mapping!", true);

	temp = nullptr;
	if ((PLongEvents = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | SEC_COMMIT, 0, sizeof(LEvBuf), LBName)) != 0) {
		temp = MapViewOfFile(PLongEvents, FILE_MAP_ALL_ACCESS, 0, 0, 0);

		if (temp) {
			LongEvents = (LEvBuf*)temp;
			LongEvents->Allocate(32);
			Message("LongEvents mapping worked!");
		}
		else Error("An error occurred while mapping the view for the PLongEvents file mapping!", true);
	}
	else Error("An error occurred while creating the PLongEvents file mapping!", true);

	if (!ShortEvents || !LongEvents) {
		StopSynthModule();
		return false;
	}

	return true;
}

bool OmniMIDI::ShakraPipe::StopSynthModule() {
	if (ShortEvents) {
		ShortEvents->Free();
		UnmapViewOfFile(ShortEvents);
		ShortEvents = nullptr;
	}

	if (PShortEvents) {
		CloseHandle(PShortEvents);
		PShortEvents = nullptr;
	}

	if (LongEvents) {
		LongEvents->Free();
		UnmapViewOfFile(LongEvents);
		LongEvents = nullptr;
	}

	if (PLongEvents) {
		CloseHandle(PLongEvents);
		PLongEvents = nullptr;
	}

	FreeSynthConfig(_wspConfig);	

	return true;
}

uint32_t OmniMIDI::ShakraPipe::PlayLongEvent(uint8_t* ev, uint32_t size) {
	if (LongEvents)
		return UPlayLongEvent(ev, size);

	return 0;
}

uint32_t OmniMIDI::ShakraPipe::UPlayLongEvent(uint8_t* ev, uint32_t size) {
	LongEvents->Write(ev, size);
	return size;
}

#endif