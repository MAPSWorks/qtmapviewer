#include "TileFetcher.h"
#include <QtCore/QCoreApplication>
#include <QtGui/QOpenGLContext>
#include <iostream>
#include <QNetworkReply>
#include <cassert>

TileFetcher::TileFetcher(const MapConfig& config, const TileRenderer& renderer)
    : GLWorker(renderer), 
    m_network(new QNetworkAccessManager(this)),
    m_config(config)
{
    // connect the network manager finished signal to the slot 
    // that creates tile images
    connect(m_network, SIGNAL(finished(QNetworkReply*)), 
        this, SLOT(loadTile(QNetworkReply*)));
}

void TileFetcher::tileRequest(const TileIndex& tile)
{
    // Create the tile URL as the standard <server>/<zoom>/<x>/<y>.<format>
    QUrl url(m_config.server +
             QString::number(tile.zoom()) + QString("/") +
             QString::number(tile.x()) + QString("/") +
             QString::number(tile.y()) + QString(".") + m_config.format);

    QNetworkRequest request;
    // many map servers require a valid User-Agent header, so we 
    // just use the application name
    request.setRawHeader("User-Agent", "qtmapviewer");
    request.setUrl(url);

    QNetworkReply *reply = m_network->get(request);
    // connect the cancel requet signal to the reply abort slot so
    // all outstanding requests will terminate if cancelRequests() fires
    connect(this, SIGNAL(cancelRequests()), reply, SLOT(abort()));

    // track each reply so we can recover the tile index when the 
    // reply completes the image download
    assert(m_replies.find(reply) == m_replies.end());
    m_replies[reply] = tile;
}

void TileFetcher::loadTile(QNetworkReply* reply)
{
    // defer reply deletion to when the owner thread next
    // enters the object event loop
    reply->deleteLater();

    // recover the tile index from the reply
    TileReplyMap::const_iterator it = m_replies.find(reply);
    assert(it != m_replies.end());
    TileIndex index = it->second;
    m_replies.erase(it);

    TileImage *tile = NULL;
    if (QNetworkReply::NoError != reply->error()) {
        tile = new TileImage(index);//tile.valid = false;
        if (reply->error() != QNetworkReply::OperationCanceledError) {
            qCritical() << "Network error for request:" 
                << reply->request().url() << reply->error();
        }
    } else {
        QImage image;
        // Load the image directly from the reply payload bytes
        image.loadFromData(reply->readAll(), m_config.format.toLocal8Bit().data());
        assert(image.width() == m_config.tile_size);
        assert(image.height() == m_config.tile_size);

        tile = new TileImage(index, image);
        m_images[index] = tile;
    }
    // emit the tile to the TileRenderer
    emit responseTile(tile);
}

void TileFetcher::deleteTile(TileImage* tile)
{
    assert(tile);
    TileImageMap::iterator it = m_images.find(tile->index());

    assert(it != m_images.end());
    assert(tile == it->second);
    // first remove the tile image from the image map
    m_images.erase(it);
    delete tile;
}

void TileFetcher::shutdown()
{
    // Clean up all tile images in the shutdown callback. 
    for (TileImageMap::iterator it = m_images.begin(); 
        it != m_images.end(); it++) {
        assert(it->second);
        delete it->second;
    }
    m_images.clear();
}

