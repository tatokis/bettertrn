#include "trnemu.h"
#include <QDebug>

void TrnEmu::run()
{
    reset();
    while(!isInterruptionRequested())
    {
        qDebug() << "Thread is running";
        QThread::msleep(_sleepInterval);
    }
    qDebug() << "Thread has been interrupted";
}
