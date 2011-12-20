TEMPLATE = app
TARGET = pdf
DEPENDPATH += .
INCLUDEPATH += .

# Input
HEADERS += pdfviewer.h resourcemanager.h
SOURCES += main.cpp pdfviewer.cpp resourcemanager.cpp
unix:LIBS += -lpoppler-qt4
