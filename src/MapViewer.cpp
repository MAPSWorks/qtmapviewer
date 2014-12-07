#include "MapViewer.h"
#include "TileTypes.h"
#include <QtCore/QCoreApplication>
#include <QtGui/QOpenGLContext>
#include <QMouseEvent>
#include <QWheelEvent>
#include <iostream>
#define _USE_MATH_DEFINES
#include <math.h>

Q_DECLARE_METATYPE(TileIndex);

MapViewer::MapViewer(const MapConfig& config, QWindow *parent)
    : QWindow(parent), 
      m_renderer(NULL), 
      m_fetcher(NULL),
      m_mouse_pressed(false),
      m_config(config)
{
    // Must register value types with Qt to use in signal/slots
    qRegisterMetaType<TileIndex>();

    // Use an OpenGL surface and window backing memory, enabling 
    // GPU rendering of the map
    setSurfaceType(QWindow::OpenGLSurface);
    // Initialize the map center in pixel coordinates
    m_map_center = latlonToPixel(m_config.zoom_level, config.center);
    m_render_state.setZoom(m_config.zoom_level);

    setWidth(config.map_size.width());
    setHeight(config.map_size.height());

    // Use a default surface format
    setFormat(QSurfaceFormat());
}

MapViewer::~MapViewer()
{
    if (m_renderer) {
        // must stop the worker thread before destrution
        m_renderer->stop();
        delete m_renderer;
        m_renderer = NULL;
    }
    if (m_fetcher) {
        // must stop the worker thread before destrution
        m_fetcher->stop();
        delete m_fetcher;
        m_fetcher = NULL;
    }
}

void MapViewer::initialize()
{
    if (!m_renderer && !m_fetcher) {
        // Here the TileRenderer is constructed first to target the MapViewer Qwindow 
        // surface, and the TileFetcher is construted to share the same GL context
        // See the GLWorker constructors for details
        m_renderer = new TileRenderer(m_config, this);
        m_fetcher = new TileFetcher(m_config, *m_renderer);

        // connect the renderer tile request signal to the fetcher tile request slot
        connect(m_renderer, SIGNAL(requestTile(const TileIndex&)), 
            m_fetcher, SLOT(tileRequest(const TileIndex&)));
        // connect the tile fetcher response signal to the tile renderer reposnse slot
        connect(m_fetcher, SIGNAL(responseTile(TileImage*)), 
            m_renderer, SLOT(tileResponse(TileImage*)));
        // connect the signal to cancel outstanding tile requests to the fetcher slot
        connect(this, SIGNAL(cancelRequests()), m_fetcher, SIGNAL(cancelRequests()));
        // connect the renderer delete tile signal to the tile fetcher slot
        connect(m_renderer, SIGNAL(deleteTile(TileImage*)), 
            m_fetcher, SLOT(deleteTile(TileImage*)));

        // start the worker threads for these objects
        m_renderer->start();
        m_fetcher->start();
    }
}

bool MapViewer::event(QEvent *event)
{
    switch (event->type()) {
        case QEvent::UpdateRequest:
            // make sure the MapViewer is initialized on the first update request
            initialize();
            return true;
        default:
            return QWindow::event(event);
    }
}

void MapViewer::resizeEvent(QResizeEvent *event) {
    const QSize& size = event->size();

    // Compute the map bounds in pixel space based on the new resize
    QRect bounds(m_map_center.x() - size.width() / 2, 
                 m_map_center.y() - size.height() / 2, size.width(), size.height());

    m_render_state.setBounds(bounds);
    m_render_state.setMapSize(size);
    m_render_state.setValid();
}

void MapViewer::mousePressEvent(QMouseEvent * event) 
{
    m_mouse_anchor = event->pos();
    // QWindow doesn't automatically filter mouse events, so we have
    // to manually track mouse press for correct move handling
    m_mouse_pressed = true;
}

