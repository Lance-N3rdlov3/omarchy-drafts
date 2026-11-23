QT += core gui qml quick quickcontrols2 dbus sql network

CONFIG += c++17 release
TARGET = drafts
TEMPLATE = app

HEADERS += \
    src/actionengine.h \
    src/backend.h \
    src/cli.h \
    src/filepicker.h \
    src/markdown.h \
    src/portalfilepicker.h \
    src/store.h \
    src/syntaxhighlighter.h \
    src/templateengine.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/actionengine.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/main.cpp \
    src/markdown.cpp \
    src/portalfilepicker.cpp \
    src/store.cpp \
    src/syntaxhighlighter.cpp \
    src/templateengine.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
