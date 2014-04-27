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
HEADERS +=	src/layout/layout.h src/layout/presentationlayout.h src/layout/gridlayout.h \
			src/viewer.h src/canvas.h src/resourcemanager.h src/grid.h src/search.h src/gotoline.h src/config.h src/download.h src/util.h

SOURCES +=	src/main.cpp \
			src/layout/layout.cpp src/layout/presentationlayout.cpp src/layout/gridlayout.cpp \
			src/viewer.cpp src/canvas.cpp src/resourcemanager.cpp src/grid.cpp src/search.cpp src/gotoline.cpp src/config.cpp src/download.cpp src/util.cpp
unix:LIBS += -lpoppler-qt4
