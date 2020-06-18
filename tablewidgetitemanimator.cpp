#include "tablewidgetitemanimator.h"
#include <QPropertyAnimation>
#include <QDebug>

TableWidgetItemAnimator::TableWidgetItemAnimator(unsigned long sleepInterval, QObject* parent) : QObject(parent),
    red(0xFF, 0x50, 0x50), green(0x50, 0xFF, 0x50), orange(0xFF, 0xA4, 0x00),
    readItemFirst(nullptr), writeItemFirst(nullptr), readItemSecond(nullptr), writeItemSecond(nullptr),
    readItemThird(nullptr), writeItemThird(nullptr),
    readAnim(new QPropertyAnimation(this, "readColour")), writeAnim(new QPropertyAnimation(this, "writeColour")),
    labelbg(nullptr)
{
    setDuration(sleepInterval);
    readAnim->setStartValue(green);
    writeAnim->setStartValue(red);
    readAnim->setEasingCurve(QEasingCurve::InOutCubic);
    writeAnim->setEasingCurve(QEasingCurve::InOutCubic);
    // Needed to let the start functions retrigger
    connect(readAnim, &QPropertyAnimation::finished, this, [this]() { this->readItemFirst = this->readItemSecond = this->readItemThird = nullptr; });
    connect(writeAnim, &QPropertyAnimation::finished, this, [this]() { this->writeItemFirst = this->writeItemSecond = this->writeItemThird = nullptr; });
}

void TableWidgetItemAnimator::startReadAnimation(QTableWidgetItem* f, QTableWidgetItem* s, QTableWidgetItem* t, const QColor& c)
{
    if(readItemFirst || readItemSecond || readItemThird)
    {
        // We need to stop the existing animation, because after this function returns, the TableWidgetItem may be deleted
        readAnim->stop();
        QBrush b(readAnim->endValue().value<QColor>());
        readItemFirst->setBackground(b);
        readItemSecond->setBackground(b);
        readItemThird->setBackground(b);
    }
    readItemFirst = f;
    readItemSecond = s;
    readItemThird = t;

    readAnim->setEndValue(c);
    readAnim->start();
}

void TableWidgetItemAnimator::startWriteAnimation(QTableWidgetItem* f, QTableWidgetItem* s, QTableWidgetItem* t, const QColor& c)
{
    if(writeItemFirst || writeItemSecond || writeItemThird)
    {
        // We need to stop the existing animation, because after this function returns, the TableWidgetItem may be deleted
        writeAnim->stop();
        QBrush b(writeAnim->endValue().value<QColor>());
        writeItemFirst->setBackground(b);
        writeItemSecond->setBackground(b);
        writeItemThird->setBackground(b);
    }
    writeItemFirst = f;
    writeItemSecond = s;
    writeItemThird = t;

    writeAnim->setEndValue(c);
    writeAnim->start();
}

void TableWidgetItemAnimator::startLabelAnimation(AnimatedLabel* l, const TrnEmu::OperationType op)
{
    // Check if the BG colour isn't set, and if so, set it
    // This needs to be done in order to avoid getting the colour before every animation
    // which can lead to feedback, resulting a non clearing field
    if(!labelbg)
        labelbg = new QColor(l->palette().window().color());

    QPropertyAnimation* anim = new QPropertyAnimation(l, "bgColour");
    anim->setDuration(sleepDuration);
    switch(op)
    {
        case TrnEmu::Read:
            anim->setStartValue(green);
            break;
        case TrnEmu::Write:
            anim->setStartValue(red);
            break;
        case TrnEmu::InPlace:
            anim->setStartValue(orange);
    }
    anim->setEasingCurve(QEasingCurve::InOutCubic);
    anim->setEndValue(*labelbg);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void TableWidgetItemAnimator::setReadColour(const QColor& c)
{
    if(!readItemFirst)
        return;
    QBrush b(c);
    readItemFirst->setBackground(b);
    readItemSecond->setBackground(b);
    readItemThird->setBackground(b);
}

void TableWidgetItemAnimator::setWriteColour(const QColor& c)
{
    if(!writeItemFirst)
        return;
    QBrush b(c);
    writeItemFirst->setBackground(b);
    writeItemSecond->setBackground(b);
    writeItemThird->setBackground(b);
}

TableWidgetItemAnimator::~TableWidgetItemAnimator()
{
    delete readAnim;
    delete writeAnim;
    if(labelbg)
        delete labelbg;
}

void TableWidgetItemAnimator::cancelIfInUse(QTableWidgetItem* itm)
{
    if(itm == readItemThird)
    {
        readAnim->stop();
        readItemThird = readItemSecond = readItemFirst = nullptr;
    }
    else if(itm == writeItemThird)
    {
        writeAnim->stop();
        writeItemThird = writeItemSecond = writeItemFirst = nullptr;
    }
}
