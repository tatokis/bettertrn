#include "tablewidgetitemanimator.h"
#include <QPropertyAnimation>
#include <QDebug>

TableWidgetItemAnimator::TableWidgetItemAnimator(unsigned long sleepInterval, QObject* parent) : QObject(parent),
    red(0xFF, 0x50, 0x50), green(0x50, 0xFF, 0x50), orange(0xFF, 0xA4, 0x00),
    readItemLeft(nullptr), writeItemLeft(nullptr), readItemRight(nullptr), writeItemRight(nullptr),
    readAnim(new QPropertyAnimation(this, "readColour")), writeAnim(new QPropertyAnimation(this, "writeColour")),
    labelbg(nullptr)
{
    unsigned long duration = sleepInterval; //calculateDuration(sleepInterval);
    readAnim->setDuration(duration);
    writeAnim->setDuration(duration);
    readAnim->setStartValue(green);
    writeAnim->setStartValue(red);
    readAnim->setEasingCurve(QEasingCurve::InQuint);
    writeAnim->setEasingCurve(QEasingCurve::InQuint);
    // Needed to let the start functions retrigger
    connect(readAnim, &QPropertyAnimation::finished, this, [this]() { this->readItemLeft = this->readItemRight = nullptr; });
    connect(writeAnim, &QPropertyAnimation::finished, this, [this]() { this->writeItemLeft = this->writeItemRight = nullptr; });
}

void TableWidgetItemAnimator::startReadAnimation(QTableWidgetItem* l, QTableWidgetItem* r, const QColor& c)
{
    if(readItemLeft || readItemRight)
    {
        // We need to stop the existing animation, because after this function returns, the TableWidgetItem may be deleted
        readAnim->stop();
    }
    readItemLeft = l;
    readItemRight = r;

    readAnim->setEndValue(c);
    readAnim->start();
}

void TableWidgetItemAnimator::startWriteAnimation(QTableWidgetItem* l, QTableWidgetItem* r, const QColor& c)
{
    if(writeItemLeft || writeItemRight)
    {
        // We need to stop the existing animation, because after this function returns, the TableWidgetItem may be deleted
        writeAnim->stop();
    }
    writeItemLeft = l;
    writeItemRight = r;

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
    anim->setDuration(500);
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
    anim->setEasingCurve(QEasingCurve::InQuint);
    anim->setEndValue(*labelbg);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

TableWidgetItemAnimator::~TableWidgetItemAnimator()
{
    delete readAnim;
    delete writeAnim;
    if(labelbg)
        delete labelbg;
}
