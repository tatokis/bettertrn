#include "asmparser.h"
#include "trnopcodes.h"
#include <QFile>
#include <QVector>
#include <QByteArray>
#include <QDebug>
#include <QHash>
#include <QRegularExpression>
#include "asmlabelarg.h"

#define VEC_APPEND(val) if(outvec.size() < currentmempos + 1) outvec.resize(currentmempos + 1); outvec[currentmempos] = val; currentmempos++
//#define OPARG(op, val)          case TrnOpcodes::op: opargs = val; break

QHash<QString, TrnOpcodes::TrnOpcode> AsmParser::opmap;
QHash<QString, quint16> AsmParser::opargmap;

int AsmParser::Parse(QFile& infile, QVector<quint32>& outvec, QString& errstr)
{
    QHash<QString, int> symboltable;
    QVector<AsmLabelArg> secondpasslabels;

    // Go through each line
    QByteArray ba;
    ba = infile.readLine(512);
    quint64 lnum = 0;
    // A regex here is probably not a good idea, but it's good enough for now.
    // Famous last words!
    QRegularExpression regex("^([A-Z\\d]+:)?[ \\t]*([A-Z,]+)[ \\t]*(.+)?$", QRegularExpression::MultilineOption);
    int currentmempos = 0;
    QString curprogname;

    while(ba.length())
    {
        lnum++;
        QString line(ba);
        // Ignore empty lines
        if(line.trimmed().isEmpty())
        {
            ba = infile.readLine();
            continue;
        }
        QRegularExpressionMatch m = regex.match(line, 0);
        // Ignore empty lines
        if(!m.capturedLength())
        {
            ba = infile.readLine();
            continue;
        }

        // Extract the capture groups
        QString label = m.captured(1).trimmed();
        QString insn = m.captured(2).trimmed().toUpper();
        QString args = m.captured(3);

        // Remove comments from arguments, as the regex matches them too
        const QString commentstr("//");
        int commentstart = args.indexOf(commentstr);
        if(commentstart != -1)
            args = args.left(commentstart);
        args = args.trimmed();

        if(insn.isEmpty())
        {
            errstr = QObject::tr("No instruction found");
            return lnum;
        }

        // Error out if the program starts without NAM
        if(insn != "NAM" && curprogname.isEmpty())
        {
            errstr = QObject::tr("Program doesn't start with NAM");
            return lnum;
        }

        // Check if insn ends with ",I", and if so remove it and mark it appropriately
        quint32 indexedref = (insn.endsWith(",I") ? 0b00000010000000000000 : 0);
        if(indexedref)
            insn.chop(2);

        quint32 indirectref = (args.startsWith(QChar('(')) && args.endsWith(QChar(')')) ? 0b00000100000000000000 : 0);
        if(indirectref)
        {
            args.chop(1);
            args = args.mid(1);
        }

        qDebug() << "Mnemonic" << insn << "args" << args;

        // Add a label to the symbol table
        if(!label.isEmpty())
        {
            // Remove semicolon from label
            label.chop(1);
            qDebug() << "Label" << label;
            // Add address
            symboltable[label] = currentmempos;
            qDebug() << "LABELPOS" << currentmempos;
        }

        // Split arguments on comma
        // All of this is fine even if no arguments exist
        QVector<QStringRef> arglist = args.splitRef(QChar(','));
        // This has to be 32 bits so that it fits whole 20 bit numbers (such as ones set by CON)
        QVector<quint32> arglistint;
        bool argsok = true;
        // FIXME: Make this look nicer
        // Ignore arguments to NAM
        if(arglist.length() > 0 && !arglist.at(0).isEmpty() && insn != "NAM")
        {
            foreach(const QStringRef& _arg, arglist)
            {
                if(!_arg.length())
                {
                    errstr = QObject::tr("Empty argument passed");
                    return lnum;
                }
                QStringRef arg = _arg;
                bool hex = (args.startsWith(QChar('$')));
                int base = 10;
                if(hex)
                {
                    arg = arg.mid(1);
                    base = 16;
                }

                bool curargok;
                // Try to parse as unsigned first
                // long is guaranteed to be at least 32 bits
                quint32 numarg = (quint32)arg.toULong(&curargok, base);
                // If that fails, parse it as signed
                if(!curargok)
                    numarg = (ulong)arg.toLong(&curargok, base);

                arglistint.append(numarg);

                // If the conversion failed, check for a label
                if(curargok)
                    continue;

                // TODO: maybe refactor this
                if(arglist.count() == 1)
                {
                    bool startswithnum = false;
                    _arg.left(1).toInt(&startswithnum);
                    if(startswithnum || _arg.at(0) == QChar('$'))
                    {
                        argsok = false;
                    }
                    else
                    {
                        // If we get here, the argument contains a label
                        // We pretend that arg parsing went okay, but mark these addresses for arg replacement in the second pass

                        // We only need a very simple parser, as parentheses can't be used, and only + and - are supported
                        // Start by splitting everything on +
                        QVector<QStringRef> plusv = _arg.split(QChar('+'));
                        qint32 numres = 0;
                        QVector<QString> labels;
                        foreach (const QStringRef& plusref, plusv)
                        {
                            // Try parsing each reference as a signed int, assuming base 10
                            // This will also parse negative numbers, so we can simply add them up
                            bool plusnumparseok;
                            // Always returns 0 if conversion fails, so no need to check separately
                            numres += plusref.toInt(&plusnumparseok);
                            // If the conversion failed, then it's a label
                            if(plusnumparseok)
                                continue;

                            // We need to check if it doesn't start with a number though
                            if(plusref.at(0).isDigit())
                            {
                                errstr = QObject::tr("Labels must not start with a number");
                                return lnum;
                            }

                            labels.append(plusref.toString());
                        }
                        secondpasslabels.append(AsmLabelArg(labels, numres, currentmempos));
                    }
                }
                else
                {
                    argsok = false;
                }
            }
        }

        // Convert insn to opcode and add to vector
        qint8 op =  StrToOpcode(insn);
        if(op > -1)
        {
            // Shift the opcode all the way to the left
            quint32 memline = (quint32)op << 15;

            // Mark references as needed
            memline |= indirectref;
            memline |= indexedref;


            // Add arguments (if any)
            quint16 opargs;
            if(MnemonicHasArgs(op))
            {
                if(arglistint.count() != 1 || !argsok)
                {
                    errstr = QObject::tr("Invalid instruction argument");
                    return lnum;
                }

                opargs = arglistint.at(0);
            }
            else
            {
                // If the mnemonic takes no arguments, then check if the opcode it corresponds to needs one
                opargs = MnemonicToOpcodeArg(insn);
            }
            memline |= (opargs & 0b1111111111111);

            VEC_APPEND(memline);
        }
        else
        {
            if(insn == "CON")
            {
                // Insert data to memory
                // Resize the memory first if needed
                int argcount = arglistint.count();
                if(!argcount)
                {
                    errstr = QObject::tr("No arguments passed to CON");
                    return lnum;
                }
                qDebug() << "CON mempos" << currentmempos;
                if(outvec.size() < currentmempos + argcount)
                {
                    outvec.resize(currentmempos + argcount);
                    qDebug() << "resize to" << currentmempos + argcount << "newsize" << outvec.count();
                }

                for(int i = 0; i < argcount; i++)
                {
                    // Make sure it's chopped to 20 bits
                    outvec[currentmempos] = arglistint.at(i) & 0b11111111111111111111;
                    currentmempos++;
                }
            }
            else if(insn == "RES")
            {
                // Reserve memory
                if(arglistint.count() != 1)
                {
                    errstr = QObject::tr("Invalid argument specified");
                    return lnum;
                }

                int resarg = arglistint.at(0);
                qDebug() << "RES mempos" << currentmempos;
                if(outvec.size() < currentmempos + resarg)
                {
                    outvec.resize(currentmempos + resarg);
                    qDebug() << "resize to" << currentmempos + resarg << "newsize" << outvec.count();
                }
                currentmempos += resarg;
            }
            else if(insn == "ORG")
            {
                if(arglistint.count() != 1)
                {
                    errstr = QObject::tr("Invalid argument specified");
                    return lnum;
                }
                currentmempos = arglistint[0];
            }
            else if(insn == "NAM")
            {
                if(args.isEmpty())
                {
                    errstr = QObject::tr("No program name specified");
                    return lnum;
                }
                // If the current program name is not empty, then there was no END from the last program
                if(!curprogname.isEmpty())
                {
                    errstr = QObject::tr("Found NAM but previous program had no END");
                    return lnum;
                }
                curprogname = args;
            }
            else if(insn == "END")
            {
                curprogname = "";
            }
            // The original TRN doesn't seem to implement EXT, and ENT doesn't seem to do anything
            // So we'll ignore them
            else if(insn == "ENT" || insn == "EXT") {}
            else
            {
                errstr = QObject::tr("Unknown instruction %1").arg(insn);
                return lnum;
            }
        }

        ba = infile.readLine();
    }

    // If the current program name is not empty, then there was no END
    if(!curprogname.isEmpty())
    {
        errstr = QObject::tr("No END found at end of program");
        return lnum;
    }

    qDebug() << "Second parsing pass" << symboltable;
    // Second-pass, replace all labels with their values, now that they are all known
    foreach (const AsmLabelArg& a, secondpasslabels)
    {
        if(a.addr > outvec.length() - 1)
        {
            qDebug() << "Internal asm parser error. Address" << a.addr << "not found in memory map";
            return -1;
        }

        qint32 finalres = a.result;

        // Iterate through all the labels inside this address, and replace them
        foreach (const QString& lbl, a.labels)
        {
            if(!symboltable.contains(lbl))
            {
                errstr = QObject::tr("Label %1 not defined").arg(lbl);
                return -1;
            }

            finalres += symboltable[lbl];
        }
        qDebug() << QString::number(finalres & 0b1111111111111, 2);
        // If all went well, add the result to the instruction
        outvec[a.addr] |= (finalres & 0b1111111111111);
        if(finalres < 0)
            outvec[a.addr] = ~outvec[a.addr] + 1;
    }

    return 0;
}

