#include "trnemu.h"
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include "trnopcodes.h"

#warning Implement first and last overflow cases
// Note: The original TRN checks for overflow only under the following conditions
// A = A + BR
// A++
// A--
// AR = (IR & 0b1111111111111) + I
// The last one happens when doing an indexed reference

// Overflow is always set to 0 after a JPO
// Else, it's updated instantly

// The S register is set to the same value as the A register sign bit (19)
// It is updated after every phase
// Z is set to 1 if A == 0
// This is also updated after every phase
// WARNING: The original TRN starts with Z set to 0, even though A is 0 too

// Format strings used for logging
static const QString outofbounds = QObject::tr("Attempted to access memory out of bounds at index %1.");
static const QString regincr = QObject::tr("Register %1++");
static const QString regdecr = QObject::tr("Register %1--");
static const QString regzero = QObject::tr("Register %1 = 0");
static const QString regassign("%1 ← %2");
static const QString regassignmask("%1 ← (%2 & %3)");
static const QString regassignormask("%1 ← %1 | (%2 & %3)");
static const QString clockpulse = QObject::tr("Clock pulse");
static const QString regldderef("%1 ← [%2]");
static const QString regstderef("[%1] ← %2");

// FIXME: convert some of these into functions

#define EMIT_LOG(arg, val)   emit executionLog(regCLOCK, arg, val)

