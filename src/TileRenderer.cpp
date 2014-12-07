#include "TileRenderer.h"
#include <QtCore/QCoreApplication>
#include <QtGui/QOpenGLContext>
#include <QMatrix4x4>
#include <iostream>

// Simple vertex shader used to position map tiles on the render target
const static char VertexShader[] =
    "#version 430\n"
	"layout (location = 0) in vec2 tile;" // tile geometry
	"uniform mat4 projection;" 
    "uniform vec2 scale;"      // tile geometry scale
	"uniform vec2 offset;"     // tile geometry offset
    "uniform vec2 tex_scale;"  // tile texture scale
    "uniform vec2 tex_offset;" // tile texture offset
    "uniform vec2 size;"       // tile size in pixels
    "out vec2 texcoord;"
    "void main() {"
        // scale the tile coord to be in the texture [0,1] range
        "texcoord = tile / size;"
        // scale and offset the texcoord to match the texture subregion
        "texcoord = tex_scale * texcoord + tex_offset;"
        // sacle and offset the tile position to match the map location
        "gl_Position = projection * vec4(scale * tile + offset, 0, 1);"
    "}";

// Simple fragment shader used to fetch tile image texture data
const static char FragmentShader[] =
    "#version 430\n"
    "layout(location = 0) out vec4 out_color;"
	"uniform sampler2D tile;"
	"in vec2 texcoord;" // texture coordinate from vertex shader
	"void main() {"
        "out_color = texture(tile, texcoord);"
	"}";

// Register the render event with Qt
const QEvent::Type TileRenderer::RenderRequest::type = (QEvent::Type)QEvent::registerEventType();

TileRenderer::TileRenderer(const MapConfig& config, QSurface* surface)
    : GLWorker(surface), 
    m_config(config),
    m_shader(NULL),
    m_render_requests(0),
    m_cache(m_config.cache_size, std::bind(&TileRenderer::tileEvicted, this, std::placeholders::_1))
{
}

void TileRenderer::render() 
{
    State state = getState();
    // Make sure the render state is valid before performing any draw calls.
    // In general, the state won't be valid until the QWindow surface gets a 
    // resize event.
    if (!state.valid()) { 
        return; 
    }

    // makeCurrent is required even though render() is only ever called by the 
    // thread that owns the GL context. This is a quirk of the implementation 
    // inside Qt.
    context()->makeCurrent(surface());
    const QSize& size = state.mapSize();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, context()->defaultFramebufferObject());
    glClearColor(0.85f,0.85f,0.85f,1);
    glViewport(0, 0, size.width(), size.height());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    QMatrix4x4 projection;
    projection.setToIdentity();
    // Perform all draw calls with a 2D orthographic projection in raster space
    projection.ortho(QRect(0, 0, size.width(), size.height()));
 
    // Safe to use static vectors because only the GL context thread enters
    static std::vector<TileDrawable> tiles;
    static std::vector<TileIndex> requests;
    tiles.clear();
    requests.clear();

    getTiles(state, tiles, requests);
    // Loop over the tile request list (missing from the cache) and 
    // update the request map. If the request index isn't already in the map
    // (which means there is an outstanding request for this tile), emit a
    // tile request for the index.
    for (size_t i = 0; i < requests.size(); i++) {
        std::pair<TileRequestMap::iterator, bool> it = 
            m_requests.insert(std::make_pair(requests[i], true));
        if (it.second) {
            emit requestTile(requests[i]);
        }
    }

    // This is render code in all its trivial glory :)
    float tile_size = float(m_config.tile_size);
    GLfloat tile_quad[8] = {0.f, 0.f, 0.f, tile_size, tile_size, 0.f, tile_size, tile_size};
    m_shader->bind();
	m_shader->setAttributeArray("tile", GL_FLOAT, tile_quad, 2);
	m_shader->enableAttributeArray("tile");
    m_shader->setUniformValue("size", QVector2D(tile_size, tile_size));
    m_shader->setUniformValue("projection", projection);

    // Render each tile in the visible list. Note that some TileDrawables 
    // point to TileImage objects at a zoom level above or below the 
    // current zoom. For these the scale/offset parameters ensure the raster
    // is properly sized and maps the correct (sub)region of the tile texture.
    for (size_t i = 0; i < tiles.size(); i++) {
        tiles[i].image->texture().bind(0);
        m_shader->setUniformValue("scale", tiles[i].scale);
        m_shader->setUniformValue("offset", tiles[i].offset);
        m_shader->setUniformValue("tex_scale", tiles[i].tex_scale);
        m_shader->setUniformValue("tex_offset", tiles[i].tex_offset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); 
        tiles[i].image->texture().release();
    }
    m_shader->release();

    // Manual swap buffers is necessary for QWindow surfaces
    context()->swapBuffers(surface());
}

