QT       += core
QT       += gui
QT       += opengl
QT       += network

TARGET = qtmapviewer
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += \
    src/main.cpp \
    src/GLWorker.cpp \
    src/MapViewer.cpp \
    src/TileFetcher.cpp \
    src/TileRenderer.cpp

HEADERS += \
    src/GLWorker.h \
    src/MapViewer.h \
    src/TileCache.h \
    src/TileFetcher.h \
    src/TileRenderer.h \
    src/TileTypes.h \
    src/MapConfig.h 
