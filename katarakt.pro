TEMPLATE = app
TARGET = katarakt
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += qt
QT += network

QMAKE_CXXFLAGS_DEBUG += -DDEBUG

# Input
HEADERS += layout.h viewer.h canvas.h resourcemanager.h grid.h search.h gotoline.h config.h download.h
SOURCES += main.cpp layout.cpp viewer.cpp canvas.cpp resourcemanager.cpp grid.cpp search.cpp gotoline.cpp config.cpp download.cpp
unix:LIBS += -lpoppler-qt4
