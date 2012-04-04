TEMPLATE = app
TARGET = pdf
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += qt debug

# Input
HEADERS += layout.h viewer.h resourcemanager.h grid.h
SOURCES += main.cpp layout.cpp viewer.cpp resourcemanager.cpp grid.cpp
unix:LIBS += -lpoppler-qt4
