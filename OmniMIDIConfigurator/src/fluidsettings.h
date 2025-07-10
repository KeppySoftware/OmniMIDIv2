#ifndef FLUIDSETTINGS_H
#define FLUIDSETTINGS_H

#include <QWidget>
#include "fluidconfig.h"
#include "synthsettings.h"

namespace Ui {
class FluidSettings;
}

const std::map<std::string, std::string> audioDrivers  = {
#if defined(__linux__)
    {"pipewire", "PipeWire"},
    {"alsa", "ALSA"},
    {"oss", "OSS"},
#elif defined(_WIN32)
    {"wasapi", "WASAPI"},
    {"dsound", "DirectSound"},
    {"waveout", "WaveOut"},
#elif defined(__APPLE__)
    {"coreaudio", "CoreAudio"},
#endif
    {"jack", "JACK"},
    {"pulseaudio", "PulseAudio"},
    {"portaudio", "PortAudio"},
    {"file", "Output to file"}
};

class FluidSettings : public QWidget, public SynthSettings
{
    Q_OBJECT

public:
    explicit FluidSettings(QWidget *parent = nullptr, FluidConfig *config = nullptr);
    ~FluidSettings();
    void loadSettings() override;
    void storeSettings() override;
    QWidget *getWidget() override;

private:
    Ui::FluidSettings *ui;
    FluidConfig *m_cfg = nullptr;
    std::map<std::string, int> driverIndexes;
};

#endif // FLUIDSETTINGS_H
