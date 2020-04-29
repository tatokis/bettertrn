#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "aboutwindow.h"
#include <QFileSystemWatcher>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include "asmparser.h"
#include "mifserializer.h"

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
    QString file = QFileDialog::getOpenFileName(this, tr("Open file"), QString(), tr("Assembly (*.asm);;Memory Image (*mif)"));
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
    if(!f.open(QIODevice::ReadOnly | QIODevice::Text))
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
    // Clear the vector before loading the new file
    pgmmem.clear();
    // Call the correct function for asm or mif
    // FIXME: Maybe in the future use content autodetect instead of the file extension
    int line = (file.toLower().endsWith(".asm") ? AsmParser::Parse(f, pgmmem) : MifSerializer::MifToVector(f, pgmmem));
    if(line)
    {
        QMessageBox::critical(this, tr("Parse error"), tr("Parse error in line %1").arg(QString::number(line)), QMessageBox::Ok);
        return 1;
    }
    qDebug() << pgmmem;

    // FIXME: move this to a new function
    // FIXME: Use a View Model or make it more efficient
    ui->memoryTable->setRowCount(0);

    int row = ui->memoryTable->rowCount();
    for(int i = 0; i < pgmmem.size(); i++)
    {
        ui->memoryTable->insertRow(row);
        QTableWidgetItem* addr = new QTableWidgetItem(QString::number(i));
        QTableWidgetItem* memcontent = new QTableWidgetItem(QString("%1").arg(pgmmem.at(i), 20, 2, QChar('0')));
        ui->memoryTable->setItem(row, 0, addr);
        ui->memoryTable->setItem(row, 1, memcontent);
        row++;
    }

    return 0;
}

void MainWindow::on_startStopBtn_clicked()
{
    if(emu)
    {
        emu->requestInterruption();
        return;
    }

    ui->startStopBtn->setText(tr("Stop"));
    emu = new TrnEmu(500, pgmmem, this);
    connect(emu, &QThread::finished, this, &MainWindow::emuThreadStopped);
    emu->start();
}

void MainWindow::emuThreadStopped()
{
    ui->startStopBtn->setText(tr("Start"));
    emu->deleteLater();
    emu = nullptr;
}

void MainWindow::on_actionSave_Memory_Image_triggered()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save Memory Image"), QString(), tr("Memory Image (*mif)"));
    if(path.isEmpty())
        return;

    QFile f(path);
    if(!f.open(QIODevice::WriteOnly))
    {
        QMessageBox::critical(this, tr("Error Saving Memory Image"), tr("Could not open the file for writing"), QMessageBox::Ok);
        return;
    }

    int line = MifSerializer::VectorToMif(f, pgmmem);
    if(line)
        QMessageBox::critical(this, tr("Error Saving Memory Image"), tr("An error occured while writing memory address %1 to file"), QMessageBox::Ok);
}
