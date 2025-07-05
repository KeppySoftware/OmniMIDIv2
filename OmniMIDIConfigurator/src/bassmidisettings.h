#ifndef BASSMIDISETTINGS_H
#define BASSMIDISETTINGS_H

#include <QWidget>
#include "bassconfig.h"
#include "synthsettings.h"

namespace Ui {
class BASSMIDISettings;
}

class BASSMIDISettings : public QWidget, public SynthSettings
{
    Q_OBJECT

public:
    explicit BASSMIDISettings(QWidget *parent = nullptr, BASSConfig *config = nullptr);
    ~BASSMIDISettings();
    void showWinAudioSettings(int idx);
    void loadSettings() override;
    void storeSettings() override;
    QWidget *getWidget() override;

#ifdef _WIN32
    void extracted(std::vector<std::string> &list);
    void reloadASIODevices();
#endif

private:
    Ui::BASSMIDISettings *ui;
    BASSConfig *m_cfg = nullptr;
};

#endif // BASSMIDISETTINGS_H
