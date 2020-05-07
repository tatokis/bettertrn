#ifndef MIFSERIALIZER_H
#define MIFSERIALIZER_H
#include <QtGlobal>
#include <QVector>
#include <QFile>
#include <QObject>

class MifSerializer
{
public:
    static int MifToVector(QFile& f, QVector<quint32>& vec, QString& errstr);
    static int VectorToMif(QFile& f, QVector<quint32>& vec);
};

#endif // MIFSERIALIZER_H
