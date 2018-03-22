#include "callListener.h"

CallListener::CallListener(MegaChatApi *megaChatApi, CallGui *callGui)
 : MegaChatVideoListener()
{    
    mMegaChatApi = megaChatApi;
    mCallGui = callGui;
    megaChatVideoListenerDelegate = new QTMegaChatVideoListener(mMegaChatApi, this);    
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

     char *copyBuff = new char[size];
     memcpy(copyBuff,buffer,size);
     unsigned char *auxBuf = reinterpret_cast<unsigned char*> (copyBuff);
     QImage *auxImg = new QImage(auxBuf, width, height, QImage::Format_RGBA8888, myImageCleanupHandler, auxBuf);
     if(auxImg->isNull())
     {
         return NULL;
     }
     return auxImg;
 }

 void CallListener::myImageCleanupHandler(void *info)
 {
     if (info)
     {
         delete info;
     }
 }
