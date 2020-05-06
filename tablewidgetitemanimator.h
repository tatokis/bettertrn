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
    void startReadAnimation(QTableWidgetItem* l, QTableWidgetItem* r, const QColor& c);
    void startWriteAnimation(QTableWidgetItem* l, QTableWidgetItem* r, const QColor& c);
    inline void setDuration(unsigned long sleepInterval) { unsigned long duration = sleepInterval; //calculateDuration(sleepInterval);
                                                           readAnim->setDuration(duration); writeAnim->setDuration(duration); }
    void startLabelAnimation(AnimatedLabel* l, const TrnEmu::OperationType op);
    //inline unsigned long calculateDuration(unsigned long sleepInterval) { return sleepInterval + (sleepInterval / 3); }

    //QColor getReadColour() const { return (readItemLeft ? readItemLeft->background().color() : QColor()); }
    void setReadColour(const QColor& c) { if(!readItemLeft) return; QBrush b(c); readItemLeft->setBackground(b); readItemRight->setBackground(b); }
    //QColor getWriteColour() const { return (writeItemLeft ? writeItemLeft->background().color() : QColor()); }
    void setWriteColour(const QColor& c) { if(!writeItemLeft) return; QBrush b(c); writeItemLeft->setBackground(b); writeItemRight->setBackground(b); }
    // Apparently the getters can just be stubs
    inline QColor getColour() { return QColor(); }

    ~TableWidgetItemAnimator();

signals:

public slots:
private:
    const QColor red, green, orange;
    QTableWidgetItem* readItemLeft;
    QTableWidgetItem* writeItemLeft;
    QTableWidgetItem* readItemRight;
    QTableWidgetItem* writeItemRight;
    QPropertyAnimation* readAnim;
    QPropertyAnimation* writeAnim;
    QColor* labelbg;
};

#endif // TABLEWIDGETITEMANIMATOR_H
