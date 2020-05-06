#ifndef ANIMATEDLABEL_H
#define ANIMATEDLABEL_H

#include <QLabel>

class AnimatedLabel : public QLabel
{
    Q_OBJECT
    Q_PROPERTY(QColor bgColour READ getColour WRITE setColour)
public:
    AnimatedLabel(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags()) : QLabel(parent, f) { setAutoFillBackground(true); }
    AnimatedLabel(const QString& s, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags()) : QLabel(s, parent, f) { setAutoFillBackground(true); }
    inline QColor getColour() { return QColor(); }

    // The "simple" way out would've been to use setStyleSheet(QString("background-color: rgb(%1, %2, %3);").arg(c.red()).arg(c.green()).arg(c.blue()));
    // But nope, that just ruins the QLabel's frame and makes it stand out
    inline void setColour(const QColor& c) { QPalette p = this->palette(); p.setColor(QPalette::Window, c); this->setPalette(p); }
};

#endif // ANIMATEDLABEL_H
