#include "callListener.h"

CallListener::CallListener(MegaChatApi *megaChatApi, CallGui *callGui, MegaChatHandle peerid)
 : MegaChatVideoListener()
{    
    mMegaChatApi = megaChatApi;
    mCallGui = callGui;
    megaChatVideoListenerDelegate = new QTMegaChatVideoListener(mMegaChatApi, this);
    mPeerid = peerid;
}
 CallListener::~ CallListener()
 {
     delete megaChatVideoListenerDelegate;     
 }

 QImage *CallListener::CreateFrame(int width, int height, char *buffer, size_t size)
 {
     if((width == 0) || (height == 0))
     {
         return NULL;
     }

     unsigned char *copyBuff = new unsigned char[size];
     memcpy(copyBuff, buffer, size);
     QImage *auxImg = new QImage(copyBuff, width, height, QImage::Format_RGBA8888, myImageCleanupHandler, copyBuff);
     if(auxImg->isNull())
     {
         delete [] copyBuff;
         return NULL;
     }
     return auxImg;
 }

 void CallListener::myImageCleanupHandler(void *info)
 {
     if (info)
     {
         unsigned char *auxBuf = reinterpret_cast<unsigned char*> (info);
         delete [] auxBuf;
     }
 }