void TileRenderer::setState(const State& state) {
    m_mutex.lock();
    m_state = state;
    if (m_render_requests == 0) {
        m_render_requests++;
        // post a render request corresponding to this state update
        QCoreApplication::postEvent(this, new TileRenderer::RenderRequest());
    }
    m_mutex.unlock();
}

TileRenderer::State TileRenderer::getState() {
    State state;
    m_mutex.lock();
    state = m_state;
    if (m_render_requests > 0) {
        m_render_requests--;
        // If there are outstanding render requests (possibly due to map tile
        // updates) post another render request to the event queue.
        if (m_render_requests > 0) {
            QCoreApplication::postEvent(this, new TileRenderer::RenderRequest());
        }
    }
    m_mutex.unlock();
    return state;
}

void TileRenderer::tileEvicted(TileImage* tile) {
    // emit a request for the TileFetcher to delete the tile image
    emit deleteTile(tile);
}

// This method generates a list of visible map tiles for the given state. It simply
// computes what tiles are inside the current pixel space bounds and queries the tile
// cache for each index. If the tile is in the cache, it goes into a TileDrawable object
// to be immidediately rendered. If not, the index goes into the the 'requests' vector
// to be requested from the TileFetcher.
void TileRenderer::getTiles(const State& state, std::vector<TileDrawable>& tiles, 
        std::vector<TileIndex>& requests)
{
    int size = m_config.tile_size;
    int pixels = int(pow(2.0, state.zoom())); // note we assume a 32bit limit here

    int x1, y1, x2, y2;
    // get the four corners of the pixel space map bounds
    state.bounds().getCoords(&x1, &y1, &x2, &y2);

    // computes offsets relative to an orthographic projection starting at 0,0
    // The modulo operator simply gives the portion of the top left tile that
    // is out of view in the current map viewport
    int xoffset = -(x1 % size);
    int yoffset = -(y1 % size);

    // This logic implements longitudinal wrapping
    int x1_shift = ((x1 < 0)?1:0);
    int x2_shift = ((x2 < 0)?1:0);
    int x_shift = ((x1 < 0)?size:0);

    x1 /= size; x2 /= size; y1 /= size; y2 /= size;
    x1 -= x1_shift;
    x2 -= x2_shift;
    xoffset -= x_shift;
  
    // rasterize the quad of visible map tiles in x and y
    int yy = 0;
    for (int y = y1; y <= y2; y++) {
        int xx = 0;
        for (int x = x1; x <= x2; x++) {
            int wx = x;
            // More nasty logic for logitudinal wrapping. There is probably
            // a much clearner way to do this, but whatever...
            if (wx < 0) {
                while (wx < 0) { wx += pixels; }
            } else if (wx >= pixels) {
                wx %= pixels;
            }
            if (y >= 0 && y < pixels) {
                TileIndex index(state.zoom(), wx, y);
                TileImage* image;
                if (m_cache.query(index, image)) {
                    
                    TileDrawable tile;
                    tile.scale = QVector2D(1.f,1.f);
                    tile.offset = QVector2D(xoffset + xx * size, yoffset + yy * size);
                    tile.image = image;
                    tile.tex_scale = QVector2D(1.f, 1.f);
                    tile.tex_offset = QVector2D(0.f,0.f);
                    tiles.push_back(tile);
                } else {
                    if (state.zoomedIn()) {
                        TileIndex index(state.zoom() - 1, x/2, y/2);
                        TileImage* image;
                        if (m_cache.query(index, image)) {
                            TileDrawable tile;
                            tile.scale = QVector2D(1.f,1.f);
                            tile.offset = QVector2D(xoffset + xx * size, yoffset + yy * size);
                            tile.image = image;
                            tile.tex_scale = QVector2D(0.5f, 0.5f);
                            tile.tex_offset = QVector2D(0.5f * (x%2), 0.5f * (y%2));
                            tiles.push_back(tile);
                        }
                    } else if (state.zoomedOut()) {
                        TileIndex indexTL(state.zoom() + 1, wx*2, y*2);
                        TileIndex indexTR(state.zoom() + 1, wx*2+1, y*2);
                        TileIndex indexBR(state.zoom() + 1, wx*2+1, y*2+1);
                        TileIndex indexBL(state.zoom() + 1, wx*2, y*2+1);
                        TileImage* image;
                        if (m_cache.query(indexTL, image)) {
                            TileDrawable tile;
                            tile.scale = QVector2D(0.5f,0.5f);
                            tile.offset = QVector2D(xoffset + xx * size, yoffset + yy * size);
                            tile.image = image;
                            tile.tex_scale = QVector2D(1.f, 1.f);
                            tile.tex_offset = QVector2D(0.f, 0.f);
                            tiles.push_back(tile);
                        }
                        if (m_cache.query(indexTR, image)) {
                            TileDrawable tile;
                            tile.scale = QVector2D(0.5f,0.5f);
                            tile.offset = QVector2D(xoffset + xx * size + size / 2, yoffset + yy * size);
                            tile.image = image;
                            tile.tex_scale = QVector2D(1.f, 1.f);
                            tile.tex_offset = QVector2D(0.f, 0.f);
                            tiles.push_back(tile);
                        }
                        if (m_cache.query(indexBR, image)) {
                            TileDrawable tile;
                            tile.scale = QVector2D(0.5f,0.5f);
                            tile.offset = QVector2D(xoffset + xx * size + size / 2, yoffset + yy * size + size / 2);
                            tile.image = image;
                            tile.tex_scale = QVector2D(1.f, 1.f);
                            tile.tex_offset = QVector2D(0.f, 0.f);
                            tiles.push_back(tile);
                        }
                        if (m_cache.query(indexBL, image)) {
                            TileDrawable tile;
                            tile.scale = QVector2D(0.5f,0.5f);
                            tile.offset = QVector2D(xoffset + xx * size, yoffset + yy * size + size / 2);
                            tile.image = image;
                            tile.tex_scale = QVector2D(1.f, 1.f);
                            tile.tex_offset = QVector2D(0.f, 0.f);
                            tiles.push_back(tile);
                        }
                    }
                    requests.push_back(index);
                }
            }
            xx++;
        } // x raster
        yy++;
    } // y raster
}

