#ifndef ASMLABELARG_H
#define ASMLABELARG_H
#include <QVector>
#include <QString>

class AsmLabelArg
{
public:
    // Since this will be used in a QVector, it requires a default initializer
    AsmLabelArg() : labels(), result(0), addr(-1) {}
    AsmLabelArg(const QVector<QString>& v, const qint32& r, const int& a) : labels(v), result(r), addr(a) {}
    // We can use QStringRef as long as the original string exists
    QVector<QString> labels;
    qint32 result;
    // Address to replace args in, during second pass
    int addr;
};

#endif // ASMLABELARG_H
