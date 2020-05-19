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
#include <QToolButton>
#include <QFontDatabase>
#include <QDesktopServices>

#define MEM_STR_FORMAT(a, b, ai, di)    QTableWidgetItem* a = new QTableWidgetItem(QString::number(ai)); \
                                        a->setFont(monofont); \
                                        QTableWidgetItem* b = new QTableWidgetItem(QString("%1").arg(di, 20, 2, QChar('0'))); \
                                        b->setFont(monofont)

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow), emu(nullptr), animator(new TableWidgetItemAnimator(500, this)), pcarrowpos(0), _pcarrow(nullptr), monofont("Monospace"), clockDelay(500)
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

    // Enable the clear button despite the LineEdit being read only
    QToolButton* tbtn = ui->outputLineEdit->findChild<QToolButton*>();
    if(tbtn)
        tbtn->setEnabled(true);

    // Because apparently it can't be done automatically,
    // set everything containing binary numbers to use a fixed width font

    // First, check if Monospace was found
    if(QFontInfo(monofont).family() != "Monospace")
    {
        // If it wasn't, attempt to load Lucida Console,
        // But also fall back to a generic font hint
        monofont.setStyleHint(QFont::TypeWriter);
        monofont.setFamily("Lucida Console");

        // Finally, check that we actually got a fixed width font based on the hint
        // If we didn't, then try to get whatever the system's default is
        if(!QFontInfo(monofont).fixedPitch())
            monofont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    }
    // Resize it
    monofont.setPointSize(11);

    ui->memoryTable->resizeColumnsToContents();

    // Update all the labels with the new font
    ui->regBR->setFont(monofont);
    ui->regA->setFont(monofont);
    ui->regAR->setFont(monofont);
    ui->regX->setFont(monofont);
    ui->regSP->setFont(monofont);
    ui->regI->setFont(monofont);
    ui->regIR->setFont(monofont);
    ui->regPC->setFont(monofont);
    ui->regSC->setFont(monofont);
    ui->clockValue->setFont(monofont);
    ui->regF1->setFont(monofont);
    ui->regZ->setFont(monofont);
    ui->regF2->setFont(monofont);
    ui->regS->setFont(monofont);
    ui->regV->setFont(monofont);
    ui->regH->setFont(monofont);
    // And finally the output
    ui->outputLineEdit->setFont(monofont);

    // Set the default speed to 2Hz
    ui->clockSlider->setValue(2);
}

MainWindow::~MainWindow()
{
    if(emu)
        delete emu;
    if(_pcarrow)
        delete _pcarrow;
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
    QString file = QFileDialog::getOpenFileName(this, tr("Open file"), QString(), tr("TRN Code (*.asm *.mif)"));
    // Return if the dialog was cancelled
    if(file.isEmpty())
        return;

    // Now that we have the file path, we should open it and read the contents
    if(loadNewFile(file))
        return;

    // If all went okay, enable the start button
    ui->startStopBtn->setEnabled(true);
}

