#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemWatcher>
#include <QFile>
#include <QDateTime>
#include "trnemu.h"

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
    void on_actionSpeed_triggered();

    void fileChangedOnDisk(QString file);
    void on_startStopBtn_clicked();

    void emuThreadStopped();

    void on_actionSave_Memory_Image_triggered();

private:
    Ui::MainWindow *ui;
    QFileSystemWatcher fswatcher;
    QFile inputfile;
    QDateTime fileLastModified;
    int loadNewFile(QString file);
    TrnEmu* emu;
    QVector<quint32> pgmmem;
};

#endif // MAINWINDOW_H
