#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "aboutwindow.h"
#include <QFileSystemWatcher>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include "asmparser.h"
#include "mifserializer.h"
#include <QCloseEvent>
#include "tablewidgetitemanimator.h"
#include "qoverloadlegacy.h"
#include <QScrollBar>

#define MEM_STR_FORMAT(a, b, ai, di)    QTableWidgetItem* a = new QTableWidgetItem(QString::number(ai)); \
                                        QTableWidgetItem* b = new QTableWidgetItem(QString("%1").arg(di, 20, 2, QChar('0')))

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow), emu(nullptr), animator(new TableWidgetItemAnimator(500, this)), pcarrowpos(0)
{
    ui->setupUi(this);
    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::close, Qt::QueuedConnection);

    // Set up file system watcher
    connect(&fswatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::fileChangedOnDisk);
    ui->memoryTable->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);

    qRegisterMetaType<TrnEmu::Register>("Register");
    qRegisterMetaType<TrnEmu::OperationType>("OperationType");

    // Stretch the action column in the horizontal log header
    QHeaderView* hv = ui->logTable->horizontalHeader();
    hv->setSectionResizeMode(0, QHeaderView::Interactive);
    hv->setSectionResizeMode(1, QHeaderView::Stretch);
    hv->setSectionResizeMode(2, QHeaderView::Interactive);

    // Focus the start button by default
    ui->startStopBtn->setFocus();
}

MainWindow::~MainWindow()
{
    if(emu)
        delete emu;
    delete animator;
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
    //qDebug() << pgmmem;

    // Clear tables
    ui->logTable->setRowCount(0);
    ui->memoryTable->setRowCount(0);

    for(int i = 0; i < pgmmem.size(); i++)
    {
        ui->memoryTable->insertRow(i);
        MEM_STR_FORMAT(addr, memcontent, i, pgmmem.at(i));
        ui->memoryTable->setItem(i, 1, addr);
        ui->memoryTable->setItem(i, 2, memcontent);
    }

    // Set the PC arrow to the first item
    QTableWidgetItem* arrow = new QTableWidgetItem("→");
    // Get the existing font and enlarge it
    QFont arrowfont = arrow->font();
    arrowfont.setPointSize(arrowfont.pointSize() + 8);
    // Set the new font to the item and add it to the table
    arrow->setFont(arrowfont);
    // Align centre
    arrow->setTextAlignment(Qt::AlignCenter);
    ui->memoryTable->setItem(0, 0, arrow);
    pcarrowpos = 0;

    return 0;
}

void MainWindow::on_startStopBtn_clicked()
{
    if(emu)
    {
        emu->requestInterruption();
        ui->startStopBtn->setEnabled(false);
        ui->pauseBtn->setEnabled(false);
        ui->stepBtn->setEnabled(false);
        resumeEmuIfRunning();
        return;
    }

    ui->outputLineEdit->clear();

    // If there's nothing loaded in memory, ask the user to open a file
    if(!pgmmem.length())
    {
        on_actionOpen_triggered();
        // If they didn't open anything, give up
        if(!pgmmem.length())
            return;
    }

    // Clear log
    ui->logTable->setRowCount(0);

    ui->startStopBtn->setText(tr("Stop"));
    ui->pauseBtn->setEnabled(true);
    emu = new TrnEmu(500, pgmmem, this);
    connect(emu, &QThread::finished, this, &MainWindow::emuThreadStopped);
    connect(ui->stepBtn, &QPushButton::clicked, emu, &TrnEmu::step);
    connect(emu, &TrnEmu::executionError, this, [this](QString str){ QMessageBox::critical(this, tr("Fatal Execution Error"), str, QMessageBox::Ok); });
    connect(emu, &TrnEmu::executionLog, this, [this](quint32 clock, QString str, QString val) {
        int row = ui->logTable->rowCount();
        // Check if we're scrolled all the way down before inserting an item
        // If we're not, then don't autoscroll after insertion
        QScrollBar* s = ui->logTable->verticalScrollBar();
        bool autoscroll = !(s->value() < s->maximum() - 2);
        ui->logTable->insertRow(row);
        ui->logTable->setItem(row, 0, new QTableWidgetItem(QString::number(clock)));
        ui->logTable->setItem(row, 1, new QTableWidgetItem(str));
        ui->logTable->setItem(row, 2, new QTableWidgetItem(val));
        if(autoscroll)
            ui->logTable->scrollToBottom();
    });

    connect(emu, &TrnEmu::outputSet, this, [this](quint32 out) {
        ui->outputLineEdit->setText(QString("%1").arg(out & 0b11111111111111111111 , 20, 2, QChar('0')));
    });

    connect(emu, &TrnEmu::requestInput, this, [this]() {
        ui->inputLineEdit->setEnabled(true);
        ui->inputLineEdit->setFocus();
    });

    // Data signals
    connect(emu, &TrnEmu::memoryUpdated, this, &MainWindow::memoryUpdate, Qt::QueuedConnection);
    // 8 bit registers
    connect(emu, OVERLOAD_PTR(SINGLE_ARG(TrnEmu::Register, TrnEmu::OperationType, quint8),TrnEmu, registerUpdated),
            this, OVERLOAD_PTR(SINGLE_ARG(TrnEmu::Register, TrnEmu::OperationType, quint8), MainWindow, registerUpdate));
    // 16 bit registers
    connect(emu, OVERLOAD_PTR(SINGLE_ARG(TrnEmu::Register, TrnEmu::OperationType, quint16),TrnEmu, registerUpdated),
            this, OVERLOAD_PTR(SINGLE_ARG(TrnEmu::Register, TrnEmu::OperationType, quint16), MainWindow, registerUpdate));
    // 32 bit registers
    connect(emu, OVERLOAD_PTR(SINGLE_ARG(TrnEmu::Register, TrnEmu::OperationType, quint32),TrnEmu, registerUpdated),
            this, OVERLOAD_PTR(SINGLE_ARG(TrnEmu::Register, TrnEmu::OperationType, quint32), MainWindow, registerUpdate));

    resetGUI();

    emu->start();
}

