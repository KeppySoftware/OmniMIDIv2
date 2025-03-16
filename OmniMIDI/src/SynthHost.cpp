/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#include "SynthHost.hpp"

#ifdef _WIN32 
bool OmniMIDI::SynthHost::SpInit(SynthModule* synthModule) {
	if (StreamPlayer != nullptr)
		SpFree();

	if (synthModule == nullptr)
		synthModule = Synth;

	StreamPlayer = new OmniMIDI::CookedPlayer(synthModule, DrvCallback, ErrLog);
	Message("StreamPlayer allocated.");

	if (!StreamPlayer)
	{
		SpFree();
		return false;
	}

	Message("StreamPlayer address: %x", StreamPlayer);
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
	char* confPath = _SHSettings->GetConfigPath();

	if (confPath) {
		auto curChkTime = std::filesystem::last_write_time(confPath);
		auto lastChkTime = curChkTime;

		while (!Synth->IsSynthInitialized());

		Message("Monitoring config at \"%s\" for changes...", confPath);
		while (Synth->IsSynthInitialized()) {
			curChkTime = std::filesystem::last_write_time(confPath);

			if (lastChkTime != curChkTime) {	
				Message("Config changed, applied changes...");

				if (Stop(true)) {
					if (Start()) {
						confPath = _SHSettings->GetConfigPath();
						curChkTime = std::filesystem::last_write_time(confPath);
						lastChkTime = curChkTime;
					}
					else Error("Something went terribly wrong while reloading the settings!", true);

					Message("Config applied!");
				}
			}

			if (Synth->SynthID() == EMPTYMODULE)
				break;

			Utils.MicroSleep(-1);
		}
	}
}

void OmniMIDI::SynthHost::RefreshSettings() {
	Message("Refreshing synth host settings...");

	auto tSHSettings = new OmniMIDI::HostSettings(ErrLog);
	auto oSHSettings = _SHSettings;

	_SHSettings = tSHSettings;
	delete oSHSettings;
	oSHSettings = nullptr;

	Message("Settings refreshed! (renderer %d, kdmapi %d, crender %s)", _SHSettings->GetRenderer(), _SHSettings->IsKDMAPIEnabled(), _SHSettings->GetCustomRenderer());
}

bool OmniMIDI::SynthHost::Start(bool StreamPlayer) {
	RefreshSettings();

	auto newSynth = GetSynth();
	auto oldSynth = Synth;

	if (newSynth) {
		if (newSynth->LoadSynthModule()) {
#ifdef _WIN32 
			Synth->SetInstance(hwndMod);
#endif

			if (newSynth->StartSynthModule()) {
#ifdef _WIN32 
				if (StreamPlayer) {
					if (!SpInit(newSynth))
						return false;
				}
#endif

				if (!_HealthThread.joinable()) 
					_HealthThread = std::jthread(&SynthHost::HostHealthCheck, this);

				if (_HealthThread.joinable()) {			
					Synth = newSynth;
					delete oldSynth;
					return true;
				}
				else Error("_HealthThread failed. (ID: %x)", true, _HealthThread.get_id());

				if (!newSynth->StopSynthModule())
					Fatal("StopSynthModule() failed!!!");
			}
			else Error("StartSynthModule() failed! The driver will not output audio.", true);
		}
		else Error("LoadSynthModule() failed! The driver will not output audio.", true);
	}

	if (!newSynth->UnloadSynthModule())
		Fatal("UnloadSynthModule() failed!!!");

	delete newSynth;

	return false;
}