qint8 AsmParser::StrToOpcode(const QString& cmd)
{
    // If the hasmap is empty, initialize it
    if(!opmap.size())
    {
        opmap["NOP"] = TrnOpcodes::NOP;
        opmap["LDA"] = TrnOpcodes::LDA;
        opmap["LDX"] = TrnOpcodes::LDX;
        opmap["LDI"] = TrnOpcodes::LDI;
        opmap["STA"] = TrnOpcodes::STA;
        opmap["STX"] = TrnOpcodes::STX;
        opmap["STI"] = TrnOpcodes::STI;
        opmap["ENA"] = TrnOpcodes::ENA;
        opmap["PSH"] = TrnOpcodes::PSH;
        opmap["POP"] = TrnOpcodes::POP;
        opmap["INA"] = TrnOpcodes::INA;
        opmap["INX"] = TrnOpcodes::INX;
        opmap["INI"] = TrnOpcodes::INI;
        opmap["DCA"] = TrnOpcodes::DCA;
        opmap["DCX"] = TrnOpcodes::DCX;
        opmap["DCI"] = TrnOpcodes::DCI;
        opmap["ENI"] = TrnOpcodes::ENI;
        opmap["LSP"] = TrnOpcodes::LSP;
        opmap["ADA"] = TrnOpcodes::ADA;
        opmap["SUB"] = TrnOpcodes::SUB;
        opmap["AND"] = TrnOpcodes::AND;
        opmap["ORA"] = TrnOpcodes::ORA;
        opmap["XOR"] = TrnOpcodes::XOR;
        opmap["CMA"] = TrnOpcodes::CMA;
        opmap["JMP"] = TrnOpcodes::JMP;
        opmap["JPN"] = TrnOpcodes::JPN;
        opmap["JAG"] = TrnOpcodes::JAG;
        opmap["JPZ"] = TrnOpcodes::JPZ;
        opmap["JPO"] = TrnOpcodes::JPO;
        opmap["JSR"] = TrnOpcodes::JSR;
        opmap["JIG"] = TrnOpcodes::JIG;
        opmap["SHAL"] = TrnOpcodes::SHAL;
        opmap["SHAR"] = TrnOpcodes::SHAR;
        opmap["SHAL"] = TrnOpcodes::SHAL;
        opmap["SHXL"] = TrnOpcodes::SHXL;
        opmap["SHXR"] = TrnOpcodes::SHXR;
        opmap["SSP"] = TrnOpcodes::SSP;
        opmap["SAXL"] = TrnOpcodes::SAXL;
        opmap["SAXR"] = TrnOpcodes::SAXR;
        opmap["INP"] = TrnOpcodes::INP;
        opmap["OUT"] = TrnOpcodes::OUT;
        opmap["RET"] = TrnOpcodes::RET;
        opmap["HLT"] = TrnOpcodes::HLT;
    }
    if(opmap.contains(cmd))
        return opmap[cmd];
    else
        return -1;
}

