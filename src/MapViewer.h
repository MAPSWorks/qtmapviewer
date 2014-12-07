#ifndef __TILE_VIEWER_H_
#define __TILE_VIEWER_H_

#include <QtGui/QWindow>
#include <QThread>
#include <QDebug>
#include <QVector2D>
#include "TileRenderer.h"
#include "TileFetcher.h"
#include "MapConfig.h"

// Main map viewer class. QWindow represents a window in the underlying
// system, so MapViewer inherits from QWindow and implements map-specific
// controls like pan/zoom. This class owns the tile fetcher and tile renderer
// used to display the map on the QWindow surface. The TileFetcher and 
// TileRenderer operated completely asynchronously, which allows simultaneous
// texture creation/upload and map rendering.
class MapViewer : public QWindow
{
    Q_OBJECT
public:
    MapViewer(const MapConfig& config, QWindow *parent = 0);
    ~MapViewer();

protected:
    // QWindow event handlers
    bool event(QEvent *event);
    void exposeEvent(QExposeEvent *event);
    void resizeEvent(QResizeEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event); 
    void mouseReleaseEvent(QMouseEvent *event); 
    void mouseDoubleClickEvent(QMouseEvent *event); 
    void keyPressEvent(QKeyEvent *event);

signals:
    void cancelRequests();

private:
    void initialize();

    // see http://en.wikipedia.org/wiki/Mercator_projection for details
    // on the mercator projection used in most map tiling systems
    QPoint latlonToPixel(int zoom, const QVector2D& coord);
    QVector2D pixelToLatlon(int zoom, const QPoint& v);

    TileRenderer *m_renderer; // renders map tiles
    TileFetcher *m_fetcher;   // fetches map tiles

    bool m_mouse_pressed;
    QPoint m_mouse_anchor;
    TileRenderer::State m_render_state;
    QPoint m_map_center;
    MapConfig m_config;
};

#endif