bool OmniMIDI::SynthHost::Stop(bool restart) {
	auto dummySynth = new SynthModule(ErrLog);
	auto deadSynth = Synth;

	Synth = dummySynth;

#ifdef _WIN32 
	SpFree();
#endif

	if (!deadSynth->StopSynthModule()) 
		Fatal("StopSynthModule() failed!!!");
	else
		Message("StopSynthModule() ok.");

	if (!deadSynth->UnloadSynthModule())
		Fatal("UnloadSynthModule() failed!!!");
	else
		Message("UnloadSynthModule() ok.");

	delete deadSynth;
	Message("Deleted synth from memory.");

	if (!restart) {
#ifdef _WIN32 
		if (DrvCallback->IsCallbackReady()) {
			DrvCallback->CallbackFunction(MOM_CLOSE, 0, 0);
			DrvCallback->ClearCallbackFunction();
			Message("Callback system has been freed.");
		}
#endif

		if (_HealthThread.joinable())
			_HealthThread.join();
	}

	return true;
}

OmniMIDI::SynthModule* OmniMIDI::SynthHost::GetSynth() {
	SynthModule* newSynth = nullptr;

	char r = _SHSettings->GetRenderer();
	switch (r) {
	case Synthesizers::External:
		extModule = loadLib(_SHSettings->GetCustomRenderer());

		if (extModule) {
			auto iM = reinterpret_cast<rInitModule>(getAddr(extModule, "initModule"));

			if (iM) {
				newSynth = iM();

				if (newSynth) {
					Message("R%d (EXTERNAL >> %s)",
						r,
						_SHSettings->GetCustomRenderer());
					break;
				}
			}
		}

		newSynth = new OmniMIDI::SynthModule(ErrLog);
		Error("The requested external module (%s) could not be loaded.", _SHSettings->GetCustomRenderer());
		break;

	case Synthesizers::FluidSynth:
		newSynth = new OmniMIDI::FluidSynth(ErrLog);
		Message("R%d (FLUIDSYNTH)", r);
		break;


	case Synthesizers::BASSMIDI:
#if defined(_NONFREE)
		newSynth = new OmniMIDI::BASSSynth(ErrLog);
		Message("R%d (BASSMIDI)", r);
#else
		Error("This version of OmniMIDI has been compiled without the _NONFREE preprocessor directive. BASSMIDI will not be available.", true);
#endif
		break;

	case Synthesizers::XSynth:
		newSynth = new OmniMIDI::XSynth(ErrLog);
		Message("R%d (XSYNTH)", r);
		break;

#if defined(WIN32)
	case Synthesizers::ShakraPipe:
		newSynth = new OmniMIDI::ShakraPipe(ErrLog);
		Message("R%d (SHAKRA)", r);
		break;
#endif

	default:
		break;
	}

	if (newSynth == nullptr) {
		newSynth = new OmniMIDI::SynthModule(ErrLog);
		Error("The chosen synthesizer (Syn%d) is not available on this platform, or does not exist.", false, r);
	}

	return newSynth;
}

float OmniMIDI::SynthHost::GetRenderingTime() {
	return Synth->GetRenderingTime();
}

unsigned int OmniMIDI::SynthHost::GetActiveVoices() {
	return Synth->GetActiveVoices();
}

void OmniMIDI::SynthHost::PlayShortEvent(unsigned int ev) {
	Synth->PlayShortEvent(ev);
}

void OmniMIDI::SynthHost::PlayShortEvent(unsigned char status, unsigned char param1, unsigned char param2) {
	Synth->PlayShortEvent(status, param1, param2);
}

