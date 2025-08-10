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

#include "SynthModule.hpp"
#include <inttypes.h>
#include <iostream>

void OmniMIDI::SynthModule::StartDebugOutput() {
    if (_synthConfig->IsDebugMode()) {
        _LogThread = std::jthread(&SynthModule::LogFunc, this);
        if (!_LogThread.joinable()) {
            Error("_LogThread failed. (ID: %x)", true, _LogThread.get_id());
            return;
        } else
            Message("_LogThread running! (ID: %x)", _LogThread.get_id());

        _synthConfig->OpenConsole();
    }
}

void OmniMIDI::SynthModule::StopDebugOutput() {
    if (_synthConfig->IsDebugMode() && _synthConfig->IsOwnConsole())
        _synthConfig->CloseConsole();

    if (_LogThread.joinable()) {
        _LogThread.join();
        Message("_LogThread freed.");
    }
}

void OmniMIDI::SynthModule::LogFunc() {
    const char Templ[] = "R%06.2f%% >> P%" PRIu64 " (Ev%08zu/%08zu)";
    char *Buf = new char[96];

    while (!IsSynthInitialized())
        Utils.MicroSleep(SLEEPVAL(1));

    while (IsSynthInitialized()) {
        sprintf(Buf, Templ, GetRenderingTime(), GetActiveVoices(),
                ShortEvents->GetReadHeadPos(), ShortEvents->GetWriteHeadPos());
        SetTerminalTitle(Buf);

        Utils.MicroSleep(SLEEPVAL(1000));
    }

    sprintf(Buf, Templ, 0.0f, (uint64_t)0, (size_t)0, (size_t)0);
    SetTerminalTitle(Buf);

    delete[] Buf;
}

void OmniMIDI::SynthModule::FreeEvBuf(BEvBuf *target) {
    if (target) {
        auto tEvents = new BEvBuf;
        auto oEvents = target;

        target = tEvents;

        delete oEvents;
    }
}

OmniMIDI::BEvBuf *OmniMIDI::SynthModule::AllocateShortEvBuf(size_t size) {
    if (ShortEvents) {
        auto tEvents = new EvBuf(size);
        auto oEvents = ShortEvents;

        ShortEvents = tEvents;

        delete oEvents;
    }

    // Double check
    return ShortEvents;
}

OmniMIDI::BEvBuf *OmniMIDI::SynthModule::AllocateLongEvBuf(size_t size) {
    if (LongEvents) {
        auto tEvents = new LEvBuf(size);
        auto oEvents = LongEvents;

        LongEvents = tEvents;

        delete oEvents;
    }

    // Double check
    return LongEvents;
}

bool OmniMIDI::SettingsModule::InitConfig(bool write, const char *pSynthName,
                                          size_t pSynthName_sz) {
    char userProfile[MAX_PATH_LONG] = {0};
    SettingsPath = new char[MAX_PATH_LONG]{0};
    size_t szSetPath = sizeof(char) * MAX_PATH_LONG;

    if (JSONStream && JSONStream->is_open())
        CloseConfig();

    if (SynthName == nullptr && pSynthName != nullptr) {
        if (strcmp(pSynthName, DUMMY_STR)) {
            if (pSynthName_sz > SYNTHNAME_SZ)
                pSynthName_sz = SYNTHNAME_SZ;

            SynthName = new char[pSynthName_sz];
            if (strcpy(SynthName, pSynthName) != SynthName) {
                Error("An error has occurred while parsing SynthName!", true);
                CloseConfig();
                return false;
            }
        }
    } else {
        Error("InitConfig called with no SynthName specified!", true);
        CloseConfig();
        return false;
    }

    if (Utils.GetFolderPath(OMShared::FIDs::UserFolder, userProfile,
                            szSetPath)) {
        Message("Utils.GetFolderPath path: %s", userProfile);

        snprintf(SettingsPath, szSetPath, "%s/OmniMIDI/settings.json",
                 userProfile);
        Message("Configuration path: %s", SettingsPath);

        Utils.CreateFolder(SettingsPath, szSetPath);

        JSONStream->open((char *)SettingsPath,
                         write ? (std::fstream::out | std::fstream::trunc)
                               : std::fstream::in);
        if (JSONStream->is_open() &&
            nlohmann::json::accept(*JSONStream, true)) {
            JSONStream->clear();
            JSONStream->seekg(0, std::ios::beg);
            jsonptr = nlohmann::json::parse(*JSONStream, nullptr, false, true);
        }

        if (jsonptr == nullptr || jsonptr["OmniMIDI"] == nullptr) {
            Error("No configuration file found! A new reference one will be "
                  "created for you.",
                  false);

            jsonptr["OmniMIDI"] = {
                ConfGetVal(Renderer),
                ConfGetVal(CustomRenderer),
                ConfGetVal(KDMAPIEnabled),
                ConfGetVal(DebugMode),
                ConfGetVal(Renderer),
                {"SynthModules", nlohmann::json::object({})}};

            WriteConfig();
        }

        mainptr = jsonptr["OmniMIDI"];
        if (mainptr == nullptr) {
            Error("An error has occurred while parsing the settings for "
                  "OmniMIDI!",
                  true);
            CloseConfig();
            return false;
        }

        MainSetVal(int, Renderer);
        MainSetVal(bool, DebugMode);
        MainSetVal(bool, KDMAPIEnabled);
        MainSetVal(std::string, CustomRenderer);

        if (SynthName) {
            synptr = mainptr["SynthModules"][SynthName];
            if (synptr == nullptr) {
                Error(
                    "No configuration settings found for the selected renderer "
                    "\"%s\"! A new reference one will be created for you.",
                    false, SynthName);
                return false;
            }
        }

        return true;
    } else
        Error("An error has occurred while parsing the user profile path!",
              true);

    return false;
}

