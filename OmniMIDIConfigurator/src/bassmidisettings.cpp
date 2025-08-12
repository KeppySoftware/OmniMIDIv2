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

    reloadASIODevices();

    connect(ui->audioEngine, &QComboBox::currentIndexChanged, this, &BASSMIDISettings::setAudioBufferVal);
#endif
    connect(ui->threading, &QComboBox::currentIndexChanged, this, &BASSMIDISettings::setAudioBufferVal);
    connect(ui->voiceLimit, &QSpinBox::valueChanged, this, &BASSMIDISettings::updateMTValues);
    connect(ui->kbDiv, &QSpinBox::valueChanged, this, &BASSMIDISettings::updateMTValues);
    connect(ui->threading, &QComboBox::currentIndexChanged, this, &BASSMIDISettings::toggleMtOptions);
}

void BASSMIDISettings::updateMTValues() {
    ui->mtVLtotal->setText(QString::number(ui->voiceLimit->value() * ui->kbDiv->value() * 16));
}

void BASSMIDISettings::toggleMtOptions() {
    bool mt_enabled;

    switch (ui->threading->currentIndex()) {
    case BASSConfig::SingleThread:
    case BASSConfig::Standard:
        mt_enabled = false;
        break;

    case BASSConfig::Multithreaded:
        mt_enabled = true;
        break;
    }

    ui->multithreading->setEnabled(mt_enabled);
    ui->linuxAudio->setEnabled(!mt_enabled);
    ui->windowsAudio->setEnabled(!mt_enabled);
    ui->bitDepth->setEnabled(!mt_enabled);
    ui->audioEngine->setEnabled(!mt_enabled);
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

void BASSMIDISettings::setAudioBufferVal() {
    if (ui->threading->currentIndex() != 2
        && ui->audioEngine->currentIndex() == 1) // WASAPI
        ui->audioBuffer->setValue(m_cfg->WASAPIBuf);
    else // Internal
        ui->audioBuffer->setValue(m_cfg->AudioBuf);
}

void BASSMIDISettings::loadSettings() {
    ui->threading->setCurrentIndex(m_cfg->Threading);
    ui->kbDiv->setValue(m_cfg->KeyboardDivisions);
    ui->threadCount->setValue(m_cfg->ThreadCount);
    ui->maxInstanceNps->setValue(m_cfg->MaxInstanceNPS);
    ui->instanceEvBufSize->setValue(m_cfg->InstanceEvBufSize);

    ui->sampleRate->setRate(m_cfg->SampleRate);
    ui->voiceLimit->setValue(m_cfg->VoiceLimit);
    ui->evBufSize->setValue(m_cfg->GlobalEvBufSize);
    ui->renderTimeLimit->setValue(m_cfg->RenderTimeLimit);

    ui->followOverlaps->setCheckState(CCS(m_cfg->FollowOverlaps));
    ui->audioLimiter->setCheckState(CCS(m_cfg->AudioLimiter));
    ui->bitDepth->setCurrentIndex(m_cfg->FloatRendering ? 0 : 1);
    ui->monoRendering->setCheckState(CCS(m_cfg->MonoRendering));
    ui->disableEffects->setCheckState(CCS(m_cfg->DisableEffects));

    toggleMtOptions();

#if defined(__linux__)
    ui->audioBuffer->setValue(m_cfg->AudioBuf);
    ui->bufPeriod->setValue(m_cfg->BufPeriod);
#elif defined(_WIN32)
    ui->audioEngine->setCurrentIndex(m_cfg->AudioEngine);
    showWinAudioSettings(m_cfg->AudioEngine);
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

    setAudioBufferVal();
}

void BASSMIDISettings::storeSettings() {
    switch (ui->threading->currentIndex()) {
    case 0:
        m_cfg->Threading = BASSConfig::SingleThread;
        break;
    case 2:
        m_cfg->Threading = BASSConfig::Multithreaded;
        break;
    default:
        m_cfg->Threading = BASSConfig::Standard;
        break;
    }
    m_cfg->KeyboardDivisions = ui->kbDiv->value();
    m_cfg->ThreadCount = ui->threadCount->value();
    m_cfg->MaxInstanceNPS = ui->maxInstanceNps->value();
    m_cfg->InstanceEvBufSize = ui->instanceEvBufSize->value();

    m_cfg->SampleRate = ui->sampleRate->getRate();
    m_cfg->VoiceLimit = ui->voiceLimit->value();
    m_cfg->GlobalEvBufSize = ui->evBufSize->value();
    m_cfg->RenderTimeLimit = ui->renderTimeLimit->value();

    m_cfg->FollowOverlaps = ui->followOverlaps->isChecked();
    m_cfg->AudioLimiter = ui->audioLimiter->isChecked();
    m_cfg->FloatRendering = ui->bitDepth->currentIndex() == 0;
    m_cfg->MonoRendering = ui->monoRendering->isChecked();
    m_cfg->DisableEffects = ui->disableEffects->isChecked();

#if defined(__linux__)
    m_cfg->BufPeriod = ui->bufPeriod->value();
    m_cfg->AudioBuf = ui->audioBuffer->value();
#elif defined(_WIN32)
    m_cfg->AudioEngine = ui->audioEngine->currentIndex();
    if (ui->threading->currentIndex() != 2
        && ui->audioEngine->currentIndex() == 1)
        m_cfg->WASAPIBuf = ui->audioBuffer->value();
    else
        m_cfg->AudioBuf = ui->audioBuffer->value();
    m_cfg->StreamDirectFeed = ui->streamDirectFeed->isChecked();
    m_cfg->ASIOChunksDivision = ui->asioChunksDivision->value();
    m_cfg->ASIOLCh = std::to_string(ui->asioLCh->value());
    m_cfg->ASIORCh = std::to_string(ui->asioRCh->value());
    if (!ui->asioDevice->currentData().isNull())
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