bool AsmParser::MnemonicHasArgs(const qint8& op)
{
    switch(op)
    {
        case TrnOpcodes::INA:
        case TrnOpcodes::NOP:
        case TrnOpcodes::HLT:
        case TrnOpcodes::INP:
        case TrnOpcodes::SHAL:
        case TrnOpcodes::SAXL:
        case TrnOpcodes::CMA:
        case TrnOpcodes::RET:
            return false;
        default:
            return true;
    }
}

quint16 AsmParser::MnemonicToOpcodeArg(const QString& mn)
{
    // Build the map if it hasn't been done already
    if(!opargmap.size())
    {
        opargmap["INA"] = 0b000;
        opargmap["INX"] = 0b001;
        opargmap["INI"] = 0b010;
        opargmap["DCA"] = 0b011;
        opargmap["DCX"] = 0b100;
        opargmap["DCI"] = 0b101;
        opargmap["SHAL"] = 0b00;
        opargmap["SHAR"] = 0b01;
        opargmap["SHXL"] = 0b10;
        opargmap["SHXR"] = 0b11;
        opargmap["SAXL"] = 0b0;
        opargmap["SAXR"] = 0b1;
        opargmap["INP"] = 0b0;
        opargmap["OUT"] = 0b1;
    }

    // We don't care here if it hasn't been found, as 0 will be returned anyway
    return opargmap[mn];
}