bool OmniMIDI::SettingsModule::WriteConfig() {
    JSONStream->close();

    JSONStream->open((char *)SettingsPath,
                     std::fstream::out | std::fstream::trunc);
    std::string dump = jsonptr.dump(4);
    JSONStream->write(dump.c_str(), dump.length());
    JSONStream->close();

    return true;
}

bool OmniMIDI::SettingsModule::ReloadConfig() {
    JSONStream->close();
    return InitConfig();
}

void OmniMIDI::SettingsModule::CloseConfig() {
    if (jsonptr != nullptr && !jsonptr.is_null()) {
        jsonptr.clear();
    }

    if (mainptr != nullptr && !mainptr.is_null()) {
        mainptr.clear();
    }

    if (synptr != nullptr && !synptr.is_null()) {
        synptr.clear();
    }

    if (JSONStream->is_open())
        JSONStream->close();

    if (SettingsPath != nullptr) {
        delete SettingsPath;
        SettingsPath = nullptr;
    }

    if (SynthName != nullptr) {
        delete SynthName;
        SynthName = nullptr;
    }
}

void OmniMIDI::SettingsModule::OpenConsole() {
#if defined _WIN32 && !defined _DEBUG
    if (IsDebugMode()) {
        if (AllocConsole()) {
            OwnConsole = true;
            FILE *dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            freopen_s(&dummy, "CONOUT$", "w", stderr);
            freopen_s(&dummy, "CONIN$", "r", stdin);
            std::cout.clear();
            std::clog.clear();
            std::cerr.clear();
            std::cin.clear();
        }
    }
#endif
}

void OmniMIDI::SettingsModule::CloseConsole() {
#if defined _WIN32 && !defined _DEBUG
    if (IsDebugMode() && OwnConsole) {
        FreeConsole();
        OwnConsole = false;
    }
#endif
}

bool OmniMIDI::SettingsModule::AppendToConfig(nlohmann::json &content) {
    if (content == nullptr)
        return false;

    if (jsonptr["OmniMIDI"]["SynthModules"][SynthName] != nullptr) {
        jsonptr["OmniMIDI"]["SynthModules"].erase(SynthName);
    }

    jsonptr["OmniMIDI"]["SynthModules"][SynthName] = content;
    mainptr = jsonptr["OmniMIDI"];
    synptr = mainptr["SynthModules"][SynthName];

    return true;
}

void OmniMIDI::SoundFontSystem::SoundFontThread() {
    while (StayAwake) {
        if (ListPath) {
            auto ftime = std::filesystem::last_write_time(ListPath);

            if (ListLastEdit != ftime) {
                ListLastEdit = ftime;

                if (SynthModule != nullptr)
                    SynthModule->LoadSoundFonts();

                Message("SoundFonts loaded.");
            }
        }

        Utils.MicroSleep(SLEEPVAL(1000000));
    }
}

bool OmniMIDI::SoundFontSystem::RegisterCallback(OmniMIDI::SynthModule *ptr) {
    if (ptr != nullptr) {
        SynthModule = ptr;
        StayAwake = true;

        _SFThread = std::jthread(&SoundFontSystem::SoundFontThread, this);
        if (!_SFThread.joinable()) {
            Error("_SFThread failed. (ID: %x)", true, _SFThread.get_id());
            return false;
        }

        Message("SoundFont autoloader running.");
        return true;
    }

    StayAwake = false;
    if (_SFThread.joinable())
        _SFThread.join();

    SynthModule = nullptr;
    Message("Disposed of SoundFont autoloader.");
    return true;
}

