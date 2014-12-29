#ifndef SDKAPI_H
#define SDKAPI_H
#include <megaapi.h>
#include "../base/promise.h"
#include "../base/gcmpp.h"

typedef std::shared_ptr<mega::MegaRequest> ReqResult;
typedef promise::Promise<ReqResult> ApiPromise;

class SdkString
{
protected:
    const char* mBuf;
    mutable size_t mSize;
public:
    SdkString(const char* aBuf): mBuf(aBuf), mSize((size_t)-1){}
    ~SdkString() {if (mBuf) delete[] mBuf;}
    const char* c_str() const {return mBuf;}
    size_t size() const
    {
        if (mSize != (size_t)-1)
            return mSize;
        return (mSize = strlen(mBuf));
    }

};

class MyListener: public mega::MegaRequestListener
{
public:
    ApiPromise mPromise;
    virtual void onRequestFinish(mega::MegaApi* api, mega::MegaRequest *request, mega::MegaError* e)
    {
        std::shared_ptr<mega::MegaRequest> req(request->copy());
        int errCode = e->getErrorCode();
        mega::marshallCall([this, req, errCode]()
        {
            if (mPromise.done())
                return; //a timeout timer may resolve it before the actual callback
            if(errCode != mega::MegaError::API_OK)
                mPromise.reject(errCode, 0x3e9aab10);
            else
                mPromise.resolve(req);
            delete this;
        });
    }
};

class MyMegaApi: public mega::MegaApi
{
public:
    std::shared_ptr<mega::MegaRequest> userData;
    MyMegaApi(const char *appKey)
        :mega::MegaApi(appKey, (const char *)NULL, "Karere")
    {
        setLogLevel(MegaApi::LOG_LEVEL_INFO);
    }
    template <typename... Args, typename MSig=void(mega::MegaApi::*)(Args..., mega::MegaRequestListener*)>
    ApiPromise call(MSig method, Args... args)
    {
        auto listener = new MyListener;
        (this->*method)(args..., listener);
        return listener->mPromise;
    }
};

#endif // SDKAPI_H
