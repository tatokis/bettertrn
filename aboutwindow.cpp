#include "aboutwindow.h"
#include "ui_aboutwindow.h"

AboutWindow::AboutWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutWindow)
{
    ui->setupUi(this);
    connect(ui->okButton, &QPushButton::clicked, this, &QWidget::close);
}

AboutWindow::~AboutWindow()
{
    delete ui;
}
