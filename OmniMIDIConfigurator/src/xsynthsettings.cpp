#include "xsynthsettings.h"
#include "ui_xsynthsettings.h"
#include "utils.h"

XSynthSettings::XSynthSettings(QWidget *parent, XSynthConfig *config)
    : QWidget(parent)
    , ui(new Ui::XSynthSettings)
    , m_cfg(config)
{
    ui->setupUi(this);
    connect(ui->limitLayers, &QCheckBox::checkStateChanged, this, [this]() {
        ui->layerCount->setEnabled(ui->limitLayers->isChecked());
    });
    connect(ui->multithreading, &QComboBox::currentIndexChanged, this, [this](int idx) {
        ui->threadCount->setEnabled(idx == 1);
    });
}

void XSynthSettings::loadSettings() {
    if (m_cfg->LayerCount == 0)
        ui->limitLayers->setCheckState(CCS(false));
    else
        ui->limitLayers->setCheckState(CCS(true));
    ui->layerCount->setValue(m_cfg->LayerCount);
    ui->fadeOutKilling->setCheckState(CCS(m_cfg->FadeOutKilling));
    ui->interpolation->setCurrentIndex(m_cfg->Interpolation);
    ui->renderWindow->setValue(m_cfg->RenderWindow);

    int mtIdx;
    switch (m_cfg->ThreadsCount) {
    case -1:
        mtIdx = 2;
        break;
    case 0:
        mtIdx = 0;
        break;
    default:
        mtIdx = 1;
        ui->threadCount->setValue(m_cfg->ThreadsCount);
    }
    ui->multithreading->setCurrentIndex(mtIdx);
}

void XSynthSettings::storeSettings() {
    if (ui->limitLayers->isChecked())
        m_cfg->LayerCount = ui->layerCount->value();
    else
        m_cfg->LayerCount = 0;
    m_cfg->FadeOutKilling = ui->fadeOutKilling->isChecked();
    m_cfg->Interpolation = ui->interpolation->currentIndex();
    m_cfg->RenderWindow = ui->renderWindow->value();
    switch (ui->multithreading->currentIndex()) {
    case 0:
        m_cfg->ThreadsCount = 0;
        break;
    case 1:
        m_cfg->ThreadsCount = ui->threadCount->value();
        break;
    case 2:
        m_cfg->ThreadsCount = -1;
        break;
    default:
        break;
    }
}

QWidget *XSynthSettings::getWidget() {
    return this;
}

XSynthSettings::~XSynthSettings() {
    delete m_cfg;
    delete ui;
}