void MainWindow::emuThreadStopped()
{
    ui->startStopBtn->setText(tr("Start"));
    ui->startStopBtn->setEnabled(true);
    ui->pauseBtn->setEnabled(false);
    ui->stepBtn->setEnabled(false);
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
        QMessageBox::critical(this, tr("Error Saving Memory Image"), tr("An error occured while writing memory address %1 to file").arg(line), QMessageBox::Ok);
}

bool MainWindow::resumeEmuIfRunning()
{
    if(emu->getPaused())
    {
        ui->pauseBtn->setText(tr("Pause"));
        ui->stepBtn->setEnabled(false);
        emu->resume();
        return true;
    }
    return false;
}

void MainWindow::on_pauseBtn_clicked()
{
    if(resumeEmuIfRunning())
        return;
    ui->stepBtn->setEnabled(true);
    ui->pauseBtn->setText(tr("Resume"));
    emu->pause();
}

void MainWindow::memoryUpdate(int addr, quint32 data, TrnEmu::OperationType t)
{
    // This should be safe as it's not possible to start the emulator with nothing in memory
   /* if(addr > ui->memoryTable->rowCount() - 1)
    {
        // Insert new row if needed
        //ui->memoryTable->insertRow(addr);
        // Maybe we should just throw an error?
    }*/

    // Update the row with the new contents
    MEM_STR_FORMAT(a, d, addr, data);
    ui->memoryTable->setItem(addr, 1, a);
    ui->memoryTable->setItem(addr, 2, d);
    const QPalette& p = ui->memoryTable->palette();
    const QColor& c = (addr % 2 ? p.alternateBase().color() : p.base().color());
    // In place memory updates are not supported
    if(t == TrnEmu::OperationType::Read)
        animator->startReadAnimation(a, d, ui->memoryTable->item(addr, 0), c);
    else
        animator->startWriteAnimation(a, d, ui->memoryTable->item(addr, 0), c);
}

#define REG_CASE(r)  case TrnEmu::Register::r: \
                                l = ui->reg##r; \
                                break

#define REG_CASE_MASK(r, length, m)     case TrnEmu::Register::r: \
                                        l = ui->reg##r; \
                                        len = length; \
                                        mask = m; \
                                        break

void MainWindow::registerUpdate(TrnEmu::Register r, TrnEmu::OperationType t, quint8 val)
{
    AnimatedLabel* l;
    quint8 len;
    quint32 mask;
    switch(r)
    {
       REG_CASE_MASK(SC, 2, 0b11);
       case TrnEmu::Register::F:
            // Custom handling because F is split to two separate ones in the UI for some reason
            ui->regF1->setText(QString("%1").arg(val & 0b1, 1, 2, QChar('0')));
            animator->startLabelAnimation(ui->regF1, t);
            ui->regF2->setText(QString("%1").arg((val >> 1) & 0b1, 1, 2, QChar('0')));
            animator->startLabelAnimation(ui->regF2, t);
            return;
        REG_CASE_MASK(V, 1, 1);
        REG_CASE_MASK(Z, 1, 1);
        REG_CASE_MASK(S, 1, 1);
        REG_CASE_MASK(H, 1, 1);
        default:
            qDebug() << "Unknown 8 bit register" << r;
            return;
    }
    l->setText(QString("%1").arg(val & mask , len, 2, QChar('0')));
    animator->startLabelAnimation(l, t);
}

