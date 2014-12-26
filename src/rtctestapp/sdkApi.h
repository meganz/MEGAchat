#ifndef SDKAPI_H
#define SDKAPI_H
#include <megaapi.h>
#include "../base/promise.h"
#include "../base/gcmpp.h"
#include <iostream>

using namespace mega;
using namespace std;
using namespace promise;
typedef std::shared_ptr<MegaRequest> ReqResult;
typedef Promise<ReqResult> ApiPromise;

class AutoString
{
protected:
    const char* mBuf;
public:
    AutoString(const char* aBuf): mBuf(aBuf){}
    ~AutoString() {if (mBuf) delete mBuf;}
    const char* c_str() const {return mBuf;}
};

class MyListener: public MegaRequestListener
{
public:
    ApiPromise mPromise;
    virtual void onRequestFinish(MegaApi* api, MegaRequest *request, MegaError* e)
    {
        std::shared_ptr<MegaRequest> req(request->copy());
        int errCode = e->getErrorCode();
        mega::marshallCall([this, req, errCode]()
        {
            if (mPromise.done())
                return; //a timeout timer may resolve it before the actual callback
            if(errCode != MegaError::API_OK)
                mPromise.reject(errCode, 0x3e9aab10);
            else
                mPromise.resolve(req);
            delete this;
        });
    }
//Currently, this callback is only valid for the request fetchNodes()
virtual void onRequestUpdate(MegaApi*api, MegaRequest *request)
{}
virtual void onRequestTemporaryError(MegaApi *api, MegaRequest *request, MegaError* error)
{
cout << "***** Temporary error in request: " << error->getErrorString() << endl;
}
virtual void onTransferFinish(MegaApi* api, MegaTransfer *transfer, MegaError* error)
{}
virtual void onTransferUpdate(MegaApi *api, MegaTransfer *transfer)
{}
virtual void onTransferTemporaryError(MegaApi *api, MegaTransfer *transfer, MegaError* error)
{}
virtual void onUsersUpdate(MegaApi* api, MegaUserList *users)
{}
virtual void onNodesUpdate(MegaApi* api, MegaNodeList *nodes)
{}
};

class MyMegaApi: public MegaApi
{
public:
    MyListener mListener;
    MyMegaApi(const char *appKey)
        :MegaApi(appKey, (const char *)NULL, "Karere")
    {
        setLogLevel(MegaApi::LOG_LEVEL_INFO);
    }
    template <typename... Args, typename MSig=void(MegaApi::*)(Args..., MegaRequestListener*)>
    ApiPromise call(MSig method, Args... args)
    {
        auto listener = new MyListener;
        (this->*method)(args..., listener);
        return listener->mPromise;
    }
};

#endif // SDKAPI_H
