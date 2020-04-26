#include "asmparser.h"
#include "trnopcodes.h"
#include <QFile>
#include <QVector>
#include <QByteArray>
#include <QDebug>
#include <QHash>
#include <QRegularExpression>

QHash<QString, TrnOpcodes::TrnOpcode> AsmParser::opmap;

int AsmParser::Parse(QFile& infile, QVector<quint32>& outvec)
{
    QHash<QString, quint32> labels;

    // Go through each line
    QByteArray ba;
    // FIXME: maybe too much data?
    ba = infile.readLine();
    quint64 lnum = 0;
    QRegularExpression regex("([A-Z]+:)?[ \t]*([A-Z]+)[ \t]*(.+)?", QRegularExpression::MultilineOption);
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
        QRegularExpressionMatch m = regex.match(line, 0, QRegularExpression::PartialPreferCompleteMatch);

        // Extract the capture groups
        QString label = m.captured(1).trimmed();
        QString insn = m.captured(2).trimmed();
        QString args = m.captured(3).trimmed();

        if(insn.isEmpty())
        {
            qDebug() << "Syntax Error: no insn in line" << lnum;
            return lnum;
        }

        // Add a label
        if(!label.isEmpty())
        {
            // Remove semicolon from label
            label.chop(1);
            qDebug() << "Label" << label;
            // Add address
            // FIXME: This may not be correct
            labels[label] = outvec.length();
        }

        // Convert insn to opcode and add to vector
        qint8 op =  StrToOpcode(insn.toUpper());
        if(op > -1)
        {
            quint32 memline = (quint32)op << 15;
            // Shift the opcode all the way to the left
            outvec.append(memline);
        }
        else
        {
            // Check for pseudoinsn here
            // NAM, CON, RES, ORG, ENT, END
            // EXT too?
        }

        qDebug() << "Opcode" << insn;

        if(!args.isEmpty())
            qDebug() << "Args" << args;

        ba = infile.readLine();
    }
    return 0;
}

qint8 AsmParser::StrToOpcode(QString cmd)
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
        opmap["OUTP"] = TrnOpcodes::OUTP;
        opmap["RET"] = TrnOpcodes::RET;
        opmap["HLT"] = TrnOpcodes::HLT;
    }
    if(opmap.contains(cmd))
        return opmap[cmd];
    else
        return -1;
}

bool AsmParser::OpcodeHasArgs(qint8 op)
{
    /*switch(op)
    {
        case TrnOpcodes::
    }*/
    return false;
}

