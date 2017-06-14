#ifndef SDKAPI_H
#define SDKAPI_H

//the megaapi.h header needs this defined externally
#ifndef ENABLE_CHAT
    #define ENABLE_CHAT 1
#endif
#include <megaapi.h>
#include "base/promise.h"
#include "base/gcmpp.h"
#include <logger.h>
#include "karereCommon.h"

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

class MyListener: public mega::MegaRequestListener
{
public:
    ApiPromise mPromise;
    virtual void onRequestFinish(mega::MegaApi* api, mega::MegaRequest *request, mega::MegaError* e)
    {
        std::shared_ptr<::mega::MegaRequest> req(request->copy());
        int errCode = e->getErrorCode();
        karere::marshallCall([this, req, errCode]()
        {
            if (mPromise.done())
                return; //a timeout timer may resolve it before the actual callback
            if(errCode != mega::MegaError::API_OK)
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
        });
    }
};

class MyListenerNoResult: public ::mega::MegaRequestListener
{
public:
    promise::Promise<void> mPromise;
    virtual void onRequestFinish(mega::MegaApi* api, mega::MegaRequest *request, mega::MegaError* e)
    {
        int errCode = e->getErrorCode();
        karere::marshallCall([this, errCode]()
        {
            if (mPromise.done())
                return; //a timeout timer may resolve it before the actual callback
            if(errCode != mega::MegaError::API_OK)
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
        });
    }
};

class MyMegaLogger: public ::mega::MegaLogger
{
    virtual void log(const char *time, int loglevel, const char *source, const char *message)
    {
        static krLogLevel sdkToKarereLogLevels[mega::MegaApi::LOG_LEVEL_MAX+1] =
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

class MyMegaApi
{
public:
    ::mega::MegaApi& sdk;
    std::unique_ptr<MyMegaLogger> mLogger;    
    MyMegaApi(::mega::MegaApi& aSdk)
    :sdk(aSdk), mLogger(new MyMegaLogger)
    {
        sdk.addLoggerObject(mLogger.get());
    }
    template <typename... Args, typename MSig=void(::mega::MegaApi::*)(Args..., ::mega::MegaRequestListener*)>
    ApiPromise call(MSig method, Args... args)
    {
        auto listener = new MyListener;
        (sdk.*method)(args..., listener);
        return listener->mPromise;
    }
    template <typename... Args, typename MSig=void(::mega::MegaApi::*)(Args..., ::mega::MegaRequestListener*)>
    promise::Promise<void> callIgnoreResult(MSig method, Args... args)
    {
        auto listener = new MyListenerNoResult;
        (sdk.*method)(args..., listener);
        return listener->mPromise;
    }

    ~MyMegaApi()
    {
        sdk.removeLoggerObject(mLogger.get());
        mLogger.reset();
        KR_LOG_DEBUG("Deleted SDK logger");
    }
};

#endif // SDKAPI_H