OmniMIDI::SynthResult OmniMIDI::SynthHost::PlayLongEvent(char* ev, unsigned int size) {
	if (!Synth->IsSynthInitialized())
		return NotInitialized;

	if (!ev || size < 4  || size > MAX_MIDIHDR_BUF || (unsigned char)ev[size - 1] != 0xF7)
		return InvalidBuffer;

	for (int readHead = 0, n = size - 1; readHead < n; ) {
		switch (ev[readHead] & 0xF0) {
		case SystemMessageStart:
		{
			bool notSpecial = false;
			unsigned char pos = 0;
			unsigned char vendor = ev[readHead + 1] & 0xFF;

			readHead += 2;
			if (vendor < 0x80) {
				switch (vendor) {
					// Universal
				case 0x7F:
				{
					char command = ev[readHead + 3];

					switch (command) {
					case 0x06: {
#ifdef _WIN32
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

						break;
#else
						return NotSupported;
#endif
					}

					case 0x7F:
						readHead += 5;
						break;

					default:
						return NotSupported;
					}

					return Ok;
				}

				// Roland
				case 0x41:
				{
					// Size of 3
					unsigned char devid = ev[readHead];
					unsigned char modid = ev[readHead + 1];
					unsigned char command = ev[readHead + 2];

					if (devid > PartMax)
						return InvalidBuffer;

					readHead += 3;
					if (command == Receive) {
						PSE params = new SE[256];
						unsigned char pseWriteHead = -1;

						unsigned int varLen = 1;

						unsigned char addrBlock = ev[readHead];
						unsigned char synthPart = ev[readHead + 1];
						unsigned char commandPart = ev[readHead + 2];
						unsigned char status = 0;

						unsigned int lastPos = 0;
						unsigned int addr = (addrBlock << 16) | (synthPart << 8) | commandPart;
						unsigned int modeSet = addr & 0xFFFF;

						unsigned char sum = addrBlock + synthPart + commandPart;
						unsigned char checksum = 0;
						unsigned char calcChecksum = 0;

						bool error = false;
						bool noParams = false;

						readHead += 3;
						while (addrBlock != SystemMessageEnd) {
							if (addr == MIDIReset) {
								delete[] params;

								Message("Detected 0x%X! MIDI reset triggered.", MIDIReset);
								char resetType = ev[readHead + 2];
								return Synth->Reset(!resetType ? resetType : vendor);
							}

							switch (modid) {
							case 0x42:
								switch (addr & BlockDiscrim) {
								case PatchCommonA:
								case PatchCommonB: {
									switch (synthPart & 0xF0) {
									case 0:
									{
										switch (commandPart) {
										case 0:
										case 1:
										case 2:
										case 3: {
											varLen = 4;
											pseWriteHead++;

											if ((commandPart & 0xF) == 0)
												lastPos = readHead;

											params[pseWriteHead].status = MasterTune;
											for (unsigned int i = 0; i < varLen; i++) {
												if (i == 1)
													params[pseWriteHead].param1 = ev[lastPos + i];

												if (i == 2)
													params[pseWriteHead].param2 = ev[lastPos + i];
											}

											readHead += varLen + 1;
											sum += params[pseWriteHead].param1 + params[pseWriteHead].param2;

											readHead++;
											break;
										}

										case 4:
											pseWriteHead++;
											params[pseWriteHead].status = MasterVolume;
											params[pseWriteHead].param1 = ev[readHead++];
											sum += params[pseWriteHead].param1;
											status = params[pseWriteHead].status;
											readHead++;
											break;

										case 5:
											pseWriteHead++;
											params[pseWriteHead].status = MasterKey;
											params[pseWriteHead].param1 = ev[readHead++];
											sum += params[pseWriteHead].param1;
											status = params[pseWriteHead].status;
											readHead++;
											break;

										case 6:
											pseWriteHead++;
											params[pseWriteHead].status = MasterPan;
											params[pseWriteHead].param1 = ev[readHead++];
											sum += params[pseWriteHead].param1;
											status = params[pseWriteHead].status;
											readHead++;
											break;
										}

										break;
									}

									case 10:
										switch (commandPart & 0xF0) {
										// Scale tuning
										case 0x40:
										{
											varLen = 0x0C;

											for (unsigned int i = 0; i < varLen; i++) {
												char tmp = ev[readHead + (4 * i)];
												if ((tmp & 0xF0) != 0x40)
													break;

												pseWriteHead++;
												params[pseWriteHead].status = RolandScaleTuning;
												params[pseWriteHead].param1 = tmp;
												sum += tmp;
											}

											readHead += varLen + 1;
											break;
										}
										}
										break;

									default:
										switch (modeSet) {
										case PatchName:
											varLen = 16;
											readHead += varLen + 1;
											break;

										case ReverbMacro:
											status = RolandReverbMacro;
											goto var1param;

										case ReverbLevel:
											status = RolandReverbLevel;
											goto var1param;

										case ReverbTime:
											status = RolandReverbTime;
											goto var1param;

										case ReverbDelayFeedback:
											status = RolandReverbDelay;
											goto var1param;

										case ChorusMacro:
											status = RolandChorusMacro;
											goto var1param;

										case ChorusLevel:
											status = RolandChorusLevel;
											goto var1param;

										case ChorusFeedback:
											status = RolandChorusFeedback;
											goto var1param;

										case ChorusDelay:
											status = RolandChorusDelay;
											goto var1param;

										case ChorusRate:
											status = RolandChorusRate;
											goto var1param;

										case ChorusDepth:
											status = RolandChorusDepth;
											goto var1param;

										case ReverbPredelayTime:
										case ReverbCharacter:
										case ReverbPreLpf:
										case ChorusPreLpf:
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
										var1param:
											pseWriteHead++;
											params[pseWriteHead].status = status;
											params[pseWriteHead].param1 = ev[readHead++];
											sum += params[pseWriteHead].param1;
											readHead++;
											break;
										}

										break;
									}
								}

								case 0:
									switch (modeSet) {
									case MIDISetup:
										params[pseWriteHead++].status = SystemReset;
										params[pseWriteHead].param1 = 0x01;
										readHead += varLen + 1;
										break;
									}
									break;
								}

							case 0x45:
								switch (addr) {
								case DisplayData:
								{
									char mult = 0;

									commandPart = ev[readHead + 7];
									char dataType = ev[readHead + 10];

									for (mult = 0; mult < dataType; mult++) {
										if (commandPart > 0x1F)
											break;

										commandPart = ev[readHead + (7 * mult)];
									}

									checksum = ev[readHead + (12 * mult)];

									if (char* asciiStream = new char[mult]) {
										if (dataType == ASCIIMode) {
											for (int i = 0; i < mult; i++) {
												asciiStream[i] = ev[readHead + (11 * i)];
											}

											Message("MSG: % s", asciiStream);								
										}
										else if (dataType == BitmapMode) {
											for (int i = 0; i < mult; i++) {
												asciiStream[i] = ev[readHead + (11 * i)];
											}

											for (int i = 0; i < mult; i = i * 16) {

											}
											Message("BITMAP: % s", asciiStream);
										}

										delete[] asciiStream;
									}
									
									readHead += 12 * mult;
									break;
								}
								}
							}

							checksum = ev[readHead];
							if (pseWriteHead != 0 && !noParams) {
								if (varLen > 1)
									sum += varLen;

								if (sum > 0x7F)
									sum = sum % ChecksumDividend;

								calcChecksum = ChecksumDividend - sum;

								if (calcChecksum != checksum) {
									Message("Checksum invalid! Expected 0x%X, but got 0x%X.", checksum, calcChecksum);
									return InvalidBuffer;
								}

								Message("Processed SysEx! (Block 0x%X, SynthPart 0x%X, CommandPart 0x%X, Checksum 0x%X)", addrBlock, synthPart, commandPart, checksum);

								for (unsigned char i = 0; i < pseWriteHead; i++) {
									Synth->PlayShortEvent(params[i].status, params[i].param1, params[i].param2);
								}

								addrBlock = ev[readHead];
								synthPart = ev[readHead + 1];
								commandPart = ev[readHead + 2];

								addr = (addrBlock << 16) | (synthPart << 8) | commandPart;
								modeSet = addr & 0xFFFF;

								if (addrBlock == SystemMessageEnd ||
									synthPart == SystemMessageEnd ||
									commandPart == SystemMessageEnd)
									break;
							}
							else if (noParams) break;

							delete[] params;
							Message("Received unsupported SysEx. (Block 0x%X, SynthPart 0x%X, CommandPart 0x%X)", addrBlock, synthPart, commandPart);
							error = true;
						}
					
						delete[] params;

						if (error)
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
