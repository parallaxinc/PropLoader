QT = core
QT += serialport

CONFIG += console
CONFIG -= app_bundle

TARGET = proploader
TEMPLATE = app

HEADERS += \
    propellerloader.h \
    propellerconnection.h \
    serialpropellerconnection.h \
    loadelf.h \
    fastpropellerloader.h \
    propellerimage.h

SOURCES += \
    main.cpp \
    propellerloader.cpp \
    propellerconnection.cpp \
    serialpropellerconnection.cpp \
    loadelf.c \
    fastpropellerloader.cpp \
    propellerimage.cpp