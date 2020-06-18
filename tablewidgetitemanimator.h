#ifndef TABLEWIDGETITEMANIMATOR_H
#define TABLEWIDGETITEMANIMATOR_H

#include <QObject>
#include <QPropertyAnimation>
#include <QTableWidgetItem>
#include <QLabel>
#include "trnemu.h"
#include "animatedlabel.h"

class TableWidgetItemAnimator : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor readColour READ getColour WRITE setReadColour)
    Q_PROPERTY(QColor writeColour READ getColour WRITE setWriteColour)
public:
    explicit TableWidgetItemAnimator(unsigned long sleepInterval, QObject *parent = nullptr);
    void startReadAnimation(QTableWidgetItem* f, QTableWidgetItem* s, QTableWidgetItem* t, const QColor& c);
    void startWriteAnimation(QTableWidgetItem* f, QTableWidgetItem* s, QTableWidgetItem* t, const QColor& c);
    inline void setDuration(unsigned long sleepInterval) {
        sleepDuration = sleepInterval;
        readAnim->setDuration(sleepInterval);
        writeAnim->setDuration(sleepInterval);
    }
    void startLabelAnimation(AnimatedLabel* l, const TrnEmu::OperationType op);
    void setReadColour(const QColor& c);
    void setWriteColour(const QColor& c);
    // Apparently the getters can just be stubs
    inline QColor getColour() { return QColor(); }
    ~TableWidgetItemAnimator();
    void cancelIfInUse(QTableWidgetItem* itm);

signals:

public slots:
private:
    const QColor red, green, orange;
    QTableWidgetItem* readItemFirst;
    QTableWidgetItem* writeItemFirst;
    QTableWidgetItem* readItemSecond;
    QTableWidgetItem* writeItemSecond;
    QTableWidgetItem* readItemThird;
    QTableWidgetItem* writeItemThird;
    QPropertyAnimation* readAnim;
    QPropertyAnimation* writeAnim;
    QColor* labelbg;
    unsigned long sleepDuration;
};

#endif // TABLEWIDGETITEMANIMATOR_H
