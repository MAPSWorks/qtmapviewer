#ifndef __GL_WORKER_H_
#define __GL_WORKER_H_

#include <QtGui/QWindow>
#include <QtGui/QOpenGLFunctions>
#include <QThread>
#include <QDebug>

// Worker class containing an OpenGL context. The thread member owns the GL
// context and also executes the GLWorker::QObject event loop. This allows 
// external objects to post GL-related events to the underlying QObject, 
// ensuring only the context thread make GL calls.
class QOpenGLContext;
class GLWorker : public QObject, 
                 protected QOpenGLFunctions 
{
    Q_OBJECT
public:
    GLWorker(QSurface* surface);
    // Constructor that uses OpenGL's shared context feature
    GLWorker(const GLWorker& shared);
    ~GLWorker();

    // Note: parent must call stop() before deleting. 
    // The general usage model is...
    // 1. create worker
    // 2. start()
    // 3. stop()
    // 4. delete worker

    // starts the worker thread
    void start();
    // stops the worker thread
    void stop();

private slots:
    // callbacks for the member thread 
    void begin();
    void end();

protected:
    // setup and shutdown can be implemented by derived classes
    // to execute GL-specific code inside the GL context thread
    virtual void setup() {};
    virtual void shutdown() {};
    QSurface* surface();
    QOpenGLContext* context();

private:
    void construct(QSurface* surface, 
        QOpenGLContext* shared = NULL);

    QSurface *m_surface;
    QOpenGLContext *m_context;
    QThread *m_thread, *m_parent;
};

#endif