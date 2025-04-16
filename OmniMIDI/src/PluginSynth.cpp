/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#include "PluginSynth.hpp"

OmniMIDI::PluginSynth::PluginSynth(const char* pluginName, ErrorSystem::Logger* PErr) : SynthModule(PErr) {
    if (!pluginName)
        throw;

    Plugin = new Lib(pluginName, LIBSUFF, ErrLog);
}

bool OmniMIDI::PluginSynth::IsPluginSupported() {
    if (Plugin != nullptr && (Plugin->IsOnline() && _PluginFuncs != nullptr)) {
        return Plugin->IsSupported(_PluginFuncs->SupportedAPIVer(), OMV2_PLGVER);
    }
    else Error("IsPluginSupported() returned false because the plugin is not loaded.", true);

    return false;
}

bool OmniMIDI::PluginSynth::LoadSynthModule() {
    auto pluginName = Plugin->GetName();

    Message("PluginSynth >> %s", pluginName);

    if (!Plugin->LoadLib()) return false;
    else Message("Loaded plugin %s", pluginName);

    
    _PluginEntryPoint = (OMv2PEP)getAddr(Plugin->GetPtr(), OMV2_ENTRY);
    if (_PluginEntryPoint == nullptr) return false;
    else Message("%s >> Found %s at address 0x%x", pluginName, OMV2_ENTRY, _PluginEntryPoint);

    _PluginFuncs = _PluginEntryPoint();
    if (_PluginFuncs == nullptr) return false;
    else Message("%s >> Got pointer to _PluginFuncs struct at address 0x%x", pluginName, _PluginFuncs);

    if (IsPluginSupported()) {
        if (!_PluginFuncs->InitPlugin)
            Error("Excuse me", true);

        return true;
    }

    return false;
}

bool OmniMIDI::PluginSynth::StartSynthModule() {
    Message("Starting synth plugin...");

    if (_PluginFuncs == nullptr) {
        Message("_PluginFuncs is null!");
        return false;
    }

    if (!_PluginFuncs->InitPlugin()) {
        Message("->InitPlugin() failed!");
        return false;
    }

    return true;
}

bool OmniMIDI::PluginSynth::StopSynthModule() {
    Message("Freeing synth plugin...");

    if (_PluginFuncs == nullptr){
        Message("_PluginFuncs is null!");
        return false;
    }

    if (!_PluginFuncs->StopPlugin()){
        Message("->StopPlugin() failed!");
        return false;
    }

    return true;
}

bool OmniMIDI::PluginSynth::UnloadSynthModule() {
    Message("Unloading synth plugin...");

    if (_PluginFuncs != nullptr) {
        Message("_PluginFuncs set to nullptr");
        _PluginFuncs = nullptr;
    }

    if (Plugin != nullptr && Plugin->IsOnline()) {
        if (!Plugin->UnloadLib()) {
            Fatal("->UnloadLib() failed! BAD!");
        }

        delete Plugin;
        Message("Deleted Plugin object.");
    }

    return true;
}