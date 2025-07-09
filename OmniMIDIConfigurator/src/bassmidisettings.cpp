#include "bassmidisettings.h"
#include "./ui_bassmidisettings.h"
#include "utils.h"
#include <QMessageBox>

BASSMIDISettings::BASSMIDISettings(QWidget *parent, BASSConfig *config)
    : QWidget(parent)
    , ui(new Ui::BASSMIDISettings)
    , m_cfg(config)
{
    ui->setupUi(this);
#if defined(__linux__)
    ui->windowsAudio->setVisible(false);
    ui->windowsAudio->hide();
#elif defined(_WIN32)
    ui->linuxAudio->setVisible(false);
    ui->linuxAudio->hide();
    connect(ui->audioEngine, &QComboBox::currentIndexChanged, this, &BASSMIDISettings::showWinAudioSettings);
    connect(ui->asioReload, &QPushButton::pressed, this, &BASSMIDISettings::reloadASIODevices);
    connect(ui->asioOpenCfg, &QPushButton::pressed, this, &BASSMIDISettings::openASIOConfig);

    reloadASIODevices();
#endif
}

#ifdef _WIN32
void BASSMIDISettings::reloadASIODevices() {
    try {
        ui->asioDevice->setEnabled(true);
        auto list = Utils::getASIODevices();
        ui->asioDevice->clear();
        for (std::string dev : list) {
            QString name = QString::fromStdString(dev);
            ui->asioDevice->addItem(name, name);
        }
    } catch (const std::exception &e) {
        ui->asioDevice->addItem("N/A", "N/A");
        ui->asioDevice->setEnabled(false);
        QString s = "An error occured while loading the available ASIO devices:\n";
        QMessageBox::warning(this, WARNING_TITLE, s + e.what());
    }
}

void BASSMIDISettings::openASIOConfig() {
    try {
        if (ui->asioDevice->currentData() != NULL)
            Utils::openASIOConfig(ui->asioDevice->currentData().toString().toStdString());
    } catch (const std::exception &e) {
        QString s = "An error occured while trying to open the control panel:\n";
        QMessageBox::warning(this, WARNING_TITLE, s + e.what());
    }
}
#endif

void BASSMIDISettings::showWinAudioSettings(int idx) {
    if (idx == 2) {
        ui->asio->setVisible(true);
        ui->asio->show();
    } else {
        ui->asio->setVisible(false);
        ui->asio->hide();
    }
}

void BASSMIDISettings::loadSettings() {
    ui->evBufSize->setValue(m_cfg->EvBufSize);
    ui->renderTimeLimit->setValue(m_cfg->RenderTimeLimit);
    ui->sampleRate->setValue(m_cfg->SampleRate);
    ui->globalVoiceLimit->setValue(m_cfg->VoiceLimit);
    ui->followOverlaps->setCheckState(CCS(m_cfg->FollowOverlaps));
    ui->audioLimiter->setCheckState(CCS(m_cfg->LoudMax));
    ui->asyncMode->setCheckState(CCS(m_cfg->AsyncMode));
    ui->monoRendering->setCheckState(CCS(m_cfg->MonoRendering));
    ui->oneThreadMode->setCheckState(CCS(m_cfg->OneThreadMode));
    ui->disableEffects->setCheckState(CCS(m_cfg->DisableEffects));
    ui->mtKbDivs->setValue(m_cfg->ExpMTKeyboardDiv);
    ui->mtVoiceLimit->setValue(m_cfg->ExpMTVoiceLimit);
    ui->multithreading->setChecked(m_cfg->ExperimentalMultiThreaded);
    ui->bitDepth->setCurrentIndex(m_cfg->FloatRendering ? 0 : 1);
#if defined(__linux__)
    ui->audioBuffer->setValue(m_cfg->AudioBuf);
    ui->bufPeriod->setValue(m_cfg->BufPeriod);
#elif defined(_WIN32)
    ui->audioEngine->setCurrentIndex(m_cfg->AudioEngine);
    showWinAudioSettings(m_cfg->AudioEngine);
    if (m_cfg->AudioEngine == 1) // WASAPI
        ui->audioBuffer->setValue(m_cfg->WASAPIBuf);
    else // Internal
        ui->audioBuffer->setValue(m_cfg->AudioBuf);
    ui->streamDirectFeed->setCheckState(CCS(m_cfg->StreamDirectFeed));
    ui->asioChunksDivision->setValue(m_cfg->ASIOChunksDivision);
    ui->asioLCh->setValue(std::stoi(m_cfg->ASIOLCh));
    ui->asioRCh->setValue(std::stoi(m_cfg->ASIORCh));
    int asioIdx = ui->asioDevice->findData(QString::fromStdString(m_cfg->ASIODevice));
    if (asioIdx != -1) {
        ui->asioDevice->setCurrentIndex(asioIdx);
    } else {
        ui->asioDevice->setCurrentIndex(0);
        storeSettings();
    }
#endif
}

void BASSMIDISettings::storeSettings() {
    m_cfg->EvBufSize = ui->evBufSize->value();
    m_cfg->RenderTimeLimit = ui->renderTimeLimit->value();
    m_cfg->SampleRate = ui->sampleRate->value();
    m_cfg->VoiceLimit = ui->globalVoiceLimit->value();
    m_cfg->FollowOverlaps = ui->followOverlaps->isChecked();
    m_cfg->LoudMax = ui->audioLimiter->isChecked();
    m_cfg->AsyncMode = ui->asyncMode->isChecked();
    m_cfg->MonoRendering = ui->monoRendering->isChecked();
    m_cfg->OneThreadMode = ui->oneThreadMode->isChecked();
    m_cfg->DisableEffects = ui->disableEffects->isChecked();
    m_cfg->ExpMTKeyboardDiv = ui->mtKbDivs->value();
    m_cfg->ExperimentalMultiThreaded = ui->multithreading->isChecked();
    m_cfg->FloatRendering = ui->bitDepth->currentIndex() == 0;
#if defined(__linux__)
    m_cfg->BufPeriod = ui->bufPeriod->value();
    m_cfg->AudioBuf = ui->audioBuffer->value();
#elif defined(_WIN32)
    m_cfg->AudioEngine = ui->audioEngine->currentIndex();
    if (m_cfg->AudioEngine == 1)
        m_cfg->WASAPIBuf = ui->audioBuffer->value();
    else
        m_cfg->AudioBuf = ui->audioBuffer->value();
    m_cfg->StreamDirectFeed = ui->streamDirectFeed->isChecked();
    m_cfg->ASIOChunksDivision = ui->asioChunksDivision->value();
    m_cfg->ASIOLCh = std::to_string(ui->asioLCh->value());
    m_cfg->ASIORCh = std::to_string(ui->asioRCh->value());
    if (ui->asioDevice->currentData() != NULL)
        m_cfg->ASIODevice = ui->asioDevice->currentData().toString().toStdString();
#endif
}

QWidget *BASSMIDISettings::getWidget() {
    return this;
}

BASSMIDISettings::~BASSMIDISettings() {
    //delete m_cfg;
    delete ui;
}
