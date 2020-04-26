#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "aboutwindow.h"
#include <QFileSystemWatcher>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include "asmparser.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow), emu(nullptr)
{
    ui->setupUi(this);
    connect(ui->actionQuit, &QAction::triggered, this, &QCoreApplication::quit);

    // Set up file system watcher
    connect(&fswatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::fileChangedOnDisk);
    ui->memoryTable->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

MainWindow::~MainWindow()
{
    if(emu)
        delete emu;
    delete ui;
}

void MainWindow::on_actionAbout_Qt_triggered()
{
    QApplication::aboutQt();
}

void MainWindow::on_actionAbout_BetterTRN_triggered()
{
    AboutWindow* wnd = new AboutWindow();
    wnd->show();
    wnd->setAttribute(Qt::WA_DeleteOnClose);
}

void MainWindow::on_actionOpen_triggered()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Open ASM file"), QString(), "*.asm");
    // Return if the dialog was cancelled
    if(file.isEmpty())
        return;

    // Now that we have the file path, we should open it and read the contents
    if(loadNewFile(file))
        return;

    // If all went okay, enable the start button
    ui->startStopBtn->setEnabled(true);
}

void MainWindow::on_actionSpeed_triggered()
{

}

void MainWindow::fileChangedOnDisk(QString file)
{
    QFileInfo fi(file);
    QDateTime newLastModified = fi.lastModified();
    if(fileLastModified == newLastModified)
        return;

    fileLastModified = newLastModified;
    qDebug() << "File" << file << "changed";

    // Ask the user if they want to reload the file
    if(QMessageBox::question(this, tr("File was modified"), tr("The file %1 was modified.\nDo you wish to reload it?").arg(file), QMessageBox::Yes, QMessageBox::No)
            != QMessageBox::Yes)
        return;

    if(loadNewFile(file))
        QMessageBox::critical(this, tr("Error opening file"), tr("Could not open the selected file"));
}

int MainWindow::loadNewFile(QString file)
{
    QFile f(file);
    if(!f.open(QIODevice::ReadOnly))
    {
        QMessageBox::critical(this, tr("Error opening file"), tr("Could not open the selected file"));
        return 1;
    }

    // Store the modification date so that we can tell if it was actually modified
    QFileInfo fi(file);
    fileLastModified = fi.lastModified();

    // Set up the filesystem watcher for any changes
    QStringList oldfiles = fswatcher.files();
    qDebug() << "Watcher files" << oldfiles;
    if(oldfiles.length())
        fswatcher.removePaths(oldfiles);

    fswatcher.addPath(file);
    int line = AsmParser::Parse(f, pgmmem);
    if(line)
        QMessageBox::critical(this, tr("Parse error"), tr("Parse error in line %1").arg(QString::number(line)), QMessageBox::Ok);
    qDebug() << pgmmem;

    // FIXME: move this to a new funtion

    for(int i = 0; i < pgmmem.size(); i++)
    {
        int row = ui->memoryTable->rowCount();
        ui->memoryTable->insertRow(row);
        QTableWidgetItem* addr = new QTableWidgetItem(QString::number(i));
        QTableWidgetItem* memcontent = new QTableWidgetItem(QString("%1").arg(pgmmem.at(i), 20, 2, QChar('0')));
        ui->memoryTable->setItem(row, 0, addr);
        ui->memoryTable->setItem(row, 1, memcontent);
    }

    return 0;
}

void MainWindow::on_startStopBtn_clicked()
{
    if(emu)
    {
        ui->startStopBtn->setText(tr("Start"));
        emu->requestInterruption();
        emu->wait();
        emu->deleteLater();
        emu = nullptr;
        return;
    }

    ui->startStopBtn->setText(tr("Stop"));
    emu = new TrnEmu(500, pgmmem, this);
    emu->start();
}