void MainWindow::fileChangedOnDisk(QString file)
{
    // Don't allow any more events to go through while we're waiting for the user to answer
    fswatcher.blockSignals(true);
    QFileInfo fi(file);
    QDateTime newLastModified = fi.lastModified();
    if(fileLastModified == newLastModified)
        return;

    fileLastModified = newLastModified;
    qDebug() << "File" << file << "changed";

    // Ask the user if they want to reload the file
    if(QMessageBox::question(this, tr("File was modified"), tr("The file %1 was modified.\nDo you wish to reload it?").arg(file), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
            != QMessageBox::Yes)
    {
        fswatcher.blockSignals(false);
        return;
    }

    fswatcher.blockSignals(false);
    loadNewFile(file);
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

    // If the emulator is running, tell it to shut down and restart
    if(emu)
    {
        askEmuThreadToStop();
        // This will block the UI for a bit, but the alternative is more painful
        emu->wait();
    }

    // Set up the filesystem watcher for any changes
    QStringList oldfiles = fswatcher.files();
    qDebug() << "Watcher files" << oldfiles;
    if(oldfiles.length())
        fswatcher.removePaths(oldfiles);

    fswatcher.addPath(file);
    // Clear the vector before loading the new file
    pgmmem.clear();
    // Call the correct function for asm or mif
    QString err;
    int line = (file.toLower().endsWith(".asm") ? AsmParser::Parse(f, pgmmem, err) : MifSerializer::MifToVector(f, pgmmem, err));

    if(line)
    {
        QString msgarg;
        if(line > 0)
            msgarg = " in line " + QString::number(line);
        QMessageBox::critical(this, tr("Parse error"), tr("Parse error%1\n%2").arg(msgarg, err), QMessageBox::Ok);
        // Clear the memory vector, otherwise it's possible to start executing
        pgmmem.clear();
        return 1;
    }

    // Clear tables
    ui->logTable->setRowCount(0);
    ui->memoryTable->setRowCount(0);

    for(int i = 0; i < pgmmem.size(); i++)
    {
        ui->memoryTable->insertRow(i);
        MEM_STR_FORMAT(addr, memcontent, i, pgmmem.at(i));
        ui->memoryTable->setItem(i, 0, new QTableWidgetItem());
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

void MainWindow::askEmuThreadToStop()
{
    ui->statusBar->showMessage(tr("Stopping emulator"));
    emu->requestInterruption();
    ui->startStopBtn->setEnabled(false);
    ui->pauseBtn->setEnabled(false);
    ui->stepBtn->setEnabled(false);
    resumeEmuIfRunning();
    // Send bogus input to resume execution if waiting for input but the user wants to quit
    emu->setInput(0);
}

void MainWindow::on_startStopBtn_clicked()
{
    if(emu)
    {
        askEmuThreadToStop();
        return;
    }

    ui->statusBar->clearMessage();
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
    emu = new TrnEmu(clockDelay, pgmmem, this);
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

        // Resize to contents
        ui->logTable->resizeColumnToContents(2);
    });

    connect(emu, &TrnEmu::outputSet, this, [this](quint32 out) {
        ui->outputLineEdit->setText(QString("%1").arg(out & 0b11111111111111111111 , 20, 2, QChar('0')));
    });

    connect(emu, &TrnEmu::requestInput, this, [this]() {
        ui->inputLineEdit->setEnabled(true);
        ui->inputLineEdit->setFocus();
        ui->statusBar->showMessage(tr("Waiting for user input"));
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

    // If we have an arrow item stored, set it to position 0
    if(_pcarrow)
    {
        // Table takes ownership of the item
        ui->memoryTable->setItem(0, 0, _pcarrow);
        _pcarrow = nullptr;
    }

    emu->start();
}

void MainWindow::emuThreadStopped()
{
    ui->startStopBtn->setText(tr("Start"));
    ui->pauseBtn->setText(tr("Pause"));
    ui->startStopBtn->setEnabled(true);
    ui->pauseBtn->setEnabled(false);
    ui->stepBtn->setEnabled(false);
    emu->deleteLater();
    emu = nullptr;
    ui->statusBar->showMessage(tr("Emulation finished"));
}

void MainWindow::on_actionSave_Memory_Image_triggered()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save Memory Image"), QString(), tr("Memory Image (*.mif)"));
    if(path.isEmpty())
        return;

    // If the path doesn't end with .mif, add it
    if(!path.endsWith(".mif", Qt::CaseInsensitive))
        path.append(".mif");

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
    QTableWidgetItem* memitem = ui->memoryTable->item(addr, 0);
    if(!memitem)
    {
        qDebug() << "PC arrow at" << addr << "is null. Not updating UI";
        return;
    }
    if(t == TrnEmu::OperationType::Read)
        animator->startReadAnimation(a, d, memitem, c);
    else
        animator->startWriteAnimation(a, d, memitem, c);
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
    switch(r)
    {
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
            // If the new position is outside the table, store the new arrow
            // We need to do that in case the user restarts without reloading
            if(val > ui->memoryTable->rowCount() - 1)
            {
                _pcarrow = arrow;
                pcarrowpos = 0;
                break;
            }
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
            animator->startLabelAnimation(ui->clockValue, t);
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
    emu->setInput(0);
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
        QMessageBox::warning(this, tr("Invalid number entered"), tr("The number you entered is not valid.\nSome valid values are 42, 0x2A, 0b101010, 0o52\nNumbers longer than 20 bits will be truncated."), QMessageBox::Ok);
        return;
    }

    ui->inputLineEdit->clear();
    ui->inputLineEdit->setEnabled(false);

    emu->setInput(parsed);
    ui->statusBar->clearMessage();
}

void MainWindow::on_actionTRN_Reference_triggered()
{
    openWithDefaultApp("docs/TRNdocument-v2.pdf");
}

void MainWindow::on_actionExample_Programs_triggered()
{
    openWithDefaultApp("examples");
}

void MainWindow::openWithDefaultApp(QString path)
{
    // Open the file
    // First, get the path
    QFileInfo fi(path);
    if(!fi.exists())
    {
        QMessageBox::critical(this, tr("Path not found"), tr("Could not open the specified resource.\nPath not found"));
        return;
    }
    QUrl url = QUrl::fromLocalFile(fi.absoluteFilePath());
    QDesktopServices::openUrl(url);
}

void MainWindow::on_clockSlider_valueChanged(int value)
{
    // Block the signals to not create an endless loop
    ui->clockSpinBox->blockSignals(true);
    ui->clockSpinBox->setValue(value);
    ui->clockSpinBox->blockSignals(false);
    setEmuDelay(value);
}

void MainWindow::on_clockSpinBox_valueChanged(int value)
{
    ui->clockSlider->blockSignals(true);
    ui->clockSlider->setValue(value);
    ui->clockSlider->blockSignals(false);
    setEmuDelay(value);
}

void MainWindow::setEmuDelay(int value)
{
    clockDelay = 1000 / value;
    animator->setDuration(clockDelay);
    if(emu)
        emu->setDelay(clockDelay);
}

void MainWindow::on_actionSave_Log_triggered()
{
    if(emu && !emu->getPaused())
    {
        QMessageBox::warning(this, tr("Please pause the emulator"), tr("Can not save log while the emulator is running.\nPlease pause or stop it and try again."), QMessageBox::Ok);
        return;
    }
    int rowcount = ui->logTable->rowCount();
    if(!rowcount)
    {
        QMessageBox::warning(this, tr("Log is empty"), tr("The log is empty.\nPlease run the emulator first to generate messages."), QMessageBox::Ok);
        return;
    }

    QString path = QFileDialog::getSaveFileName(this, tr("Save Log"), QString(), tr("Log File (*.csv)"));
    if(path.isEmpty())
        return;

    // If the path doesn't end with .csv, add it
    if(!path.endsWith(".csv", Qt::CaseInsensitive))
        path.append(".csv");

    QFile f(path);
    if(!f.open(QIODevice::WriteOnly))
    {
        QMessageBox::critical(this, tr("Error Saving Log"), tr("Could not open the file for writing"), QMessageBox::Ok);
        return;
    }

    // Write the header
    f.write(QString("Clock,Action,Value\n").toUtf8());
    for(int i = 0; i < rowcount; i++)
    {
        QString str = QString("%1,%2,%3\n").arg(ui->logTable->item(i, 0)->text(),
                                                ui->logTable->item(i, 1)->text(),
                                                ui->logTable->item(i, 2)->text());

        f.write(str.toUtf8());
    }
}
