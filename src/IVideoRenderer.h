#ifndef IVIDEORENDERER_H
#define IVIDEORENDERER_H

class IVideoRenderer
{
public:
    virtual unsigned char* getImageBuffer(int bufSize, int width, int height, void** userData) = 0;
    virtual void frameComplete(void* userData) = 0;
    virtual void onStreamAttach() {};
    virtual void onStreamDetach() {};
};

#endif // IVIDEORENDERER_H
