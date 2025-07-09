#include "aboutwindow.h"
#include "ui_aboutwindow.h"

#include "utils.h"

AboutWindow::AboutWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AboutWindow)
{
    ui->setupUi(this);

    // ToDo: set OM version

    SynthVersions v = Utils::getSynthVersions();

    ui->bassver->setText(QString::fromStdString(v.bass));
    ui->bassmidiver->setText(QString::fromStdString(v.bassmidi));
    ui->xsynth->setText(QString::fromStdString(v.xsynth));
    ui->fluidver->setText(QString::fromStdString(v.fluidsynth));

    this->layout()->setSizeConstraint(QLayout::SetFixedSize);
    this->setFixedSize(this->minimumSizeHint());
}

AboutWindow::~AboutWindow()
{
    delete ui;
}
