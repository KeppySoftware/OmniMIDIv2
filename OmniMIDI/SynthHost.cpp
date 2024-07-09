/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#include "SynthHost.hpp"

#ifdef _WIN32 
bool OmniMIDI::SynthHost::SpInit() {
	if (StreamPlayer != nullptr)
		SpFree();

	StreamPlayer = new OmniMIDI::CookedPlayer(Synth, DrvCallback);
	LOG(SHErr, "StreamPlayer allocated.");

	if (!StreamPlayer)
	{
		SpFree();
		return false;
	}

	LOG(SHErr, "StreamPlayer address: %x", StreamPlayer);
	return true;
}

bool OmniMIDI::SynthHost::SpFree() {
	if (StreamPlayer != nullptr) {
		if (StreamPlayer->EmptyQueue()) {
			StreamPlayer->Stop();
			delete StreamPlayer;
			StreamPlayer = nullptr;
		}
		else return false;
	}

	return true;
}
#endif

void OmniMIDI::SynthHost::HostHealthCheck() {
	wchar_t* confPath = SHSettings->GetConfigPath();

	if (confPath) {
		auto ftime = std::filesystem::last_write_time(confPath);
		auto chktime = ftime;

		while (!Synth->IsSynthInitialized());

		while (Synth->IsSynthInitialized() && Synth->SynthID() != EMPTYMODULE) {
			ftime = std::filesystem::last_write_time(confPath);

			if (chktime != ftime) {			
				if (Stop(true)) {
					Start();
					confPath = SHSettings->GetConfigPath();
					while (!Synth->IsSynthInitialized() && Synth->SynthID() != EMPTYMODULE);
					chktime = std::filesystem::last_write_time(confPath);
				}
			}
		}
	}
}

void OmniMIDI::SynthHost::RefreshSettings() {
	LOG(SHErr, "Refreshing synth host settings...");

	auto tSHSettings = new OmniMIDI::SHSettings;
	auto oSHSettings = SHSettings;

	SHSettings = tSHSettings;
	delete oSHSettings;
	oSHSettings = nullptr;

	LOG(SHErr, "Settings refreshed! (renderer %d, kdmapi %d, crender %s)", SHSettings->GetRenderer(), SHSettings->IsKDMAPIEnabled(), SHSettings->GetCustomRenderer());
}

bool OmniMIDI::SynthHost::Start(bool StreamPlayer) {
	RefreshSettings();

	auto tSynth = GetSynth();
	auto oSynth = Synth;

	if (tSynth) {
		if (tSynth->LoadSynthModule()) {
			tSynth->SetInstance(hwndMod);

			if (tSynth->StartSynthModule()) {
#ifdef _WIN32 
				if (StreamPlayer) {
					if (!SpInit())
						return false;
				}
#endif

				if (!_HealthThread.joinable()) 
					_HealthThread = std::jthread(&SynthHost::HostHealthCheck, this);

				if (_HealthThread.joinable()) {
					Synth = tSynth;
					delete oSynth;
					return true;
				}
				else NERROR(SHErr, "_HealthThread failed. (ID: %x)", true, _HealthThread.get_id());

				if (!tSynth->StopSynthModule())
					FNERROR(SHErr, "StopSynthModule() failed!!!");
			}
			else NERROR(SHErr, "StartSynthModule() failed! The driver will not output audio.", true);
		}
		else NERROR(SHErr, "LoadSynthModule() failed! The driver will not output audio.", true);
	}

	if (!tSynth->UnloadSynthModule())
		FNERROR(SHErr, "UnloadSynthModule() failed!!!");

	delete tSynth;

	return false;
}

bool OmniMIDI::SynthHost::Stop(bool restart) {
	auto tSynth = new SynthModule;
	auto oSynth = Synth;

	Synth = tSynth;

#ifdef _WIN32 
	SpFree();
#endif

	if (!oSynth->StopSynthModule()) 
		FNERROR(SHErr, "StopSynthModule() failed!!!");

	if (!oSynth->UnloadSynthModule())
		FNERROR(SHErr, "UnloadSynthModule() failed!!!");

	delete oSynth;

	if (!restart) {
		if (DrvCallback->IsCallbackReady()) {
			DrvCallback->CallbackFunction(MOM_CLOSE, 0, 0);
			DrvCallback->ClearCallbackFunction();
			LOG(SHErr, "Callback system has been freed.");
		}

		if (_HealthThread.joinable())
			_HealthThread.join();
	}

	return true;
}

