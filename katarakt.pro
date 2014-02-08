TEMPLATE = app
TARGET = katarakt
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += qt
QT += network

DEFINES += "POPPLER_VERSION_MAJOR=`pkg-config --modversion poppler-qt4 | cut -d . -f 1`"
DEFINES += "POPPLER_VERSION_MINOR=`pkg-config --modversion poppler-qt4 | cut -d . -f 2`"
DEFINES += "POPPLER_VERSION_MICRO=`pkg-config --modversion poppler-qt4 | cut -d . -f 3`"

QMAKE_CXXFLAGS_DEBUG += -DDEBUG

# Input
HEADERS += layout.h viewer.h canvas.h resourcemanager.h grid.h search.h gotoline.h config.h download.h util.h
SOURCES += main.cpp layout.cpp viewer.cpp canvas.cpp resourcemanager.cpp grid.cpp search.cpp gotoline.cpp config.cpp download.cpp
unix:LIBS += -lpoppler-qt4
