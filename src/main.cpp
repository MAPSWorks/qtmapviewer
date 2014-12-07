// Nolan Goodnight
// ngoodnight@gmail.com

#include "MapViewer.h"
#include "MapConfig.h"
#include <QtGui/QGuiApplication>
#include <QCommandLineParser>
#include <QHostInfo>

// Parse the command line a use options to override the MapConfig defaults
bool parseCommandLine(MapConfig& config, QString& error)
{
    QCommandLineParser parser;
    const QCommandLineOption helpOption = parser.addHelpOption();

    QCommandLineOption server(QStringList() << "s" << "server-url",
            QCoreApplication::translate("main", "Map tile server URL with trailing /"),
            QCoreApplication::translate("main", "URL"));
    parser.addOption(server);

    QCommandLineOption format(QStringList() << "f" << "image-format",
            QCoreApplication::translate("main", "Map tile image format (e.g. png)"),
            QCoreApplication::translate("main", "format"));
    parser.addOption(format);
    
    QCommandLineOption min_zoom(QStringList() << "min-zoom",
            QCoreApplication::translate("main", "Map minimum zoom level"),
            QCoreApplication::translate("main", "zoom"));
    parser.addOption(min_zoom);

    QCommandLineOption max_zoom(QStringList() << "max-zoom",
            QCoreApplication::translate("main", "Map maximum zoom level"),
            QCoreApplication::translate("main", "zoom"));
    parser.addOption(max_zoom);

    QCommandLineOption tile_size(QStringList() << "t" << "tile-size",
            QCoreApplication::translate("main", "Map tile size in pixels (e.g. 256)"),
            QCoreApplication::translate("main", "size"));
    parser.addOption(tile_size);

    QCommandLineOption cache_size(QStringList() << "c" << "cache-size",
            QCoreApplication::translate("main", "Map tile cache size in tiles (e.g. 512)"),
            QCoreApplication::translate("main", "cache"));
    parser.addOption(cache_size);

    if (!parser.parse(QGuiApplication::arguments())) {
        error = parser.errorText();
        return true;
    }

    if (parser.isSet(helpOption)) {
        parser.showHelp();
        return false;
    }

    if (parser.isSet(server)) {
        QString s = parser.value(server);
        QHostInfo info = QHostInfo::fromName(s);
        if (info.error() == QHostInfo::NoError) {
            config.server = s;
        } else {
            qDebug() << "Invalid map tile server URL: " << s;
        }
    }
    if (parser.isSet(format)) {
        config.format = parser.value(format);
    }
    if (parser.isSet(min_zoom)) {
        QVariant range(parser.value(min_zoom));
        config.min_zoom = range.toInt();
    }
    if (parser.isSet(max_zoom)) {
        QVariant range(parser.value(max_zoom));
        config.max_zoom = range.toInt();
    }
    if (parser.isSet(tile_size)) {
        QVariant range(parser.value(tile_size));
        config.tile_size = range.toInt();
    }
    if (parser.isSet(cache_size)) {
        QVariant range(parser.value(cache_size));
        config.cache_size = size_t(range.toInt());
    }
    return false;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("qtmapviewer");

    MapConfig config;
    
    // Default onfiguration for the map viewer.
    // See http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames for a list
    // of config options for servers and zoom levels.
    config.server = "http://a.tile.openstreetmap.org/";
    config.format = QString("png");
    // San Franciso, CA :)
    config.center = QVector2D(-122.20877392578124f, 37.65175620758778f);
    config.min_zoom = 0;
    config.max_zoom = 19; // max for most servers
    config.zoom_level = 10;
    config.map_size = QSize(1080, 720);
    config.tile_size = 256; // square tiles
    config.cache_size = 256u; // 512 tile cache

    QString error;
    if (parseCommandLine(config, error)) {
        qDebug(qPrintable(error));
        return -1;
    } else {
        // print out the map configuration at startup
        config.print();
    }

    MapViewer viewer(config);
    viewer.setTitle("qtmapviewer");
    viewer.show();

    return app.exec();
}