OmniMIDI::SynthModule* OmniMIDI::SynthHost::GetSynth() {
	SynthModule* tSynth;

	char r = SHSettings->GetRenderer();
	switch (r) {
	case Synthesizers::External:
		extModule = loadLib(SHSettings->GetCustomRenderer());

		if (extModule) {
			auto iM = reinterpret_cast<rInitModule>(getAddr(extModule, "initModule"));

			if (iM) {
				tSynth = iM();

				if (tSynth) {
					LOG(SHErr, "R%d (EXTERNAL >> %s)",
						r,
						SHSettings->GetCustomRenderer());
					break;
				}
			}
		}

		tSynth = new OmniMIDI::SynthModule;
		NERROR(SHErr, "The requested external module (%s) could not be loaded.", SHSettings->GetCustomRenderer());
		break;

	case Synthesizers::TinySoundFont:
		tSynth = new OmniMIDI::TinySFSynth;
		LOG(SHErr, "R%d (TINYSF)", r);
		break;

	case Synthesizers::FluidSynth:
		tSynth = new OmniMIDI::FluidSynth;
		LOG(SHErr, "R%d (FLUIDSYNTH)", r);
		break;

#if !defined _M_ARM
	case Synthesizers::BASSMIDI:
		tSynth = new OmniMIDI::BASSSynth;
		LOG(SHErr, "R%d (BASSMIDI)", r);
		break;
#endif

#if defined _M_AMD64
	case Synthesizers::XSynth:
		tSynth = new OmniMIDI::XSynth;
		LOG(SHErr, "R%d (XSYNTH)", r);
		break;
#endif

#if !defined _M_ARM || !defined _M_ARM64
	case Synthesizers::ksynth:
		tSynth = new OmniMIDI::KSynthM;
		LOG(SHErr, "R%d (KSYNTH)", r);
		break;
#endif

#if defined WIN32 
	case Synthesizers::ShakraPipe:
		tSynth = new OmniMIDI::ShakraPipe;
		LOG(SHErr, "R%d (SHAKRA)", r);
		break;
#endif

	default:
		tSynth = new OmniMIDI::SynthModule;
		NERROR(SHErr, "The chosen synthesizer (Syn%d) is not available on this platform, or does not exist.", false, r);
		break;
	}

	return tSynth;
}

void OmniMIDI::SynthHost::PlayShortEvent(unsigned char status, unsigned char param1, unsigned char param2) {
	Synth->PlayShortEvent(status, param1, param2);
}

OmniMIDI::SynthResult OmniMIDI::SynthHost::PlayLongEvent(char* ev, unsigned int size) {
	if (!Synth->IsSynthInitialized())
		return NotInitialized;

	if (!ev || size < 4 || (unsigned char)ev[size - 1] != 0xF7)
		return InvalidBuffer;

	for (int readHead = 0, n = size - 1; readHead < n; ) {
		switch (ev[readHead] & 0xF0) {
		case SystemMessageStart:
		{
			char pos = 0;
			char vendor = ev[readHead + 1] & 0xFF;
			unsigned int buf = ev[readHead] << 8 | ev[readHead + 1];

			if (vendor < 0x80) {
				switch (vendor) {
				// Universal
				case 0x7F:
				{
					char command = ev[readHead + 3];
					if (command == 0x06) {
						readHead += 4;

						char subcommand = ev[readHead];
						while (subcommand != 0x7F) {
							switch (subcommand) {
							case 0x01:
								StreamPlayer->Stop();
								StreamPlayer->EmptyQueue();
								break;
							case 0x02:
							case 0x03:
								StreamPlayer->Start();
								break;
							case 0x09:
								StreamPlayer->Stop();
								break;
							case 0x0D:
								return Synth->Reset();
							default:
								if (ev[readHead + 1] == 0x7F)
									return Ok;

								return NotSupported;
							}

							subcommand = ev[readHead++];
						}
					}
					else return NotSupported;

					return Ok;
				}

				// Roland
				case 0x41:
				{
					char devid = ev[readHead + 2];
					char modid = ev[readHead + 3];
					char command = ev[readHead + 4];

					if (devid > 0x1F)
						return InvalidBuffer;

					if (command == 0x12) {
						char addrBlock = ev[readHead + 5];
						char synthPart = ev[readHead + 6];
						char addrPt3 = ev[readHead + 7];

						unsigned int addr = (addrBlock << 16) | (synthPart << 8) | addrPt3;

						if (addr == 0x40007F) {
							char resetType = ev[readHead + 8];
							return Synth->Reset(!resetType ? resetType : vendor);
						}

						switch (modid) {
						case 0x42:
							switch (addrBlock) {
							case 0x00:

							}

						case 0x45:
							switch (addrBlock) {
							// Display data
							case 0x10:
							{
								char mult = 0;

								addrPt3 = ev[readHead + (7 * mult)];
								char dataType = ev[readHead + (10 * mult)];

								switch (dataType) {
								case 0x20:
									for (/* tit */; mult < 32; mult++) {
										if (addrPt3 > 0x1F)
											break;

										addrPt3 = ev[readHead + (7 * mult)];
									}

									if (char* asciiStream = new char[mult]) {
										for (int i = 0; i < mult; i++) {
											asciiStream[i] = ev[i + (13 * i)];
										}

										LOG(SHErr, "Roland Display Data: %s", asciiStream);

										delete[] asciiStream;
									}
									return Ok;

								case 0x40:
									return NotSupported;
								}

								return Ok;
							}

							default:
								return NotSupported;
							}
						
						default:
							return NotSupported;
						}
					}
					else return NotSupported;

					return Ok;
				}

				default:
					readHead += 5;
					break;
				}

				LOG(SHErr, "SysEx Begin: 0x%x", buf);

				while ((buf & 0xFF) != SystemMessageEnd) {

					switch (pos) {
					case 0:
						buf = ev[readHead + pos];
						break;

					case 1:
					case 2:
						buf = buf << 8 | ev[readHead + pos];
						break;
					}

					pos++;

					if (pos == 3) {
						LOG(SHErr, "SysEx Ev: 0x%x", buf);

						if (Synth->PlayLongEvent(ev, 3))
							return InvalidParameter;

						pos = 0;

						readHead += 3;
					}
				}

				LOG(SHErr, "SysEx End: 0x%x", buf);
				return Ok;
			}

			continue;
		}

		case ActiveSensing:
		case SystemMessageEnd:
			break;

		default:
			Synth->PlayShortEvent(ev[readHead], ev[readHead + 1], ev[readHead + 2]);
			readHead += 3;
			continue;
		}
	}

	return Ok;
}