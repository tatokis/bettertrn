#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemWatcher>
#include <QFile>
#include <QDateTime>
#include "trnemu.h"
#include "tablewidgetitemanimator.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_actionAbout_Qt_triggered();
    void on_actionAbout_BetterTRN_triggered();
    void on_actionOpen_triggered();
    void fileChangedOnDisk(QString file);
    void on_startStopBtn_clicked();
    void emuThreadStopped();
    void on_actionSave_Memory_Image_triggered();
    void on_pauseBtn_clicked();
    void memoryUpdate(int addr, quint32 data, TrnEmu::OperationType t);
    void registerUpdate(TrnEmu::Register r, TrnEmu::OperationType t, quint8 val);
    void registerUpdate(TrnEmu::Register r, TrnEmu::OperationType t, quint16 val);
    void registerUpdate(TrnEmu::Register r, TrnEmu::OperationType t, quint32 val);
    void on_inputLineEdit_editingFinished();
    void on_actionTRN_Reference_triggered();
    void on_actionExample_Programs_triggered();
    void on_clockSlider_valueChanged(int value);
    void on_clockSpinBox_valueChanged(int value);

    void on_actionSave_Log_triggered();

private:
    Ui::MainWindow *ui;
    QFileSystemWatcher fswatcher;
    QFile inputfile;
    QDateTime fileLastModified;
    int loadNewFile(QString file);
    TrnEmu* emu;
    QVector<quint32> pgmmem;
    bool resumeEmuIfRunning();
    void closeEvent(QCloseEvent* e);
    TableWidgetItemAnimator* animator;
    inline void resetGUI();
    int pcarrowpos;
    QTableWidgetItem* _pcarrow;
    void askEmuThreadToStop();
    QFont monofont;
    void openWithDefaultApp(QString path);
    unsigned long clockDelay;
    void setEmuDelay(int value);
};

#endif // MAINWINDOW_H