void TileRenderer::tileResponse(TileImage* tile)
{
    m_requests.erase(tile->index());

    if (tile->valid()) {
        m_cache.insert(tile->index(), tile);
        m_mutex.lock();
        if (!m_render_requests) {
            m_render_requests++;
            QCoreApplication::postEvent(this, new TileRenderer::RenderRequest());
        }
        m_mutex.unlock();
    }
}

void TileRenderer::setup()
{
    m_shader = new QGLShaderProgram;
	if (!m_shader->addShaderFromSourceCode(QGLShader::Vertex, std::string(VertexShader).c_str())) {
		qWarning() << "Vertex shader compile error: " << m_shader->log();
	}
	if (!m_shader->addShaderFromSourceCode(QGLShader::Fragment, std::string(FragmentShader).c_str())) {
		qWarning() << "Fragment shader compile error: " << m_shader->log();
	}
	if (!m_shader->link()) {
		qWarning() << "Shader program link error: " << m_shader->log();
	}
}

void TileRenderer::shutdown()
{
    m_shader->removeAllShaders();
    delete m_shader;
    m_shader = NULL;
}

void TileRenderer::customEvent(QEvent *event) 
{
    // Handle custom internal event type to trigger a render call
    if (event->type() == RenderRequest::type) {
        render();
    } else {
        QObject::event(event);
    }     
}

