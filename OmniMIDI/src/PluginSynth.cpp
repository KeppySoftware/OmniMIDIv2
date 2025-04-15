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

    if (!_PluginFuncs->InitPlugin)
        Error("Excuse me", true);

    return true;
}

bool OmniMIDI::PluginSynth::StartSynthModule() {
    if (_PluginFuncs == nullptr)
        return false;

    if (!_PluginFuncs->InitPlugin())
        return false;

    return true;
}

bool OmniMIDI::PluginSynth::StopSynthModule() {
    if (_PluginFuncs == nullptr)
        return true;

    if (!_PluginFuncs->StopPlugin())
        return false;

    return true;
}

bool OmniMIDI::PluginSynth::UnloadSynthModule() {
    if (_PluginFuncs != nullptr) {
        _PluginFuncs = nullptr;
    }

    if (Plugin != nullptr && Plugin->IsOnline()) {
        if (!Plugin->UnloadLib())
            throw;

        delete Plugin;
    }

    return true;
}