TEMPLATE = app
TARGET = 
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += debug
QT += xml

OBJECTS_DIR=.tmp
MOC_DIR=.tmp

HEADERS += mainwin.h tempview.h thermcam.h
SOURCES += main.cpp mainwin.cpp tempview.cpp thermcam.cpp thermcam_lock.cpp