#define REG_LOAD(dst, src)  reg##dst = reg##src; \
                            EMIT_LOG(regassign.arg(regToString[Register::dst], regToString[Register::src]), QString::number(reg##src)); \
                            emit registerUpdated(Register::src, OperationType::Read, reg##src); \
                            emit registerUpdated(Register::dst, OperationType::Write, reg##dst)

#define REG_LOAD_MASK(dst, src, mask)   reg##dst = reg##src & mask; \
                                        EMIT_LOG(regassignmask.arg(regToString[Register::dst], regToString[Register::src], \
                                            QString("0b%1").arg(mask, 13, 2, QChar('0'))), \
                                            QString::number(reg##dst)\
                                        ); \
                                        emit registerUpdated(Register::src, OperationType::Read, reg##src); \
                                        emit registerUpdated(Register::dst, OperationType::Write, reg##dst)

// Sign extension
// It's pretty easy since we're always going from 13 bits to 20
// If the sign bit is 1, we just OR 0b1111111000000000000
// If it's not, we can just leave it as-is, as it will default to 0 due to how REG_LOAD_MASK works
#warning Check if ENI needs sign extension as well
#define REG_LOAD_SIGN_EXT(dst, src)     reg##dst = reg##src & 0b1111111111111; \
                                        if(reg##dst & 0b1000000000000) \
                                            reg##dst |= 0b11111110000000000000; \
                                        EMIT_LOG(regassignmask.arg(regToString[Register::dst], regToString[Register::src], \
                                            QString("0b1111111111111")), \
                                            QString::number(reg##dst)\
                                        ); \
                                        emit registerUpdated(Register::src, OperationType::Read, reg##src); \
                                        emit registerUpdated(Register::dst, OperationType::Write, reg##dst)


#define REG_LOAD_OR_MASK(dst, src, mask)    reg##dst |= reg##src & mask; \
                                            EMIT_LOG(regassignormask.arg(regToString[Register::dst], regToString[Register::src], \
                                              QString("0b%1").arg(mask, 13, 2, QChar('0'))), \
                                              QString::number(reg##dst)\
                                            ); \
                                            emit registerUpdated(Register::src, OperationType::Read, reg##src); \
                                            emit registerUpdated(Register::dst, OperationType::Write, reg##dst)

#define REG_LOAD_DEREF(dst, src)    if((unsigned int)_memory.length() <= reg##src) \
                                    { \
                                        emit executionError(outofbounds.arg(reg##src)); \
                                        return; \
                                    } \
                                    reg##dst = _memory.at(reg##src); \
                                    EMIT_LOG(regldderef.arg(regToString[Register::dst], regToString[Register::src]), QString::number(reg##dst));\
                                    emit memoryUpdated(reg##src, reg##dst, OperationType::Read)

#define REG_STORE_DEREF(dst, src)   if((unsigned int)_memory.length() <= reg##dst) \
                                    { \
                                        emit executionError(outofbounds.arg(reg##src)); \
                                        return; \
                                    } \
                                    _memory[reg##dst] = reg##src; \
                                    EMIT_LOG(regstderef.arg(regToString[Register::dst], regToString[Register::src]), QString::number(reg##dst));\
                                    emit memoryUpdated(reg##dst, reg##src, OperationType::Write)

// FIXME: check if the Z S V registers need to be updated here in the ui(?), as the macro is used in an internal action as well
#define REG_INCR(dst)   reg##dst++; \
                        reg##dst &= 0b11111111111111111111; \
                        EMIT_LOG(regincr.arg(regToString[Register::dst]), QString::number(reg##dst)); \
                        emit registerUpdated(Register::dst, OperationType::InPlace, reg##dst)

// Same as above, but with a different log message
#define CLOCK_TICK()    regCLOCK++; \
                        EMIT_LOG(clockpulse, QString::number(regCLOCK)); \
                        emit registerUpdated(Register::CLOCK, OperationType::InPlace, regCLOCK)

#define REG_DECR(dst)   reg##dst--; \
                        reg##dst &= 0b11111111111111111111; \
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

TrnEmu::TrnEmu(unsigned long sleepInterval, QVector<quint32> pgm, QObject* parent) :
    QThread(parent), _memory(pgm), _isProcessing(new QMutex()), _sleepInterval(sleepInterval), _cond(new QWaitCondition()),
    _inputCond(new QWaitCondition()), _shouldPause(false), _paused(false), overflow(false)
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
                if(regIR & 0b10000000000000) // Indexed
                    regF = 0b01;
                else if(regIR & 0b100000000000000) // Indirect
                    regF = 0b10;
                else
                    regF = 0b11; // None
                break;

            case 0b01:
                EMIT_LOG("Dereferencing argument", QString("Indexed"));
                regAR = (regIR & 0b1111111111111) + regI;
                EMIT_LOG("AR ← (IR & 0b1111111111111) + I", QString::number(regAR));
                emit registerUpdated(Register::IR, OperationType::Read, regIR);
                emit registerUpdated(Register::I, OperationType::Read, regI);
                emit registerUpdated(Register::AR, OperationType::Write, regAR);

                // Now that we're done, check if we also need to perform an indirect deref
                if(regIR & 0b100000000000000)
                    regF = 0b10;
                else
                    regF = 0b11;
                break;

            case 0b10:
                EMIT_LOG("Dereferencing argument", QString("Indirect"));
                DO_READ();
                PHASE_END();

                CLOCK_TICK();
                REG_LOAD_MASK(AR, BR, 0b1111111111111);
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
                        REG_LOAD(BR, A);
                        PHASE_END();

                        CLOCK_TICK();
                        DO_WRITE();
                        break;

                    case TrnOpcodes::STX:
                        EMIT_LOG(tr("Store register X to the argument address"), "STX");
                        REG_LOAD(BR, X);
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
                        EMIT_LOG(tr("Load argument to register A"), "ENA");

                        REG_LOAD_SIGN_EXT(A, IR);

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
                                EMIT_LOG(tr("Decrement register A"), "DCA");
                                REG_DECR(A);
                                break;
                            case InPlaceRegUpdateArg::DCX:
                                EMIT_LOG(tr("Decrement register X"), "DCX");
                                REG_DECR(X);
                                break;
                            case InPlaceRegUpdateArg::DCI:
                                EMIT_LOG(tr("Decrement register I"), "DCI");
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
                        //ADD_WITH_OVERFLOW_CHECK(A, BR);
                        regA += regBR;
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

                    case TrnOpcodes::AND:
                        EMIT_LOG(tr("AND registers A and BR"), "AND");
                        DO_READ();
                        PHASE_END();

                        CLOCK_TICK();
                        regA &= regBR;
                        EMIT_LOG("A = A & BR", QString::number(regA));
                        emit registerUpdated(Register::A, OperationType::InPlace, regA);
                        emit registerUpdated(Register::BR, OperationType::Read, regBR);
                        break;

                    case TrnOpcodes::ORA:
                        EMIT_LOG(tr("OR registers A and BR"), "ORA");
                        DO_READ();
                        PHASE_END();

                        CLOCK_TICK();
                        regA |= regBR;
                        EMIT_LOG("A = A | BR", QString::number(regA));
                        emit registerUpdated(Register::A, OperationType::InPlace, regA);
                        emit registerUpdated(Register::BR, OperationType::Read, regBR);
                        break;

                    case TrnOpcodes::XOR:
                        EMIT_LOG(tr("XOR registers A and BR"), "XOR");
                        DO_READ();
                        PHASE_END();

                        CLOCK_TICK();
                        regA ^= regBR;
                        EMIT_LOG("A = A ^ BR", QString::number(regA));
                        emit registerUpdated(Register::A, OperationType::InPlace, regA);
                        emit registerUpdated(Register::BR, OperationType::Read, regBR);
                        break;

                    case TrnOpcodes::CMA:
                        EMIT_LOG(tr("Calculate register A's complement"), "CMA");
                        regA = (~regA) & 0b11111111111111111111;
                        EMIT_LOG("A = ~A", QString::number(regA));
                        emit registerUpdated(Register::A, OperationType::InPlace, regA);
                        break;

                    case TrnOpcodes::JMP:
                        EMIT_LOG(tr("Jump to address"), "JMP");
                        REG_LOAD_MASK(PC, BR, 0b1111111111111);
                        break;

                    case TrnOpcodes::JPN:
                        EMIT_LOG(tr("Jump to address if A is negative"), "JPN");
                        if(regS)
                            REG_LOAD_MASK(PC, BR, 0b1111111111111);
                        break;

                    case TrnOpcodes::JAG:
                        EMIT_LOG(tr("Jump to address if A is greater than zero"), "JAG");
                        if(!(regS || regZ))
                            REG_LOAD_MASK(PC, BR, 0b1111111111111);
                        break;

                    case TrnOpcodes::JPZ:
                        EMIT_LOG(tr("Jump to address if A is zero"), "JPZ");
                        if(regZ)
                            REG_LOAD_MASK(PC, BR, 0b1111111111111);
                        break;

                    case TrnOpcodes::JPO:
                        EMIT_LOG(tr("Jump to address if overflow"), "JPO");
                        if(regV)
                        {
                            REG_LOAD_MASK(PC, BR, 0b1111111111111);
                            REG_ZERO(V); // Reset the overflow
                        }
                        break;

                    case TrnOpcodes::JSR:
                        EMIT_LOG(tr("Jump to subroutine address"), "JSR");
                        REG_INCR(SP);
                        PHASE_END();

                        // For some reason these are executed in the same clock cycle
                        //CLOCK_TICK();
                        REG_LOAD(AR, SP);
                        REG_LOAD_OR_MASK(BR, PC, 0b1111111111111);
                        PHASE_END();

                        CLOCK_TICK();
                        DO_WRITE();
                        REG_LOAD_MASK(PC, IR, 0b1111111111111);
                        break;

                    case TrnOpcodes::JIG:
                        EMIT_LOG(tr("Jump to address if I is greater than zero"), "JPO");
                        // Check if the first 10 bits are greater than 0, and then make sure the 20th bit is 0
                        if((regI & 0b01111111111111111111) > 0 && (regI & 0b10000000000000000000) == 0)
                            REG_LOAD_MASK(PC, BR, 0b1111111111111);
                        break;

                    // More stuff here
                    case TrnOpcodes::SHAL:
                        switch(regIR & 0b11)
                        {
                        case 0b00:
                            EMIT_LOG(tr("Left shift register A"), "SHAL");
                            regA <<= 1;
                            emit registerUpdated(Register::A, OperationType::InPlace, regA);
                            EMIT_LOG(tr("A << 1"), QString::number(regA));
                            break;
                        case 0b01:
                            EMIT_LOG(tr("Right shift register A"), "SHAR");
                            regA >>= 1;
                            EMIT_LOG(tr("A >> 1"), QString::number(regA));
                            emit registerUpdated(Register::A, OperationType::InPlace, regA);
                            break;
                        case 0b10:
                            EMIT_LOG(tr("Left shift register X"), "SHXL");
                            regX <<= 1;
                            EMIT_LOG(tr("X << 1"), QString::number(regX));
                            emit registerUpdated(Register::X, OperationType::InPlace, regX);
                            break;
                        case 0b11:
                            EMIT_LOG(tr("Right shift register X"), "SHXR");
                            regX >>= 1;
                            EMIT_LOG(tr("X >> 1"), QString::number(regX));
                            emit registerUpdated(Register::X, OperationType::InPlace, regX);
                            break;
                        }
                        break;

                    case TrnOpcodes::SSP:
                        REG_LOAD_OR_MASK(BR, SP, 0b1111111111111);
                        PHASE_END();

                        CLOCK_TICK();
                        DO_WRITE();
                        break;

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

                    case TrnOpcodes::RET:
                        EMIT_LOG(tr("Return from subroutine"), "RET");
                        REG_LOAD(AR, SP);
                        PHASE_END();

                        CLOCK_TICK();
                        DO_READ();
                        PHASE_END();

                        CLOCK_TICK();
                        REG_LOAD_MASK(PC, BR, 0b1111111111111);
                        REG_DECR(SP);
                        REG_ZERO(F);
                        PHASE_END();

                        CLOCK_TICK();
                        // Manually zero out SC here and go back to the start of the loop due to how this instruction has to be implemented
                        REG_ZERO(SC);
                        continue;

                    case TrnOpcodes::HLT:
                    {
                        regH = 1;
                        EMIT_LOG(tr("Halt"), "HLT");
                        QString num = QString::number(1);
                        EMIT_LOG(regassign.arg(regToString[Register::H], num), num);
                        emit registerUpdated(Register::H, OperationType::InPlace, (quint8)1);
                        return;
                    }
                    default:
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

        // TRN does these at the end of each phase, so we'll do the same here, even though it's a bit wasteful
        // Only update the UI if the state has changed

        // Check for Zero
        quint8 isZero = !(regA & 0b11111111111111111111);
        // This only works because regZ can either be 0 or 1, otherwise we'd need to !!regZ
        updateFlagReg(regZ, isZero, Register::Z);

        // Check for sign. It's the 19th bit
        quint8 isNegative = !!(regA & 0b10000000000000000000);
        updateFlagReg(regS, isNegative, Register::S);

        // Finally, check for overflow
        // We need to use a separate variable, as it gets checked on every cycle
        updateFlagReg(regV, overflow, Register::V);

        CHECKPOINT;
    }
    qDebug() << "TRN Emulation thread has ended";
}

void TrnEmu::updateFlagReg(quint8& reg, quint8 isFlag, Register regEnum)
{
    if(isFlag == reg)
        return;
    reg = isFlag;
    QString num = QString::number(isFlag);
    EMIT_LOG(regassign.arg(regToString[regEnum], num), num);
    emit registerUpdated(regEnum, OperationType::InPlace, isFlag);
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
