#ifndef SDKAPI_H
#define SDKAPI_H

//the megaapi.h header needs this defined externally
#ifndef ENABLE_CHAT
    #define ENABLE_CHAT 1
#endif
#include <megaapi.h>
#include "base/promise.h"
#include "base/gcmpp.h"
#include "base/trackDelete.h"
#include <logger.h>
#include <string.h>
#include "karereCommon.h" //for KR_LOG_DEBUG

typedef std::shared_ptr<::mega::MegaRequest> ReqResult;
typedef promise::Promise<ReqResult> ApiPromise;
enum {ERRTYPE_MEGASDK = 0x3e9aab10};

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
    operator bool() const { return mBuf != nullptr; }

};

class MyListener: public ::mega::MegaRequestListener
{
    void *appCtx;
    karere::DeleteTrackable::Handle wptr;
    
public:
    MyListener(void *ctx, karere::DeleteTrackable::Handle wptr) : appCtx(ctx), wptr(wptr) { }
    ApiPromise mPromise;
    virtual void onRequestFinish(::mega::MegaApi* /*api*/, ::mega::MegaRequest *request, ::mega::MegaError* e)
    {
        if (wptr.deleted())
            return;

        std::shared_ptr<::mega::MegaRequest> req(request->copy());
        int errCode = e->getErrorCode();
        karere::marshallCall([this, req, errCode]()
        {
            if (wptr.deleted())
                return;

            if (mPromise.done())
                return; //a timeout timer may resolve it before the actual callback

            if(errCode != ::mega::MegaError::API_OK)
            {
                std::string errmsg = "Mega API error ";
                errmsg.append(std::to_string(errCode)).append(" (")
                      .append(::mega::MegaError::getErrorString(errCode))+=')';
                mPromise.reject(errmsg, errCode, ERRTYPE_MEGASDK);
            }
            else
            {
                mPromise.resolve(req);
            }
            delete this;
        }, appCtx);
    }
};

class MyListenerNoResult: public ::mega::MegaRequestListener
{
    void *appCtx;
    karere::DeleteTrackable::Handle wptr;

public:
    MyListenerNoResult(void *ctx, karere::DeleteTrackable::Handle wptr) : appCtx(ctx), wptr(wptr) { }

    promise::Promise<void> mPromise;
    virtual void onRequestFinish(::mega::MegaApi* /*api*/, ::mega::MegaRequest * /*request*/, ::mega::MegaError* e)
    {
        int errCode = e->getErrorCode();
        karere::marshallCall([this, errCode]()
        {
            if (wptr.deleted())
                return;

            if (mPromise.done())
                return; //a timeout timer may resolve it before the actual callback

            if(errCode != ::mega::MegaError::API_OK)
            {
                std::string errmsg = "Mega API error ";
                errmsg.append(std::to_string(errCode)).append(" (")
                      .append(::mega::MegaError::getErrorString(errCode))+=')';
                mPromise.reject(errmsg, errCode, ERRTYPE_MEGASDK);
            }
            else
            {
                mPromise.resolve();
            }
            delete this;
        }, appCtx);
    }
};

class MyMegaLogger: public ::mega::MegaLogger
{
    virtual void log(const char * /*time*/, int loglevel, const char *source, const char *message)
    {
        static krLogLevel sdkToKarereLogLevels[::mega::MegaApi::LOG_LEVEL_MAX+1] =
        {
            krLogLevelError, krLogLevelError, krLogLevelWarn,
            krLogLevelInfo, krLogLevelDebug, krLogLevelDebugVerbose
        };
        std::string sourceFile;
        if (source)
        {
            std::string tmp = std::string(source);
            size_t start = tmp.rfind('/');
            if (start == std::string::npos)
            {
                start = tmp.rfind('\\');
            }

            if (start != std::string::npos)
            {
                sourceFile = "(" + tmp.substr(start+1) + ")";
            }
        }
        KARERE_LOG(krLogChannel_megasdk, sdkToKarereLogLevels[loglevel], "%s %s", message, sourceFile.c_str());
    }
};

class MyMegaApi: public karere::DeleteTrackable
{
public:
    ::mega::MegaApi& sdk;
    std::unique_ptr<MyMegaLogger> mLogger;
    void *appCtx;
    bool logging;
    
    MyMegaApi(::mega::MegaApi& aSdk, void *ctx, bool addlogger = true)
    :sdk(aSdk), mLogger(new MyMegaLogger), appCtx(ctx), logging(addlogger)
    {
        if (addlogger)
        {
            sdk.addLoggerObject(mLogger.get());
        }
    }
    template <typename... Args, typename MSig=void(::mega::MegaApi::*)(Args..., ::mega::MegaRequestListener*)>
    ApiPromise call(MSig method, Args... args)
    {
        auto listener = new MyListener(appCtx, getDelTracker());
        (sdk.*method)(args..., listener);
        return listener->mPromise;
    }
    template <typename... Args, typename MSig=void(::mega::MegaApi::*)(Args..., ::mega::MegaRequestListener*)>
    promise::Promise<void> callIgnoreResult(MSig method, Args... args)
    {
        auto listener = new MyListenerNoResult(appCtx, getDelTracker());
        (sdk.*method)(args..., listener);
        return listener->mPromise;
    }

    ~MyMegaApi()
    {
        if (logging)
        {
            sdk.removeLoggerObject(mLogger.get());
        }
        mLogger.reset();
        KR_LOG_DEBUG("Deleted SDK logger");
    }
};

#endif // SDKAPI_H
