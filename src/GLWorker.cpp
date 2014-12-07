#include "GLWorker.h"
#include <QtCore/QCoreApplication>
#include <QtGui/QOpenGLContext>
#include <iostream>
#include <cassert>

GLWorker::GLWorker(QSurface* surface)
    : m_surface(NULL),
      m_context(NULL),
      m_thread(NULL), 
      m_parent(NULL)
{
    // initialize using only the input surface
    construct(surface);
}

GLWorker::GLWorker(const GLWorker& shared)
    : m_surface(NULL),
      m_context(NULL),
      m_thread(NULL), 
      m_parent(NULL)
{
    // initialize using the shared object surface and GL context
    construct(shared.m_surface, shared.m_context);
}

GLWorker::~GLWorker()
{
    // make sure the parent thread is the object owner
    assert(m_parent == QThread::currentThread());
    delete m_thread;
    m_thread = NULL;
    delete m_context;
    m_context = NULL;
}

QSurface* GLWorker::surface() 
{
    return m_surface;
}

QOpenGLContext* GLWorker::context() 
{
    return m_context;
}

void GLWorker::start()
{
    // store calling thread handle
    m_parent = QThread::currentThread();
    // release context from current thread
    m_context->doneCurrent();
    // move context to worker thread
    m_context->moveToThread(m_thread);
    // move worker object to worker thread
    moveToThread(m_thread);
    // start worker thread
    m_thread->start();
}

void GLWorker::stop()
{
    assert(m_parent == QThread::currentThread());
    // signal worker thread to exit
    m_thread->quit();
    // wait for worker thread
    m_thread->wait();
    // return context ownership to parent thread
    m_context->makeCurrent(m_surface);
}

void GLWorker::begin()
{
    m_context->makeCurrent(m_surface);
    // initialize the cross-platform GL functions
    initializeOpenGLFunctions();
    // call the subclass virtual setup function
    setup();
}

void GLWorker::end()
{
    // call subclass virtual shutdown function
    shutdown();
    // release context from worker thread
    m_context->doneCurrent();
    // move context from worker thread to parent thread
    m_context->moveToThread(m_parent);
}


void GLWorker::construct(QSurface* surface, QOpenGLContext* shared)
{
    // create GL context and worker thread objects
    m_context = new QOpenGLContext();
    m_context->setFormat(surface->format());
    m_surface = surface;
    if (shared) {
        m_context->setShareContext(shared);
    }
    m_context->create();
    m_thread = new QThread();
    // connect thread started signal to begin slot
    connect(m_thread, SIGNAL(started()), this, SLOT(begin()));
    // connect thread finished signal to end slot
    connect(m_thread, SIGNAL(finished()), this, SLOT(end()));
}

