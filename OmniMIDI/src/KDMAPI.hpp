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

#ifndef _KDMAPI_H
#define _KDMAPI_H

#pragma once

// KDMAPI settings
#define KDMAPI_SET						0x0
#define KDMAPI_GET						0x1
#define KDMAPI_MANAGE					0x2
#define KDMAPI_LEAVE					0x3

// ??
#define KDMAPI_SOUNDFONT				0xF0000

// DO NOT USE THOSE, THEY ARE USED INTERNALLY BY OMNIMIDI
#define WINMMWRP_OFF					0xFFFFE
#define WINMMWRP_ON						0xFFFFF
// DO NOT USE THOSE, THEY ARE USED INTERNALLY BY OMNIMIDI

#define KDMAPI_STREAM					0xFFFFD
#define KDMAPI_CAPFRAMERATE				0x10000
#define KDMAPI_DEBUGMMODE				0x10001
#define KDMAPI_DISABLEFADEOUT			0x10002
#define KDMAPI_DONTMISSNOTES			0x10003

#define KDMAPI_ENABLESFX				0x10004
#define KDMAPI_FULLVELOCITY				0x10005
#define KDMAPI_IGNOREVELOCITYRANGE		0x10006
#define KDMAPI_IGNOREALLEVENTS			0x10007
#define KDMAPI_IGNORESYSEX				0x10008
#define KDMAPI_IGNORESYSRESET			0x10009
#define KDMAPI_LIMITRANGETO88			0x10010
#define KDMAPI_MT32MODE					0x10011
#define KDMAPI_MONORENDERING			0x10012
#define KDMAPI_NOTEOFF1					0x10013
#define KDMAPI_EVENTPROCWITHAUDIO		0x10014
#define KDMAPI_SINCINTER				0x10015
#define KDMAPI_SLEEPSTATES				0x10016

#define KDMAPI_AUDIOBITDEPTH			0x10017
#define KDMAPI_AUDIOFREQ				0x10018
#define KDMAPI_CURRENTENGINE			0x10019
#define KDMAPI_BUFFERLENGTH				0x10020
#define KDMAPI_MAXRENDERINGTIME			0x10021
#define KDMAPI_MINIGNOREVELRANGE		0x10022
#define KDMAPI_MAXIGNOREVELRANGE		0x10023
#define KDMAPI_OUTPUTVOLUME				0x10024
#define KDMAPI_TRANSPOSE				0x10025
#define KDMAPI_MAXVOICES				0x10026
#define KDMAPI_SINCINTERCONV			0x10027

#define KDMAPI_OVERRIDENOTELENGTH		0x10028
#define KDMAPI_NOTELENGTH				0x10029
#define KDMAPI_ENABLEDELAYNOTEOFF		0x10030
#define KDMAPI_DELAYNOTEOFFVAL			0x10031

#define KDMAPI_CHANUPDLENGTH			0x10032

#define KDMAPI_UNLOCKCHANS				0x10033

#endif