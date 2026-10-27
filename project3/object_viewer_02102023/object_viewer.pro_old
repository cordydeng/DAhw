QT += widgets

QMAKE_CXXFLAGS += -g
INCLUDEPATH += $$PWD/gdsFileParser/include

SOURCES    = main.cpp \
             canvas.cpp \
             shape.cpp \
             main_window.cpp \
             gdsFileParser/src/gdsFileParser.cpp \
             gdsFileParser/src/gdsFileParserMain.cpp

HEADERS    = canvas.h \
             main_window.h \
             shape.h \
             transform.h 

RESOURCES   = object_viewer.qrc

# install
target.path = object_viewer 
INSTALLS += target
