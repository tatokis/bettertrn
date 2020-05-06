#include "trnemu.h"
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include "trnopcodes.h"

// Format strings used for logging
//static const QString ;
static const QString outofbounds = QObject::tr("Attempted to access memory out of bounds at index %1");
static const QString regincr = QObject::tr("Register %1++");
static const QString regdecr = QObject::tr("Register %1--");
static const QString regzero = QObject::tr("Register %1 = 0");
static const QString regassign = QObject::tr("%1 ← %2");
static const QString clockpulse = QObject::tr("Clock pulse");

#define EMIT_LOG(arg, val)   emit executionLog(regCLOCK, arg, val)

#define REG_LOAD(dst, src)  reg##dst = reg##src; \
                            EMIT_LOG(regassign.arg(regToString[Register::dst], regToString[Register::src]), QString::number(reg##src)); \
                            emit registerUpdated(Register::src, OperationType::Read, reg##src); \
                            emit registerUpdated(Register::dst, OperationType::Write, reg##dst)

#define REG_LOAD_MASK(dst, src, mask)   reg##dst = reg##src & mask; \
                                        emit registerUpdated(Register::src, OperationType::Read, reg##src); \
                                        emit registerUpdated(Register::dst, OperationType::Write, reg##dst)

#define REG_LOAD_DEREF(dst, src)    if((unsigned int)_memory.length() <= reg##src) \
                                    { \
                                        emit executionError(outofbounds.arg(reg##src)); \
                                        return; \
                                    } \
                                    reg##dst = _memory.at(reg##src); \
                                    emit memoryUpdated(reg##src, reg##dst, OperationType::Read)

#define REG_STORE_DEREF(dst, src)   if((unsigned int)_memory.length() <= reg##dst) \
                                    { \
                                        emit executionError(outofbounds.arg(reg##src)); \
                                        return; \
                                    } \
                                    _memory[reg##src] = reg##dst; \
                                    emit memoryUpdated(reg##src, reg##dst, OperationType::Write)

// FIXME: check if the Z S V registers need to be updated here in the ui(?), as the macro is used in an internal action as well
#define REG_INCR(dst)   reg##dst++; \
                        EMIT_LOG(regincr.arg(regToString[Register::dst]), QString::number(reg##dst)); \
                        emit registerUpdated(Register::dst, OperationType::InPlace, reg##dst)

// Same as above, but with a different log message
#define CLOCK_TICK()    regCLOCK++; \
                        EMIT_LOG(clockpulse, QString::number(regCLOCK)); \
                        emit registerUpdated(Register::CLOCK, OperationType::InPlace, regCLOCK)

#define REG_DECR(dst)   reg##dst--; \
                        EMIT_LOG(regdecr.arg(regToString[Register::dst]), QString::number(reg##dst)); \
                        emit registerUpdated(Register::dst, OperationType::InPlace, reg##dst)

#define REG_ZERO(dst)   reg##dst = 0; \
                        EMIT_LOG(regzero.arg(regToString[Register::dst]), QString::number(reg##dst)); \
                        emit registerUpdated(Register::dst, OperationType::InPlace, reg##dst)

#define CHECKPOINT      QThread::msleep(_sleepInterval); \
                        { \
                            QMutexLocker l(_isProcessing); \
                            if(_shouldPause) \
                                _cond->wait(_isProcessing); \
                        }

#define PHASE_END()     REG_INCR(SC); \
                        qDebug() << "New SC value" << regSC; \
                        CHECKPOINT

#define DO_READ()   REG_LOAD_DEREF(BR, AR)
#define DO_WRITE()  REG_STORE_DEREF(AR, BR)

/*#define REG_ZERO_OPCODE(r)  reg##r &= 0b1111111111111; \
                            emit registerUpdated(Register::r, OperationType::InPlace, reg##r)
                            */

TrnEmu::TrnEmu(unsigned long sleepInterval, QVector<quint32> pgm, QObject* parent) :
    QThread(parent), _memory(pgm), _isProcessing(new QMutex()), _sleepInterval(sleepInterval), _cond(new QWaitCondition()),
    _inputCond(new QWaitCondition()), _shouldPause(false)
{
    reset();
}

TrnEmu::~TrnEmu()
{
    delete _cond;
    delete _inputCond;
    delete _isProcessing;
}

void TrnEmu::run()
{
    while(!isInterruptionRequested())
    {
        // Tick!
        CLOCK_TICK();
        quint8 opcode;

        switch(regF)
        {
            case 0b00:
                EMIT_LOG("Fetching next instruction", QString());

                REG_LOAD(AR, PC);
                PHASE_END();

                CLOCK_TICK();
                DO_READ();
                REG_INCR(PC);
                PHASE_END();

                CLOCK_TICK();
                REG_LOAD(IR, BR);
                REG_LOAD_MASK(AR, BR, 0b1111111111111);
                PHASE_END();

                CLOCK_TICK();
                // Detect the type of reference
                if(regIR & 0b10000000000000)
                    regF = 0b01;
                else if(regIR & 0b1000000000000)
                    regF = 0b10;
                else
                    regF = 0b11;
                break;

            case 0b01:
                qDebug() << "Indirect Reference";
                regF = 0b10;
                break;

            case 0b10:
                qDebug() << "Indexed Reference";
                regF = 0b11;
                break;

            case 0b11:
                opcode = (regIR >> 15) & 0b11111;
                EMIT_LOG("Executing instruction", QString());
                // Decode and execute
                switch(opcode)
                {
                    case TrnOpcodes::NOP:
                        EMIT_LOG(tr("No Operation"), "NOP");
                        break;

                    case TrnOpcodes::LDA:
                        EMIT_LOG(tr("Load argument to register A"), "LDA");
                        DO_READ();
                        PHASE_END();

                        CLOCK_TICK();
                        REG_LOAD(A, BR);
                        break;

                    case TrnOpcodes::LDX:
                        EMIT_LOG(tr("Load argument to register X"), "LDX");
                        DO_READ();
                        PHASE_END();

                        CLOCK_TICK();
                        REG_LOAD(X, BR);
                        break;

                    case TrnOpcodes::LDI:
                        EMIT_LOG(tr("Load BR's data to register I"), "LDI");
                        DO_READ();
                        PHASE_END();

                        CLOCK_TICK();
                        REG_LOAD_MASK(I, BR, 0b1111111111111);
                        break;

                    case TrnOpcodes::STA:
                        EMIT_LOG(tr("Store register A to the argument address"), "STA");
                        REG_LOAD(A, BR);
                        PHASE_END();

                        CLOCK_TICK();
                        DO_WRITE();
                        break;

                    case TrnOpcodes::STX:
                        EMIT_LOG(tr("Store register X to the argument address"), "STX");
                        REG_LOAD(X, BR);
                        PHASE_END();

                        CLOCK_TICK();
                        DO_WRITE();
                        break;

                    case TrnOpcodes::STI:
                        EMIT_LOG(tr("Store register I's data to the argument address"), "STI");
                        // Zero the opcode and E/D fields, and then copy the data from the I register
                        regBR &= (regI & 0b1111111111111);
                        emit registerUpdated(Register::I, OperationType::Read, regI);
                        emit registerUpdated(Register::BR, OperationType::Write, regBR);
                        PHASE_END();

                        CLOCK_TICK();
                        DO_WRITE();
                        break;

                    case TrnOpcodes::ENA:
#warning "CHECK IF ENA NEEDS TO TICK THE CLOCK"
                        EMIT_LOG(tr("Load argument to register A"), "ENA");

                        REG_LOAD_MASK(A, IR, 0b1111111111111);

                        PHASE_END();
                        break;

                    case TrnOpcodes::PSH:
                        EMIT_LOG(tr("Push to the stack"), "PSH");
                        REG_INCR(SP);
                        REG_LOAD(BR, A);
                        PHASE_END();
                        CLOCK_TICK();

                        REG_LOAD(AR, SP);
                        PHASE_END();
                        CLOCK_TICK();

                        DO_WRITE();
                        break;

                    case TrnOpcodes::POP:
                        EMIT_LOG(tr("Pop from the stack"), "POP");
                        REG_LOAD(AR, SP);
                        PHASE_END();

                        CLOCK_TICK();
                        DO_READ();
                        PHASE_END();

                        CLOCK_TICK();
                        REG_LOAD(A, BR);
                        REG_DECR(SP);
                        break;

                    // Same opcode for INA, INX, INI, DCA, DCX, DCI
                    case TrnOpcodes::INA:
                        // FIXME: Handle wrap around in macros
                        switch(regIR & 0b111)
                        {
                            case InPlaceRegUpdateArg::INA:
                                EMIT_LOG(tr("Increment register A"), "INA");
                                REG_INCR(A);
                                break;
                            case InPlaceRegUpdateArg::INX:
                                EMIT_LOG(tr("Increment register X"), "INX");
                                REG_INCR(X);
                                break;
                            case InPlaceRegUpdateArg::INI:
                                EMIT_LOG(tr("Increment register I"), "INI");
                                REG_INCR(I);
                                break;
                            case InPlaceRegUpdateArg::DCA:
                                EMIT_LOG(tr("Decrement register A"), "INA");
                                REG_DECR(A);
                                break;
                            case InPlaceRegUpdateArg::DCX:
                                EMIT_LOG(tr("Decrement register X"), "INX");
                                REG_DECR(X);
                                break;
                            case InPlaceRegUpdateArg::DCI:
                                EMIT_LOG(tr("Decrement register I"), "INI");
                                REG_DECR(I);
                                break;
                        }
                        //PHASE_END(); // This might not be needed. FIXME: investigate
                        break;

                    case TrnOpcodes::ENI:
                        EMIT_LOG(tr("Load IR's argument to register I"), "ENI");
                        REG_LOAD_MASK(I, IR, 0b1111111111111);
                        break;

                    case TrnOpcodes::LSP:
                        EMIT_LOG(tr("Load stack pointer"), "LSP");
                        DO_READ();
                        PHASE_END();

                        CLOCK_TICK();
                        REG_LOAD_MASK(SP, BR, 0b1111111111111);
                        break;

                    case TrnOpcodes::ADA:
                        EMIT_LOG(tr("Add registers A and BR, and store the result to A"), "ADA");
                        DO_READ();
                        PHASE_END();

                        CLOCK_TICK();
                        regA = regA + regBR;
                        emit executionLog(regCLOCK, "A = A + BR", QString::number(regA));
                        emit registerUpdated(Register::BR, OperationType::Read, regBR);
                        emit registerUpdated(Register::A, OperationType::InPlace, regA);
                        break;

                    case TrnOpcodes::SUB:
                        EMIT_LOG(tr("Subtract memory value from A, and store the result to A"), "SUB");
                        DO_READ();
                        PHASE_END();

                        CLOCK_TICK();
                        regBR = ~regBR;
                        emit executionLog(regCLOCK, "BR = ~BR", QString::number(regA));
                        emit registerUpdated(Register::BR, OperationType::InPlace, regBR);
                        REG_INCR(A);
                        PHASE_END();

                        CLOCK_TICK();
                        regA += regBR;
                        emit registerUpdated(Register::A, OperationType::InPlace, regA);
                        emit registerUpdated(Register::BR, OperationType::Read, regBR);
                        break;

                    // FIXME: literally everything after DCI and before SAXL
                    // FIXME: Test SAXL/SAXR properly
                    case TrnOpcodes::SAXL:
                    {
                        // Combine A and X into a 64 bit register
                        // U means unused
                        // 0bUUUUUUUUUUUUUUUUUUUUUUUUAAAAAAAAAAAAAAAAAAAAXXXXXXXXXXXXXXXXXXXX
                        // Only 40 bits will be used
                        quint64 axregs = ((quint64)regX & 0b11111111111111111111) | ((((quint64)regA) & 0b11111111111111111111) << 20);

                        if(regIR & 0b1)
                        {
                            // SAXR
                            EMIT_LOG(tr("Shift registers A and X combined to the right"), "SAXR");
                            axregs = axregs >> 1;
                        }
                        else
                        {
                            // SAXL
                            EMIT_LOG(tr("Shift registers A and X combined to the left"), "SAXL");
                            axregs = axregs << 1;
                        }

                        // Split them up again
                        regX = axregs & 0b11111111111111111111;
                        regA = (axregs >> 20) & 0b11111111111111111111;
                        emit registerUpdated(Register::X, OperationType::InPlace, regX);
                        emit registerUpdated(Register::A, OperationType::InPlace, regA);
                        break;
                    }

                    // More instructions here
                    case TrnOpcodes::OUT:
                        // If the argument is 0b1, then output
                        if(regIR & 0b1)
                        {
                            EMIT_LOG(tr("Output to console"), "OUT");
                            REG_LOAD(BR, A);
                            PHASE_END();

                            CLOCK_TICK();
                            emit outputSet(regBR);
                        }
                        else
                        {
                            EMIT_LOG(tr("Read user input"), "INP");
                            {
                                QMutexLocker l(_isProcessing);
                                emit requestInput();
                                _inputCond->wait(_isProcessing);
                            }
                            PHASE_END();

                            CLOCK_TICK();
                            REG_LOAD(A, BR);
                        }
                        break;

                    case TrnOpcodes::HLT:
                        regH = 1;
                        EMIT_LOG(tr("Halt"), "HLT");
                        emit registerUpdated(Register::H, OperationType::InPlace, (quint8)1);
                        return;

                    default:
                        // FIXME: Add error string
                        qDebug() << "Invalid opcode";
                        emit executionError(tr("Invalid opcode %1").arg(opcode, 5, 2, QChar('0')));
                        return;
                }
                regF = 0b00;
                break;

        }
        // Always set SC to 0 after executing an instruction
        REG_ZERO(SC);

        emit registerUpdated(Register::F, OperationType::InPlace, regF);
        CHECKPOINT;
    }
    qDebug() << "Thread has ended";
}

void TrnEmu::pause()
{
    _paused = true;
    QMutexLocker l(_isProcessing);
    _shouldPause = true;
}

void TrnEmu::resume()
{
    _cond->wakeAll();
    QMutexLocker l(_isProcessing);
    _shouldPause = false;
    _paused = false;
}

// FIXME: This deadlocks if the user tries to stop the emulator while it's waiting for input
void TrnEmu::setInput(quint32 input)
{
    regBR = input;
    _inputCond->wakeAll();
}

void TrnEmu::step()
{
    if(_paused)
    {
        // Same as resume but doesn't set shouldPause to false
        _cond->wakeAll();
    }
}
