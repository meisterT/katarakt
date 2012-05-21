TEMPLATE = app
TARGET = katarakt
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += qt

QMAKE_CXXFLAGS_DEBUG += -DDEBUG

# Input
HEADERS += layout.h viewer.h canvas.h resourcemanager.h grid.h search.h
SOURCES += main.cpp layout.cpp viewer.cpp canvas.cpp resourcemanager.cpp grid.cpp search.cpp
unix:LIBS += -lpoppler-qt4