void MainWindow::registerUpdate(TrnEmu::Register r, TrnEmu::OperationType t, quint16 val)
{
    AnimatedLabel* l;
    switch(r)    {
        REG_CASE(AR);
        // Handle PC manually to set the arrow in the table
        case TrnEmu::Register::PC:
        {
            l = ui->regPC;
            // We can't take the existing item because that will cause the animation to continue on the next line
            // Instead, we copy it (manually) and then clear the original
            QTableWidgetItem* oldarrow = ui->memoryTable->item(pcarrowpos, 0);
            QTableWidgetItem* arrow = new QTableWidgetItem(oldarrow->text());
            oldarrow->setText("");
            arrow->setFont(oldarrow->font());
            arrow->setTextAlignment(oldarrow->textAlignment());
            // Set it to the new PC position
            ui->memoryTable->setItem(val, 0, arrow);
            // Update the "pointer"
            pcarrowpos = val;
            break;
        }
        REG_CASE(I);
        REG_CASE(SP);
        default:
            qDebug() << "Unknown 16 bit register" << val;
            return;
    }
    l->setText(QString("%1").arg(val & 0b1111111111111 , 13, 2, QChar('0')));
    animator->startLabelAnimation(l, t);
}

void MainWindow::registerUpdate(TrnEmu::Register r, TrnEmu::OperationType t, quint32 val)
{
    AnimatedLabel* l;
    switch(r)
    {
        REG_CASE(BR);
        REG_CASE(IR);
        REG_CASE(A);
        REG_CASE(X);
        case TrnEmu::Register::CLOCK:
            // Clock is not in binary
            ui->clockValue->setText(QString("%1").arg(val, 7, 10, QChar('0')));
            return;
        default:
            qDebug() << "Unknown 32 bit register" << val;
            return;
    }
    l->setText(QString("%1").arg(val & 0b11111111111111111111 , 20, 2, QChar('0')));
    animator->startLabelAnimation(l, t);
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    if(!emu)
    {
        e->accept();
        return;
    }
    int res = QMessageBox::question(this, tr("Emulation currently running"),
                                    tr("The emulator is currently running.\nAre you sure you want to quit?"), QMessageBox::Cancel, QMessageBox::Yes);
    if(res == QMessageBox::Cancel)
    {
        e->ignore();
        return;
    }

    // We need to check again in case the thread stopped while the user was asked the question
    if(!emu)
    {
        e->accept();
        return;
    }
    emu->requestInterruption();
    resumeEmuIfRunning();
    emu->wait();
    // The destructor will delete emu
    e->accept();
}

const QString empty1BitReg("0");
const QString empty2BitReg("00");
//const QString empty3BitReg("000");
const QString empty7BitReg("0000000");
const QString empty13BitReg("0000000000000");
const QString empty20BitReg("00000000000000000000");

void MainWindow::resetGUI()
{
    // Would be nice if this was a bit less of a mess
    ui->regBR->setText(empty20BitReg);
    ui->regA->setText(empty20BitReg);
    ui->regX->setText(empty20BitReg);
    ui->regIR->setText(empty20BitReg);

    ui->regSP->setText(empty13BitReg);
    ui->regI->setText(empty13BitReg);
    ui->regPC->setText(empty13BitReg);
    ui->regAR->setText(empty13BitReg);

    ui->clockValue->setText(empty7BitReg);

    ui->regSC->setText(empty2BitReg);

    ui->regF1->setText(empty1BitReg);
    ui->regF2->setText(empty1BitReg);
    ui->regV->setText(empty1BitReg);
    ui->regZ->setText(empty1BitReg);
    ui->regS->setText(empty1BitReg);
    ui->regH->setText(empty1BitReg);
}

void MainWindow::on_inputLineEdit_editingFinished()
{
    QString userinput = ui->inputLineEdit->text();

    if(!emu || userinput.isEmpty())
        return;

    int base = 10;
    if(userinput.startsWith("0b", Qt::CaseInsensitive))
    {
        userinput = userinput.mid(2);
        base = 2;
    }
    else if(userinput.startsWith("0x", Qt::CaseInsensitive))
    {
        userinput = userinput.mid(2);
        base = 16;
    }
    else if(userinput.startsWith("0o", Qt::CaseInsensitive))
    {
        userinput = userinput.mid(2);
        base = 8;
    }

    qDebug() << "User input" << userinput;
    bool ok;
    quint32 parsed = userinput.toULong(&ok, base);

    if(!ok)
    {
        ui->inputLineEdit->clear();
        QMessageBox::warning(this, tr("Invalid number entered"), tr("The number you entered is not valid.\nSome valid values are 0x10, 0b010101, 42, 0o7\nNumbers larger than 20 bits will be truncated."), QMessageBox::Ok);
        return;
    }

    ui->inputLineEdit->clear();
    ui->inputLineEdit->setEnabled(false);

    emu->setInput(parsed);
}
