#ifndef PARSEASM_H
#define PARSEASM_H
#include <QVector>
#include <QFile>
#include "trnopcodes.h"
#include <QHash>

class AsmParser
{
public:
    static int Parse(QFile& infile, QVector<quint32>& outvec);
private:
    static qint8 StrToOpcode(QString cmd);
    static QHash<QString, TrnOpcodes::TrnOpcode> opmap;
    static bool OpcodeHasArgs(qint8 op);
};

#endif // PARSEASM_H
