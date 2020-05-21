#include "aboutwindow.h"
#include "ui_aboutwindow.h"
#include <QIcon>
#include <QPixmap>

AboutWindow::AboutWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutWindow)
{
    ui->setupUi(this);
    connect(ui->okButton, &QPushButton::clicked, this, &QWidget::close);
    // Load the icon
    QIcon ico(":/bettertrn.ico");
    // Get the 128x128 size
    ui->iconLabel->setPixmap(ico.pixmap(128, 128));
}

AboutWindow::~AboutWindow()
{
    delete ui;
}
