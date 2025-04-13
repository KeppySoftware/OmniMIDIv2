/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#include "PluginSynth.hpp"

OmniMIDI::PluginSynth::PluginSynth(const char* pluginName, ErrorSystem::Logger* PErr) : SynthModule(PErr) {
    if (!pluginName)
        throw;

    Plugin = new Lib(pluginName, nullptr, ErrLog);
}

bool OmniMIDI::PluginSynth::LoadSynthModule() {       
    if (!Plugin->LoadLib())
        return false;

    _PluginEntryPoint = (OMv2PEP)getAddr(Plugin->Ptr(), OMV2_ENTRY);
    if (_PluginEntryPoint == nullptr)
        return false;

    _PluginFuncs = _PluginEntryPoint();
    if (_PluginFuncs == nullptr)
        return false;

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
    }

    return true;
}