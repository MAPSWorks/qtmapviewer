#ifndef __TILE_TYPES_H_
#define __TILE_TYPES_H_

#include <QOpenGLTexture>
#include <QThread>
#include <iostream>
#include <tuple>
#include <cassert>

// defines a tile by x,y coordinate and a zoom level
// the std::tuple allows ease key generation for 
// insert/query of the tile cache
class TileIndex : public std::tuple<int, int, int> {
public:
    TileIndex(): tuple(-1, -1, -1) {}
    TileIndex(int zoom, int x, int y): tuple(zoom, x, y) {}

    int zoom() const { return std::get<0>(*this); }
    int x() const {    return std::get<1>(*this); }
    int y() const {    return std::get<2>(*this); }

    // Format the tile index into a string for printing
    QString string() const {
        return QString("[") + 
             QString::number(zoom()) + 
             QString(",") +
             QString::number(x()) + 
             QString(",") +
             QString::number(y()) +
             QString("]");
    }
};

// This class represents a tile image in the OpenGL context. 
// The constructor takes a QImage and creates a QOpenGLTexture
// to hold the map tile image data. Object of this type can only 
// be created/destroyed by the TileFetcher, making it safe to pass
// around pointers that can be stored in the renderer's tile cache.
class TileImage {
public:
    QOpenGLTexture& texture() {
        return *m_texture;
    }
    bool valid() const {
        return (m_texture != NULL);
    }
    const TileIndex& index() const {
        return m_index;
    }
private:
    friend class TileFetcher;

    TileImage(const TileIndex& index)
        : m_index(index), 
        m_owner(QThread::currentThread()),
        m_texture(NULL) {}

    // Constructs a TileImage from the QImage, storing the current
    // QThread to make sure it makes on destruction
    TileImage(const TileIndex& index, const QImage& image)
        : m_index(index), 
        m_owner(QThread::currentThread()),
        m_texture(new QOpenGLTexture(image, QOpenGLTexture::DontGenerateMipMaps))
    {
        m_texture->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
    }

    ~TileImage() {
        assert(m_owner == QThread::currentThread());
        if (valid()) {
            delete m_texture;
            m_texture = NULL;
        }
    }

    TileIndex m_index;         // tile index for the image
    QThread *m_owner;          // thread that creates the image
    QOpenGLTexture *m_texture; // OpenGL texture for the image data
};

#endif
