#include "mainwindow.h"
#include "custom-widgets/soundfontlist.h"
#include "./ui_mainwindow.h"
#include <QMessageBox>
#include <QFileDialog>
#include "bassconfig.h"
#include "bassmidisettings.h"
#include "xsynthconfig.h"
#include "xsynthsettings.h"
#include "fluidconfig.h"
#include "fluidsettings.h"
#include "utils.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);
    try {
        m_cfg.loadFromDisk();
    } catch (const std::exception &e) {
        QString s = "An error occured while loading the settings:\n";
        QMessageBox::critical(this, ERROR_TITLE, s + e.what());
        exit(0);
    }

    // ToDo: Temporary
    m_sfLists.push_back(new SFListConfig('A'));
    ui->sfList->replaceList(m_sfLists[0]);

    // Setup UI
    ui->sfList->setUpHeader();
    ui->sfList->removeInvalid();
    loadSettingsToUI();
    displayEngineSettings();

    // Actions
    connect(ui->actionExit, &QAction::triggered, this, []() { exit(0); });
    connect(ui->actionAbout, &QAction::triggered, this, [this]() { m_aboutWin.show(); });

    // SoundFonts tab
    connect(ui->sfListAddBtn, &QPushButton::pressed, this, [this]() {
        auto paths = QFileDialog::getOpenFileNames(this, tr("Open File"), "",
                                                 tr("SoundFont Files (*.sf2 *.sfz)"));
        for (auto path : paths)
            if (!path.isNull())
                ui->sfList->addSoundFont(path);
    });
    connect(ui->sfListRmBtn, &QPushButton::pressed, ui->sfList, &SoundFontList::removeSelected);
    connect(ui->sfListMvUpBtn, &QPushButton::pressed, ui->sfList, &SoundFontList::moveUp);
    connect(ui->sfListMvDnBtn, &QPushButton::pressed, ui->sfList, &SoundFontList::moveDown);
    connect(ui->sfListClrBtn, &QPushButton::pressed, ui->sfList, &SoundFontList::removeAll);
    connect(ui->sfListCfgBtn, &QPushButton::pressed, ui->sfList, &SoundFontList::openCfgs);
    connect(ui->sfListReloadBtn, &QPushButton::pressed, ui->sfList, &SoundFontList::saveData);

    // Driver Settings tab
    connect(ui->engineSelector, &QComboBox::currentIndexChanged, this, [this](int idx) {
        m_cfg.Renderer = idx;
        storeConfig();
        displayEngineSettings();
    });
    connect(ui->allowKdmapi, &QCheckBox::checkStateChanged, this, [this]() {
        m_cfg.KDMAPIEnabled = ui->allowKdmapi->isChecked();
        storeConfig();
    });
    connect(ui->enginePath, &QLineEdit::textChanged, this, [this](const QString &s) {
        m_cfg.CustomRenderer = s.toStdString();
        storeConfig();
    });
    connect(ui->enginePathBrowse, &QPushButton::pressed, this, [this]() {
        auto path = QFileDialog::getOpenFileName(this, tr("Open File"), "",
#ifdef _WIN32
                                                 tr("DLL Files (*.dll)"));
#elif __linux__
                                                 tr("Shared Library Files (*.so)"));
#elif __APPLE__
                                                 tr("Shared Library Files (*.dylib)"));
#endif
        if (!path.isNull())
            ui->enginePath->setText(path);
    });

    // Synth Settings tab
    connect(ui->engineSettingsApply->button(QDialogButtonBox::Apply), &QPushButton::pressed,
            this, [this]() {
        m_engineSettings->storeSettings();
        storeConfig();
    });
    connect(ui->engineSettingsApply->button(QDialogButtonBox::Discard), &QPushButton::pressed,
            this, [this]() { m_engineSettings->loadSettings(); });
    connect(ui->engineSettingsApply->button(QDialogButtonBox::RestoreDefaults), &QPushButton::pressed,
            this, &MainWindow::resetSynthDefaults);
}

void MainWindow::loadSettingsToUI() {
    ui->engineSelector->setCurrentIndex(m_cfg.Renderer);
    ui->allowKdmapi->setCheckState(CCS(m_cfg.KDMAPIEnabled));
    ui->enginePath->setText(QString::fromStdString(m_cfg.CustomRenderer));
}

void MainWindow::displayEngineSettings() {
    ui->tabWidget->setTabEnabled(2, true);
    try {
        switch (m_cfg.Renderer) {
        case Synthesizers::BASSMIDI: {
            BASSConfig *config = new BASSConfig(&m_cfg);
            m_cfg.setSynthConfig(config);
            m_engineSettings = new BASSMIDISettings(ui->engineSettingsScrollArea, config);
            break;
        }
        case Synthesizers::FluidSynth: {
            FluidConfig *config = new FluidConfig(&m_cfg);
            m_cfg.setSynthConfig(config);
            m_engineSettings = new FluidSettings(ui->engineSettingsScrollArea, config);
            break;
        }
        case Synthesizers::XSynth: {
            XSynthConfig *config = new XSynthConfig(&m_cfg);
            m_cfg.setSynthConfig(config);
            m_engineSettings = new XSynthSettings(ui->engineSettingsScrollArea, config);
            break;
        }
        case Synthesizers::ShakraPipe:
            ui->tabWidget->setTabEnabled(2, false);
            ui->engineSettingsScrollArea->setWidget(new QWidget());
            m_cfg.setSynthConfig(nullptr);
            return;
        default:
            ui->engineSelector->setCurrentIndex(0);
            return;
        }
    } catch (const std::exception &e) {
        QString s = "An error occured while loading the synthesizer settings:\n";
        s += e.what();
        s += "\nOmniMIDI will load the default configuration.";
        QMessageBox::warning(this, WARNING_TITLE, s);
        resetSynthDefaults();
        return;
    }

    ui->engineSettingsScrollArea->setWidget(m_engineSettings->getWidget());
    m_engineSettings->loadSettings();
}

void MainWindow::storeConfig() {
    try {
        m_cfg.storeToDisk();
    } catch (const std::exception &e) {
        QString s = "An error occured while saving the configuration:\n";
        QMessageBox::warning(this, WARNING_TITLE, s + e.what());
    }
}

void MainWindow::resetSynthDefaults() {
    try {
        m_cfg.resetSynthDefaults();
    } catch (const std::exception &e) {
        QString s = "An error occured while resetting the synthesizer configuration:\n";
        QMessageBox::warning(this, WARNING_TITLE, s + e.what());
    }
}

MainWindow::~MainWindow() {
    for (auto sflist : m_sfLists)
        delete sflist;
    delete ui;
}
