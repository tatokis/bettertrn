#ifndef PARSEASM_H
#define PARSEASM_H
#include <QVector>
#include <QFile>
#include "trnopcodes.h"
#include <QHash>

class AsmParser
{
public:
    static int Parse(QFile& infile, QVector<quint32>& outvec, QString& errstr);
private:
    static qint8 StrToOpcode(const QString& cmd);
    static QHash<QString, TrnOpcodes::TrnOpcode> opmap;
    static QHash<QString, quint16> opargmap;
    static bool MnemonicHasArgs(const qint8& op);
    static quint16 MnemonicToOpcodeArg(const QString& mn);
};

#endif // PARSEASM_H