std::vector<OmniMIDI::SoundFont> *
OmniMIDI::SoundFontSystem::LoadList(std::string list) {
    char tmpUtils[MAX_PATH_LONG] = {0};
    char OMPath[MAX_PATH_LONG] = {0};

    char *listPath = nullptr;
    size_t szListPath = sizeof(char) * MAX_PATH_LONG;

    const char *soundfontPath = nullptr;

    if (SoundFonts.size() > 0)
        return &SoundFonts;

    if (Utils.GetFolderPath(OMShared::FIDs::UserFolder, tmpUtils, szListPath)) {
        if (list.empty()) {
            snprintf(OMPath, szListPath, "%s/OmniMIDI/lists/OmniMIDI_A.json",
                     tmpUtils);
            listPath = OMPath;
        } else
            listPath = (char *)list.c_str();

        Utils.CreateFolder(listPath, szListPath);

        std::fstream sfs;
        sfs.open(listPath, std::fstream::in);

        if (sfs.is_open()) {
            try {
                // Read the JSON data from there
                auto json = nlohmann::json::parse(sfs, nullptr, false, true);

                if (json != nullptr) {
                    auto &JsonData = json["SoundFonts"];

                    if (!(JsonData == nullptr || JsonData.size() < 1)) {
                        SoundFonts.clear();

                        for (size_t i = 0; i < JsonData.size(); i++) {
                            SoundFont SF;
                            nlohmann::json subitem = JsonData[i];

                            // Is item valid?
                            if (subitem != nullptr) {
                                SF.path = subitem["path"].is_null()
                                              ? "\0"
                                              : subitem["path"];
                                SF.enabled = subitem["enabled"].is_null()
                                                 ? SF.enabled
                                                 : (bool)subitem["enabled"];

                                if (SF.path.empty())
                                    continue;

                                soundfontPath = SF.path.c_str();

                                if (Utils.DoesFileExist(SF.path)) {
                                    SF.xgdrums = subitem["xgdrums"].is_null()
                                                     ? SF.xgdrums
                                                     : (bool)subitem["xgdrums"];
                                    SF.linattmod =
                                        subitem["linattmod"].is_null()
                                            ? SF.linattmod
                                            : (bool)subitem["linattmod"];
                                    SF.lindecvol =
                                        subitem["lindecvol"].is_null()
                                            ? SF.lindecvol
                                            : (bool)subitem["lindecvol"];
                                    SF.nofx = subitem["nofx"].is_null()
                                                  ? SF.minfx
                                                  : (bool)subitem["nofx"];
                                    SF.minfx = subitem["minfx"].is_null()
                                                   ? SF.minfx
                                                   : (bool)subitem["minfx"];
                                    SF.nolimits =
                                        subitem["nolimits"].is_null()
                                            ? SF.nolimits
                                            : (bool)subitem["nolimits"];
                                    SF.norampin =
                                        subitem["norampin"].is_null()
                                            ? SF.norampin
                                            : (bool)subitem["norampin"];

                                    SF.spreset = subitem["spreset"].is_null()
                                                     ? SF.spreset
                                                     : (int)subitem["spreset"];
                                    SF.sbank = subitem["sbank"].is_null()
                                                   ? SF.sbank
                                                   : (int)subitem["sbank"];
                                    SF.dpreset = subitem["dpreset"].is_null()
                                                     ? SF.dpreset
                                                     : (int)subitem["dpreset"];
                                    SF.dbank = subitem["dbank"].is_null()
                                                   ? SF.dbank
                                                   : (int)subitem["dbank"];
                                    SF.dbanklsb =
                                        subitem["dbanklsb"].is_null()
                                            ? SF.dbanklsb
                                            : (int)subitem["dbanklsb"];

                                    SoundFonts.push_back(SF);
                                } else
                                    Error("The SoundFont \"%s\" could not be "
                                          "found!",
                                          false, soundfontPath);
                            }

                            // If it's not, then let's loop until the end of the
                            // JSON struct
                        }

                        ListPath = new char[szListPath];

                        if (ListPath)
                            strncpy(ListPath, listPath, szListPath);
                    } else
                        Error("\"%s\" does not contain a valid \"SoundFonts\" "
                              "JSON structure.",
                              false, listPath);
                } else
                    Error("Invalid JSON structure!", false);
            } catch (const nlohmann::json::parse_error &ex) {
                Error("An error has occurred while parsing the SoundFont JSON. "
                      "%s.",
                      true, ex.what());
            } catch (const nlohmann::json::type_error &ex) {
                // Common error, just return nullptr and we'll retry
                Message("The SoundFont JSON contains a value whose type does "
                        "not match the expected semantics. %s.",
                        ex.what());
            } catch (const nlohmann::json::other_error &ex) {
                Error("An unknown error has occurred while reading the "
                      "SoundFont JSON. %s.",
                      true, ex.what());
            }

            sfs.close();

            return (SoundFonts.size() > 0) ? &SoundFonts : nullptr;
        } else {
            Error("SoundFonts JSON at path \"%s\" does not exist. The driver "
                  "will create an example one for you to edit.",
                  false, listPath);

            SoundFont eSF;
            auto example = eSF.GetExampleList();

            sfs.close();
            sfs.open((char *)OMPath, std::fstream::out | std::fstream::trunc);
            std::string dump = example.dump(4);
            sfs.write(dump.c_str(), dump.length());
            sfs.close();
        }
    }

    return nullptr;
}

bool OmniMIDI::SoundFontSystem::ClearList() {
    if (SoundFonts.size() > 0)
        SoundFonts.clear();

    if (ListPath) {
        delete[] ListPath;
        ListPath = nullptr;
    }

    return true;
}