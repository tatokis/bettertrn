#-------------------------------------------------
#
# Project created by QtCreator 2020-04-26T16:51:40
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = bettertrn
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++14 file_copies

SOURCES += \
        main.cpp \
        mainwindow.cpp \
    aboutwindow.cpp \
    trnemu.cpp \
    asmparser.cpp \
    mifserializer.cpp \
    tablewidgetitemanimator.cpp

HEADERS += \
        mainwindow.h \
    aboutwindow.h \
    trnemu.h \
    trnopcodes.h \
    asmparser.h \
    asmlabelarg.h \
    mifserializer.h \
    tablewidgetitemanimator.h \
    animatedlabel.h \
    qoverloadlegacy.h

FORMS += \
        mainwindow.ui \
    aboutwindow.ui

COPIES += pdf
pdf.files = docs/TRNdocument-v2.pdf
pdf.path = $$OUT_PWD/docs

# FIXME: add the rest of the docs

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
