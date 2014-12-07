#ifndef __MAP_CONFIG_H_
#define __MAP_CONFIG_H_

#include <QVector2D>
#include <QGuiApplication>

// Main object used to store all map configuration state
struct MapConfig {
    QString server;    // map tile server endpoint
    QString format;    // map tile image format
    QVector2D center;  // lat/lon center of the map
    int min_zoom;      // minimum map zoom level
    int max_zoom;      // maximum map zoom level
    int zoom_level;    // starting map zoom level
    QSize map_size;    // map viewport width/height
    int tile_size;     // map tile pixel size (square)
    size_t cache_size; // tile cache size in tiles

    void print() const {
        printf("  Server:\t%s\n", qPrintable(server));
        printf("  Image format:\t%s\n", qPrintable(format));
        printf("  Map Center:\t[%f, %f]\n", center.x(), center.y());
        printf("  Zoom Range:\t[%d, %d]\n", min_zoom, max_zoom);
        printf("  Start zoom:\t%d\n", zoom_level);
        printf("  Map Size:\t%d x %d\n", map_size.width(), map_size.height());
        printf("  Tile Size:\t%d pixels\n", tile_size);
        printf("  Cache Size:\t%u tiles\n", cache_size);
    }
};

#endif
