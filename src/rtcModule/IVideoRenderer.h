#ifndef IVIDEORENDERER_H
#define IVIDEORENDERER_H
namespace rtcModule
{
/**
 * @brief This is the interface that is used to pass frames from the webrtc module to the
 * application for rendering in the GUI, or other purposes. For each frame, getImageBuffer()
 * is called, then image is written to that buffer in 32bit ARGB format, and frameComplete()
 * is called, signalling to the application that the frame is written to the buffer.
 * A user pointer can be provided by the application to associate the two calls. It could
 * be just the raw buffer pointer, so that \c frameComplete() can use and free it, or
 * \c getImageBuffer() can allocate a high-level bitmap object and return
 * its internal image buffer. In that case the application needs to also associate a pointer
 * to that bitmap object (rather than to the raw memory) so that it can use and free
 * the high-level bitmap object properly.
 */
class IVideoRenderer
{
public:
    /**
     * @brief getImageBuffer Called by _a worker thread_ to get a buffer where to write
     * frame image data. The size of the buffer must be width*height*4. The image is
     * written in ARGB format, with 4 bytes per pixel.
     * @param width The width of the frame
     * @param height The height of the frame
     * @param userData The user can return any void* via this parameter, and it will be
     * passed to \c frameComplete()
     * @return A pointer to the frame buffer where the frame will be written
     */
    virtual void* getImageBuffer(unsigned short width, unsigned short height, int sourceType, void*& userData) = 0;

    /**
     * @brief frameComplete Called _by a worker thread_ after a call to \c getImageBuffer()
     * when the buffer is populated with a frame image. It is safe to free the buffer
     * in this call or after it.
     * @param userData The user pointer provided to \c getImageBuffer()
     */
    virtual void frameComplete(void* userData) = 0;

    /**
     * @brief onVideoAttach Called when a video stream is attached to the player component
     * Frames can be expected after that point
     */
    virtual void onVideoAttach() {}

    /**
     * @brief onVideoDetach Called when the video stream is detached from the player
     * component.
     */
    virtual void onVideoDetach() {}

    /**
     * @brief clearViewport
     * Optionally implement this to clear the video viewport, i.e. set it to black when
     * there is no video stream or playback is stopped
     */
    virtual void clearViewport() {}

    /** Called _by a worker thread_ when the renderer is not needed and not referenced
     * anymore, so that it can be destroyed, even from within this callback.
     * \attention Although the renderer is no longer referenced from the library,
     * frame update messages may still be in the application's message queue (in case
     * such is used for posting frames to the GUI). These messages may reference the
     * renderer. To solve this, the app may want to post the destroy call via
     * application's message queue, assuming that the same message queue is used
     * to post frames to the GUI thread. This is usually the case, as a GUI application normally has only
     * one, ordered, message queue.
     * However, if this is not the case, it is the developer's responsibility
     * to destroy the renderer safely.
     */
    virtual void released() {}
    virtual ~IVideoRenderer() {}
};
class NullRenderer: public IVideoRenderer
{
public:
    virtual void* getImageBuffer(unsigned short /*width*/, unsigned short /*height*/, int /*sourceType*/, void*& /*userData*/)
    { return nullptr; }
    virtual void frameComplete(void* /*userp*/) {}
    virtual void released() { delete this; } //we don't post frames to the GUI so no problem with deleting immediately
};
}
#endif // IVIDEORENDERER_H
