#ifndef TRNEMU_H
#define TRNEMU_H
#include <QVector>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

class TrnEmu : public QThread
{
Q_OBJECT
//Q_PROPERTY(Reg state READ state NOTIFY stateChanged)
public:
    TrnEmu(unsigned long sleepInterval, QVector<quint32> pgm, QObject* parent);
    ~TrnEmu();
    void run();
    void pause();
    void resume();
    inline bool getPaused() const { return _paused; }
    void setInput(quint32 input);
    typedef enum {
        // 32 bit regs
        BR,
        A,
        X,
        IR,
        // 16 bit regs
        SP,
        I,
        PC,
        AR,
        // 8 bit regs
        SC,
        CLOCK,
        F,
        V,
        Z,
        S,
        H,
        REG_MAX // used for the enum->str array length
    } Register;

    const char* const regToString[REG_MAX] = {
        "BR",
        "A",
        "X",
        "IR",
        "SP",
        "I",
        "PC",
        "AR",
        "SC",
        "CLOCK",
        "F",
        "V",
        "Z",
        "S",
        "H",
    };

    const char* const insnToString[REG_MAX] = {
        "BR",
        "A",
        "X",
        "IR",
        "SP",
        "I",
        "PC",
        "AR",
        "SC",
        "CLOCK",
        "F",
        "V",
        "Z",
        "S",
        "H",
    };

    typedef enum {
        Read,
        Write,
        InPlace, // When a register is modified in place (incremented/shifted/...)
    } OperationType;

    typedef enum {
        INA,
        INX,
        INI,
        DCA,
        DCX,
        DCI,
    } InPlaceRegUpdateArg;

public slots:
    void step();
private:
    QVector<quint32> _memory;
    quint32 regBR, regA, regX, regIR, regCLOCK;
    quint16 regSP, regI, regPC, regAR;
    quint8 regSC, regF, regV, regZ, regS, regH;
    inline void reset() { regBR = regAR = regA = regX = regIR = regSP = regI = regSC = regCLOCK = regF = regV = regZ = regS = regH = regPC = 0; }
    QMutex* _isProcessing;
    unsigned long _sleepInterval;
    QWaitCondition* _cond;
    QWaitCondition* _inputCond;
    bool _shouldPause;
    bool _paused; // This is NOT protected by a mutex. Must only be used by the parent thread. Same as getPaused
    bool overflow;
    // Private internal functions that should only be called by the emu thread
    void updateFlagReg(quint8& reg, quint8 isFlag, Register regEnum);

signals:
    //void dataModified(Register, OperationType);
    void memoryUpdated(int addr, quint32 data, OperationType t);
    void registerUpdated(Register r, OperationType t, quint8 val);
    void registerUpdated(Register r, OperationType t, quint16 val);
    void registerUpdated(Register r, OperationType t, quint32 val);
    void executionLog(quint32 clk, QString log, QString val);
    void executionError(QString err);
    void outputSet(quint32 out);
    void requestInput();
};

#endif // TRNEMU_H
