#include "fluidsettings.h"
#include "ui_fluidsettings.h"
#include <QString>
#include "utils.h"

FluidSettings::FluidSettings(QWidget *parent, FluidConfig *config)
    : QWidget(parent)
    , ui(new Ui::FluidSettings)
    , m_cfg(config)
{
    ui->setupUi(this);
}

void FluidSettings::loadSettings() {
    // https://www.fluidsynth.org/api/CreatingAudioDriver.html
    int i = 0;
    driverIndexes.clear();
    for (auto it : audioDrivers) {
        driverIndexes[it.first] = i;
        ui->audioDriver->insertItem(i++, QString::fromStdString(it.second), QString::fromStdString(it.first));
    }
    ui->audioDriver->setCurrentIndex(driverIndexes[m_cfg->Driver]);
    ui->bitDepth->setCurrentIndex(m_cfg->SampleFormat == "float" ? 0 : 1);
    ui->sampleRate->setRate(m_cfg->SampleRate);
    ui->periods->setValue(m_cfg->Periods);
    ui->periodSize->setValue(m_cfg->PeriodSize);
    ui->voiceLimit->setValue(m_cfg->VoiceLimit);
    ui->minNoteLength->setValue(m_cfg->MinimumNoteLength);
    ui->evBufSize->setValue(m_cfg->EvBufSize);
    ui->overflowVolume->setValue(m_cfg->OverflowVolume);
    ui->overflowReleased->setValue(m_cfg->OverflowReleased);
    ui->overflowPercussion->setValue(m_cfg->OverflowPercussion);
    ui->overflowImportant->setValue(m_cfg->OverflowImportant);
    ui->threadCount->setValue(m_cfg->ThreadsCount);
    ui->expMultithreading->setCheckState(CCS(m_cfg->ExperimentalMultiThreaded));
}

void FluidSettings::storeSettings() {
    m_cfg->Driver = driverIndexes[ui->audioDriver->currentText().toStdString()];
    m_cfg->SampleFormat = ui->bitDepth->currentIndex() == 0 ? "float" : "16bits";
    m_cfg->SampleRate = ui->sampleRate->getRate();
    m_cfg->Periods = ui->periods->value();
    m_cfg->PeriodSize = ui->periodSize->value();
    m_cfg->VoiceLimit = ui->voiceLimit->value();
    m_cfg->MinimumNoteLength = ui->minNoteLength->value();
    m_cfg->EvBufSize = ui->evBufSize->value();
    m_cfg->OverflowVolume = ui->overflowVolume->value();
    m_cfg->OverflowReleased = ui->overflowReleased->value();
    m_cfg->OverflowPercussion = ui->overflowPercussion->value();
    m_cfg->OverflowImportant = ui->overflowImportant->value();
    m_cfg->ThreadsCount = ui->threadCount->value();
    m_cfg->ExperimentalMultiThreaded = ui->expMultithreading->isChecked();
}

QWidget *FluidSettings::getWidget() {
    return this;
}

FluidSettings::~FluidSettings() {
    delete m_cfg;
    delete ui;
}
