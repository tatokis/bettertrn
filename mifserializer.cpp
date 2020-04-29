#include "mifserializer.h"

int MifSerializer::MifToVector(QFile& f, QVector<quint32>& vec)
{
    QByteArray ba = f.readLine();
    int lnum = 0;
    while(ba.length())
    {
        lnum++;
        // Split the line in the tab character
        QString line(ba);
        QVector<QStringRef> split = line.splitRef(QChar('\t'));

        if(split.length() != 2)
            return lnum;

        bool ok;
        int currentmempos = (int)split.at(0).toUShort(&ok, 2);
        if(!ok)
            return lnum;

        unsigned long val = split.at(1).toULong(&ok, 2);
        if(!ok)
            return lnum;

        // Because they used a hashmap originally, we need to check if we need to expand the vector
        // as the addresses might skip, and then insert the line
        if(vec.size() < currentmempos + 1)
            vec.resize(currentmempos + 1);

        vec[currentmempos] = val;

        ba = f.readLine();
    }
    return 0;
}

// This file must be opened in binary mode
int MifSerializer::VectorToMif(QFile& f, QVector<quint32>& vec)
{
    for(int i = 0; i < vec.length(); i++)
    {
        QString str = QString("%1\t%2\n").arg(i, 13, 2, QChar('0')).arg(vec.at(i), 20, 2, QChar('0'));
        if(!f.write(str.toUtf8()))
            return i + 1;
    }
    return 0;
}
