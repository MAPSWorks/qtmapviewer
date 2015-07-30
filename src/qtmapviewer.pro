QT       += core
QT       += gui
QT       += opengl
QT       += network

TARGET = qtmapviewer
CONFIG   += console
CONFIG   += c++11
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += \
    main.cpp \
    GLWorker.cpp \
    MapViewer.cpp \
    TileFetcher.cpp \
    TileRenderer.cpp

HEADERS += \
    GLWorker.h \
    MapViewer.h \
    TileCache.h \
    TileFetcher.h \
    TileRenderer.h \
    TileTypes.h \
    MapConfig.h
