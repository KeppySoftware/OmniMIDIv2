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
			bool notSpecial = false;
			char pos = 0;
			char vendor = ev[readHead + 1] & 0xFF;

			readHead += 2;
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
					// Size of 3
					char devid = ev[readHead];
					char modid = ev[readHead + 1];
					char command = ev[readHead + 2];

					if (devid > PartMax)
						return InvalidBuffer;

					readHead += 3;
					if (command == Receive) {
						unsigned int varLen = 1;

						char addrBlock = ev[readHead];
						char synthPart = ev[readHead + 1];
						char commandPart = ev[readHead + 2];
						char status = 0;
						char p1 = 0;
						char p2 = 0;

						unsigned int lastPos = 0;
						unsigned int addr = (addrBlock << 16) | (synthPart << 8) | commandPart;
						unsigned int modeSet = addr & 0xFFFF;

						unsigned char sum = addrBlock + synthPart + commandPart;
						unsigned char checksum = 0;
						unsigned char calcChecksum = 0;

						if (addr == MIDIReset) {
							char resetType = ev[readHead + 8];
							return Synth->Reset(!resetType ? resetType : vendor);
						}

						readHead += 3;
						switch (modid) {
						case 0x42:
							switch (addr & BlockDiscrim) {
							case PatchCommonA:
							case PatchCommonB: {
								switch (synthPart) {
								case 0:
								{
									switch (commandPart) {
									case 0:
									case 1:
									case 2:
									case 3: {
										varLen = 4;
										if ((commandPart & 0xF) == 0)
											lastPos = readHead;

										for (int i = 0; i < varLen; i++) {
											if (i == 1)
												p1 = ev[lastPos + i];

											if (i == 2)
												p2 = ev[lastPos + i];
										}

										checksum = ev[lastPos + (varLen + 1)];

										sum += p1 + p2;
										status = MasterTune;

										break;
									}

									case 4:
										p1 = ev[readHead++];
										checksum = ev[readHead++];
										sum += p1;
										status = MasterVolume;
										break;

									case 5:
										p1 = ev[readHead++];
										checksum = ev[readHead++];
										sum += p1;
										status = MasterKey;
										break;

									case 6:
										p1 = ev[readHead++];
										checksum = ev[readHead++];
										sum += p1;
										status = MasterPan;
										break;
									}

									break;
								}

								default:
									switch (modeSet) {
									case PatchName:
										varLen = 16;
										checksum = ev[(readHead++) + varLen];
										break;

									case ReverbMacro:
									case ReverbCharacter:
									case ReverbPreLpf:
									case ReverbLevel:
									case ReverbTime:
									case ReverbDelayFeedback:
									case ReverbPredelayTime:
									case ChorusMacro:
									case ChorusPreLpf:
									case ChorusLevel:
									case ChorusFeedback:
									case ChorusDelay:
									case ChorusRate:
									case ChorusDepth:
									case ChorusSendLevelToReverb:
									case ChorusSendLevelToDelay:
									case DelayMacro:
									case DelayPreLpf:
									case DelayTimeCenter:
									case DelayTimeRatioLeft:
									case DelayTimeRatioRight:
									case DelayLevelCenter:
									case DelayLevelLeft:
									case DelayLevelRight:
									case DelayLevel:
									case DelayFeedback:
									case DelaySendLevelToReverb:
									case EQLowFreq:
									case EQLowGain:
									case EQHighFreq:
									case EQHighGain:
										// placeholder
										status = Unknown1;
										p1 = ev[readHead++];
										sum += p1;
										checksum = ev[readHead++];
										break;
									}

									break;
								}
							}

							case 0:
								switch (modeSet) {
								case MIDISetup:
									status = SystemReset;
									checksum = ev[(readHead++) + varLen];
									Synth->Reset(0x01);
									break;
								}
								break;
							}

						case 0x45:
							switch (addr) {
								// Display data
							case DisplayData:
							{
								char mult = 0;

								commandPart = ev[readHead + (7 * mult)];
								char dataType = ev[readHead + (10 * mult)];

								switch (dataType) {
								case ASCIIMode:
									for (/* damn son */; mult < 32; mult++) {
										if (commandPart > 0x1F)
											break;

										commandPart = ev[readHead + (7 * mult)];
									}

									if (char* asciiStream = new char[mult]) {
										for (int i = 0; i < mult; i++) {
											asciiStream[i] = ev[i + (13 * i)];
										}

										LOG(SHErr, "Roland Display Data: %s", asciiStream);

										delete[] asciiStream;
									}
									break;

								case BitmapMode:
									// TODO
									break;
								}

								break;
							}
							}
						}

						if (status) {
							if (varLen > 1)
								sum += varLen;

							if (sum > 0x7F)
								sum = sum % ChecksumDividend;

							calcChecksum = ChecksumDividend - sum;

							if (calcChecksum != checksum) {
								LOG(SHErr, "Checksum invalid! Expected 0x%X, but got 0x%X.", checksum, calcChecksum);
								return InvalidBuffer;
							}

							LOG(SHErr, "Processed SysEx! (Block 0x%X, SynthPart 0x%X, CommandPart 0x%X)", addrBlock, synthPart, commandPart);
							Synth->PlayShortEvent(status, p1, p2);
							break;
						}

						LOG(SHErr, "Received unsupported SysEx. (Block 0x%X, SynthPart 0x%X, CommandPart 0x%X)", addrBlock, synthPart, commandPart);
						return NotSupported;
					}
					else if (command == Send) {
						// TODO
						return NotSupported;
					}
					else return NotSupported;

					break;
				}

				default:
					notSpecial = true;
					readHead += 5;
					break;
				}

				if (notSpecial)
					continue;

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