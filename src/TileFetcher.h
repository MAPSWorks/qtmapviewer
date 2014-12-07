#ifndef __TILE_FETCHER_H_
#define __TILE_FETCHER_H_

#include "TileRenderer.h"
#include "TileTypes.h"
#include "MapConfig.h"
#include <QNetworkAccessManager>

// This class manages fetching tile data from a remote server. It also
// owns all TileImage objects created by converting tile image data into
// OpenGL textures suitable for rendering in the TileRenderer. All 
// communiation between this class and a TileRenderer happens via signal/slot
// connections.
class TileFetcher : public GLWorker
{
    Q_OBJECT
public:
    TileFetcher(const MapConfig& config, const TileRenderer& renderer);

public slots:
    void tileRequest(const TileIndex& tile);
    void loadTile(QNetworkReply* reply);
    void deleteTile(TileImage* tile);

signals:
    void responseTile(TileImage* tile);
    void cancelRequests();

protected:
    void shutdown();

private:
    struct Config {
        Config(const MapConfig& config)
        : server(config.server),
        format(config.format),
        tile_size(config.tile_size) {}

        QString server;
        QString format;
        int tile_size;
    };

    typedef std::map<QNetworkReply*, TileIndex> TileReplyMap;
    typedef std::map<TileIndex, TileImage*> TileImageMap;

    // Qt network layer abstraction
    QNetworkAccessManager *m_network;

    TileReplyMap m_replies; // tracks network replies
    TileImageMap m_images;  // tracks allocated tile images
    Config m_config;        // store internal config state      
}; 

#endif