void MapViewer::mouseMoveEvent(QMouseEvent * event) 
{
   if (m_mouse_pressed) {
       // For a mouse move event we need to translate the view of the map.
       // This is done by computing the pixel offset vector and adding it
       // to the map center coordinate. We then recompute the map bounds in
       // pixel space and update the render state with the new values.
       QPoint diff = m_mouse_anchor - event->pos();
       m_map_center += diff;
       m_mouse_anchor = event->pos();

       const QSize& size = m_render_state.mapSize();
       QRect bounds(m_map_center.x() - size.width() / 2, 
                    m_map_center.y() - size.height() / 2, size.width(), size.height());

       m_render_state.setBounds(bounds);
       m_renderer->setState(m_render_state);

       m_mouse_anchor = event->pos();
   } 
}

void MapViewer::mouseReleaseEvent(QMouseEvent * event) 
{
    m_mouse_anchor = event->pos();
    m_mouse_pressed = false;
}

void MapViewer::mouseDoubleClickEvent(QMouseEvent * event)
{
    const QSize& size = m_render_state.mapSize();
    m_map_center += (event->pos() - QPoint(size.width() / 2, size.height() / 2));

    // Here a left button double click zoom in, a right button zooms out
    if (event->buttons() & Qt::LeftButton) {
        // Zoom in
        m_render_state.setZoom(std::min(m_render_state.zoom() + 1, m_config.max_zoom));
        if (m_render_state.zoomedIn()) {
            // only scale the map center coordinate if we zoomed in
            m_map_center = QPoint(m_map_center.x() * 2, m_map_center.y() * 2);
        }
    } else if (event->buttons() & Qt::RightButton) {
        // Zoom out
        m_render_state.setZoom(std::max(m_render_state.zoom() - 1, m_config.min_zoom));
        if (m_render_state.zoomedOut()) {
            // only scale the map center coordinate if we zoomed out
            m_map_center = QPoint(m_map_center.x() / 2, m_map_center.y() / 2);
        }
    }
    // emit a signal to cancel outstanding tile requests since we are moving to a new
    // zoom level in the map. This allows the network layer to immediately start 
    // downloading map tiles for the current level.
    emit cancelRequests();

    // A zoom operation changes the map center in pixel coordinates because the 
    // center is defined within the pixel spae of a specific zoom level
    QRect bounds(m_map_center.x() - size.width() / 2, 
                 m_map_center.y() - size.height() / 2, size.width(), size.height());

    m_render_state.setBounds(bounds);
    m_renderer->setState(m_render_state);
}

void MapViewer::keyPressEvent(QKeyEvent *event)
{
    // Wire up the escape key to exit the application
    if (event->key() == Qt::Key_Escape) {
        close();
    };
}

void MapViewer::exposeEvent(QExposeEvent *event)
{
    Q_UNUSED(event);
    if (isExposed()) {
        // Make sure the viewer is initialized when the window is exposed
        initialize();
        m_renderer->setState(m_render_state);
    }
}

// Converts from a latitude/longitude value to a pixel coordinate for a 
// given zoom level. See http://en.wikipedia.org/wiki/Mercator_projection
QPoint MapViewer::latlonToPixel(int zoom, const QVector2D& v) {
    double tile_size = double(m_config.tile_size);
    double to_rad = M_PI / 180.0;
    double z = pow(2.0, zoom);
    double x = (v.x() + 180.) / 360. * z * tile_size;
    double y = 0.5 * (1. - log(tan(to_rad * v.y()) + 1.0 / cos(to_rad * v.y())) / M_PI) * z * tile_size;
    return QPoint(int(x), int(y));
}

// Converts from a pixel coordinate at a zoom level to a latitude/longitude value
QVector2D MapViewer::pixelToLatlon(int zoom, const QPoint& v) {
    double tile_size = double(m_config.tile_size);
    double to_deg = 180.0 / M_PI;
    double z = pow(2.0, zoom);
    double lon = 360. * v.x() / z - 180.;
    double lat = to_deg * atan(sinh(M_PI - 2. * M_PI * v.y() / z));
    return QVector2D(lon / tile_size, lat / tile_size);
}



