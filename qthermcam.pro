TEMPLATE = app
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += debug
QT += xml widgets

OBJECTS_DIR=.tmp
MOC_DIR=.tmp

HEADERS += mainwin.h tempview.h thermcam.h
SOURCES += main.cpp mainwin.cpp tempview.cpp thermcam.cpp thermcam_lock.cpp
