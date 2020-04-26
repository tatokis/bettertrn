#ifndef TRNEMU_H
#define TRNEMU_H
#include <QVector>
#include <QThread>

class TrnEmu : public QThread
{
Q_OBJECT
public:
    TrnEmu(unsigned long sleepInterval, QVector<quint32> pgm, QObject* parent) : QThread(parent), memory(pgm), _sleepInterval(sleepInterval) {_pause.store(1);}
    void run();
    void pause();
private:
    QVector<quint32> memory;
    quint32 regBR, regAR, regA, regX, regIR;
    quint16 regSP, regI;
    quint8 regSC, clock, regF1, regF2, regV, regZ, regS, regH;
    inline void reset() { regBR = regAR = regA = regX = regIR = regSP = regI = regSC = clock = regF1 = regF2 = regV = regZ = regS = regH = 0; }
    QAtomicInt _pause;
    unsigned long _sleepInterval;
};

#endif // TRNEMU_H
