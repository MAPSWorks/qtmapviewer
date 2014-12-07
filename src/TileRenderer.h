#ifndef __TILE_RENDERER_H_
#define __TILE_RENDERER_H_

#include "GLWorker.h"
#include "TileTypes.h"
#include "TileCache.h"
#include "MapConfig.h"
#include <QOpenGLTexture>
#include <QVector2D>
#include <QGLShaderProgram>
#include <QMutex>

// This class implements a basic map tile rendering engine.
class TileRenderer : public GLWorker
{
    Q_OBJECT
public:
    // This class defines the state used to control the tile renderer.
    // The MapViewer modifies a local copy of this state and then calls
    // setState() to update the renderer. 
    class State {
    public:
        State(): m_valid(false), m_last_zoom(-1) {}
        void setValid() {
            m_valid = true;
        }
        void setBounds(const QRect& bounds) {
            m_map_bounds = bounds;
        }
        void setZoom(int zoom) {
            if (m_last_zoom == -1) {
                m_last_zoom = zoom;
            } else {
                m_last_zoom = m_zoom;
            }
            m_zoom = zoom;
        }
        void setMapSize(const QSize& size) {
            m_map_size = size;
        }
        bool valid() const {
            return m_valid;
        }
        const QRect& bounds() const {
            return m_map_bounds;
        }
        int zoom() const {
            return m_zoom;
        }
        bool zoomedIn() const {
            return (m_zoom > m_last_zoom);
        }
        bool zoomedOut() const {
            return (m_zoom < m_last_zoom);
        }
        const QSize& mapSize() const {
            return m_map_size;
        }
    private:
        bool m_valid;
        QRect m_map_bounds;
        int m_zoom;
        int m_last_zoom;
        QSize m_map_size;
    };

    TileRenderer(const MapConfig& config, QSurface* surface);

    void setState(const State& state);

public slots:
    void tileResponse(TileImage* tile);

signals:
    void requestTile(const TileIndex& tile);
    void deleteTile(TileImage* tile);
    void cancelRequests();

protected:
    void customEvent(QEvent *event);
    void setup();
    void shutdown();

private:
    // Render request event type used by the MapViewer to post render
    // requests to the TileRenderer QObject event queue
    class RenderRequest : public QEvent {
    public:
        RenderRequest(): QEvent(type) {}
        static const QEvent::Type type;
    };

    struct Config {
        Config(const MapConfig& config)
        : tile_size(config.tile_size),
        cache_size(config.cache_size) {}

        int tile_size;
        size_t cache_size;
    };

    State getState();

    struct TileDrawable {
        QVector2D scale, offset;
        QVector2D tex_scale, tex_offset;
        TileImage *image;
    };
    typedef std::map<TileIndex, bool> TileRequestMap;

    void render();
    void tileEvicted(TileImage* tile);
    // this method generates a list of visible map tiles for the given state
    void getTiles(const State& state, std::vector<TileDrawable>& tiles, 
        std::vector<TileIndex>& requests);

    Config m_config;
    State m_state;

    TileCache m_cache;
    TileRequestMap m_requests;
    QGLShaderProgram *m_shader;

    // Used for protecting the render state
    QMutex m_mutex;
    size_t m_render_requests;
};

#endif
