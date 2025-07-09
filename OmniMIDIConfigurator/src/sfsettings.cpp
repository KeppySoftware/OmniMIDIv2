#include "sfsettings.h"
#include "ui_sfsettings.h"
#include <QPushButton>
#include "utils.h"

SFSettings::SFSettings(SoundFont *sf, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SFSettings)
    , m_sf(sf)
{
    ui->setupUi(this);

    loadSettings();

    connect(ui->buttonBox->button(QDialogButtonBox::Ok), &QPushButton::pressed, this, [this]() {
        saveSettings();
        emit modified();
        close();
    });
    connect(ui->buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::pressed, this, [this]() {
        loadSettings();
        close();
    });
}

void SFSettings::loadSettings() {
    ui->sBank->setValue(m_sf->sbank);
    ui->sPreset->setValue(m_sf->spreset);
    ui->dBank->setValue(m_sf->dbank);
    ui->dPreset->setValue(m_sf->dpreset);
    ui->dBankLsb->setValue(m_sf->dbanklsb);
    ui->xgmode->setCheckState(CCS(m_sf->xgdrums));
    ui->minfx->setCheckState(CCS(m_sf->minfx));
    ui->nofx->setCheckState(CCS(m_sf->nofx));
    ui->sblimits->setCheckState(CCS(m_sf->nolimits));
    ui->norampin->setCheckState(CCS(m_sf->norampin));
    //ToDo: envelope
}

void SFSettings::saveSettings() {
    m_sf->sbank = ui->sBank->value();
    m_sf->spreset = ui->sPreset->value();
    m_sf->dbank = ui->dBank->value();
    m_sf->dpreset = ui->dPreset->value();
    m_sf->dbanklsb = ui->dBankLsb->value();
    m_sf->xgdrums = ui->xgmode->isChecked();
    m_sf->minfx = ui->minfx->isChecked();
    m_sf->nofx = ui->nofx->isChecked();
    m_sf->nolimits = ui->sblimits->isChecked();
    m_sf->norampin = ui->norampin->isChecked();
}

SFSettings::~SFSettings()
{
    delete ui;
}
