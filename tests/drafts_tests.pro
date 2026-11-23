QT += core gui qml quick quickcontrols2 testlib dbus sql network
CONFIG += c++17 testcase
TARGET = drafts_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/actionengine.h \
    ../src/backend.h \
    ../src/cli.h \
    ../src/filepicker.h \
    ../src/markdown.h \
    ../src/store.h \
    ../src/syntaxhighlighter.h \
    ../src/templateengine.h \
    ../src/theme.h \
    ../src/translator.h

SOURCES += \
    drafts_tests.cpp \
    ../src/actionengine.cpp \
    ../src/backend.cpp \
    ../src/cli.cpp \
    ../src/markdown.cpp \
    ../src/store.cpp \
    ../src/syntaxhighlighter.cpp \
    ../src/templateengine.cpp \
    ../src/theme.cpp \
    ../src/translator.cpp

RESOURCES += ../src/resources.qrc
