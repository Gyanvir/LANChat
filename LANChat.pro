QT       += core gui network widgets

CONFIG += c++17

TARGET = LANChat
TEMPLATE = app

# Ensure debug info is generated, but release builds can be made too.
CONFIG += debug_and_release

# Project directories
INCLUDEPATH += include

# Source files
SOURCES += \
    src/main.cpp \
    src/MainWindow.cpp \
    src/PeerManager.cpp \
    src/ChatManager.cpp \
    src/DatabaseManager.cpp

# Header files
HEADERS += \
    include/MainWindow.h \
    include/PeerManager.h \
    include/ChatManager.h \
    include/DatabaseManager.h
