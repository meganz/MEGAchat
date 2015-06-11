#ifndef _PROMISE_H
#define _PROMISE_H
#include <stdexcept>
#include <vector>
#include <string>
#include <utility>
#include <memory>
#include <assert.h>

namespace promise
{
#define PROMISE_LOG(fmtString,...) printf("promise: " fmtString"\n", ##__VA_ARGS__)
#ifdef PROMISE_DEBUG_REFS
    #define PROMISE_LOG_REF(fmtString,...) PROMISE_LOG(fmtString, ##__VA_ARGS__)
#else
    #define PROMISE_LOG_REF(fmtString,...)
#endif

static const char* kNoMoreCallbacksMsg =
  "No more space for promise callbacks, please increase the N template argument";

template <bool B, typename T>
struct EnableIf
{};

template <typename T>
struct EnableIf<true, T>
{typedef T type;};

#define PROMISE_ENABLE_IF(cond, t) \
typename EnableIf<cond, t>::type

template <typename A, typename B>
struct NotSame{enum {value=true};};
template <typename A>
struct NotSame<A,A>{enum {value=false};};

//Promise error class. We need it to be a refcounted object, because
//often the user would return somehting like return PromiseError(...)
//That would be converted to a failed promise object, and the Error would
//need to be copied into the Promise. By having a refcounted dynamic object,
//only the pointer will be transferred and the refcount changed.
struct ErrorShared
{
    std::string mMsg;
    int mCode;
    int mType;
    ErrorShared(const std::string& aMsg, int aCode=0, int aType=0)
        :mMsg(aMsg),mCode(aCode),mType(aType){}
    ~ErrorShared()
    {}
};

class Error: protected std::shared_ptr<ErrorShared>
{
public:
    typedef std::shared_ptr<ErrorShared> Base;
    Error(const std::string& msg, int code=0, int type=0)
        :Base(new ErrorShared(msg, code, type))
    {}
    Error(){}
    Error(const char* msg, int code=0, int type=0)
        :Base(new ErrorShared(msg?msg:"", code, type))
    {}
    using Base::operator=;
    const std::string& msg() const {return get()->mMsg;}
    const char* what() const {return get()->mMsg.c_str();}
    int type() const {return get()->mType;}
    int code() const {return get()->mCode;}
    std::string toString() const
    {
        return "Error: '"+get()->mMsg+"'\nType: "+
        std::to_string(get()->mType)+" Code: "+std::to_string(get()->mCode);
    }
};

class PromiseBase
{
protected:
public:
    virtual void reject(const Error& err) = 0;
    virtual PromiseBase* clone() const = 0;
    virtual ~PromiseBase(){}
};

template <int L>
class CallbackList
{
public:
    struct Item
    {
        void* callback;
        PromiseBase* promise;
    };
protected:
    Item items[L];
    int mCount;
public:
    CallbackList():mCount(0){}
    inline void checkCanAdd() const
    {
        if (mCount>=L)
            throw std::runtime_error(kNoMoreCallbacksMsg);
    }
/**
 * Takes ownership of callback, copies the promise.
 * Accepts the callback as a smart pointer of type SP.
 * This is because the method can throw if the list is full,
 * and in this case the smartpointer will prevent the pointer leak.
*/
    template<class SP>
    inline void push(SP& cb, PromiseBase* promise)
    {
        if (mCount>=L)
        {
            delete promise;
            throw std::runtime_error(kNoMoreCallbacksMsg);
        }
        Item& item = items[mCount++];
        item.callback = cb.release();
        item.promise = promise;
    }

    inline Item& operator[](int idx)
    {
        assert((idx >= 0) && (idx <= mCount));
        return items[idx];
    }
    inline const Item& operator[](int idx) const
    {
        assert((idx >= 0) && (idx <= mCount));
        return items[idx];
    }
    inline int count() const {return mCount;}
    inline void addListMoveItems(CallbackList& other)
    {
        int cnt = other.count();
        if (mCount+cnt > L)
            throw std::runtime_error(kNoMoreCallbacksMsg);
        for (int i=0; i<cnt; i++)
        {
            Item& otherItem = other.items[i];
            Item& item = items[mCount+i];
            item.callback = otherItem.callback;
            item.promise = otherItem.promise;
        }
        other.mCount = 0;
        mCount += cnt;
    }
    template <class C>
    void clear()
    {
        for (int i=0; i<mCount; i++)
        {
            Item& item = items[i];
            delete static_cast<C*>(item.callback);
            delete item.promise;
        }
        mCount = 0;
    }
    ~CallbackList() {assert(mCount == 0);}
};

struct _Empty{};

template<typename T, int L=4>
class Promise: public PromiseBase
{
public:
    enum ResolvedState
    {
        PROMISE_RESOLV_NOT = 0,
        PROMISE_RESOLV_SUCCESS = 1,
        PROMISE_RESOLV_FAIL = 2
    };
protected:
    template<class P>
    struct ICallback
    {
        virtual void operator()(const P&) = 0;
        virtual ~ICallback(){}
    };
    template <class P, class CB>
    struct Callback: public ICallback<P>
    {
    protected:
        CB mCb;
    public:
        virtual void operator()(const P& err) { mCb(err); }
        Callback(CB&& cb): mCb(std::forward<CB>(cb)){}
    };
    typedef ICallback<T> ISuccessCb;
    typedef ICallback<Error> IFailCb;
    template <class CB>
    struct SuccessCb: public Callback<T, CB>
    {  using Callback<T,CB>::Callback; };
    template <class CB>
    struct FailCb: public Callback<Error, CB>
    {  using Callback<Error,CB>::Callback; };

/** Helper funtion to be able to deduce the callback type of the passed lambda and create and
  * Callback object with that type. We cannot do that by derectly callind the Callback constructor
  */
    template <class P, class CB>
    ICallback<P>* createCb(CB&& cb)
    {
        return new Callback<P,CB>(std::forward<CB>(cb));
    }

    struct SharedObj
    {
        struct CbLists
        {
            CallbackList<L> mSuccessCbs;
            CallbackList<L> mFailCbs;
        };
        int mRefCount;
        CbLists* mCbs;
        ResolvedState mResolved;
        bool mPending;
        typename std::remove_const<T>::type mResult;
        Error mError;
        SharedObj()
        :mRefCount(1), mCbs(NULL), mResolved(PROMISE_RESOLV_NOT),
         mPending(false)
        {
            PROMISE_LOG_REF("%p: addRef->1", this);
        }

        ~SharedObj()
        {
            if (mCbs)
            {
                mCbs->mSuccessCbs.template clear<ISuccessCb>();
                mCbs->mFailCbs.template clear<IFailCb>();
                delete mCbs;
            }
        }
        inline CbLists& cbs()
        {
            if (!mCbs)
                mCbs = new CbLists;
            return *mCbs;
        }
    };

    template <typename Ret>
    struct ValueTypeFromCbRet
    {  typedef typename std::remove_const<Ret>::type Type; };
    template<typename Ret>
    struct ValueTypeFromCbRet<Promise<Ret> >
    {  typedef typename std::remove_const<Ret>::type Type;  };

    void addRef()
    {
        if (!mSharedObj)
            return;
        mSharedObj->mRefCount++;
        PROMISE_LOG_REF("%p: addRef->%d", mSharedObj, mSharedObj->mRefCount);
    }

    void reset(SharedObj* other=NULL)
    {
        if (mSharedObj)
        {
            int cnt = --(mSharedObj->mRefCount);
            PROMISE_LOG_REF("%p: decRef->%d", mSharedObj, cnt);
            if (cnt <= 0)
            {
                PROMISE_LOG_REF("%p: delete", mSharedObj);
                delete mSharedObj;
            //    mSharedObj = NULL;
                assert(cnt == 0);
            }
        }
        mSharedObj = other;
        addRef();
    }

    inline void decRef()
    {
        if (!mSharedObj)
            return;
        int cnt = --(mSharedObj->mRefCount);
        PROMISE_LOG_REF("%p: decRef->%d", mSharedObj, cnt);
        if (cnt <= 0)
        {
            PROMISE_LOG_REF("%p: delete", mSharedObj);
            delete mSharedObj;
            mSharedObj = NULL;
            assert(cnt == 0);
        }
    }
/*    inline void swap(Promise<T>& other)
    {
        auto tmp = mSharedObj;
        mSharedObj = other.mSharedObj;
        other.mSharedObj = tmp;
    }
*/
//We need to keeps the shared object in memory until the promise is
//resolved, even when all user Promise objects go out of scope
//This reference will be released when the promise is resolved or rejected
//            addRef();
public://TODO: Must make these protected and make them accessible buy a friend proxy
    inline CallbackList<L>& thenCbs() {return mSharedObj->cbs().mSuccessCbs;}
    inline CallbackList<L>& failCbs() {return mSharedObj->cbs().mFailCbs;}
public:
    SharedObj* mSharedObj;
    typedef T Type;
    Promise(const _Empty&) : mSharedObj(NULL){} //internal use
    Promise() : mSharedObj(new SharedObj){}
    Promise(const Promise& other):mSharedObj(NULL)
    {
        reset(other.mSharedObj);
    }
    template <class=PROMISE_ENABLE_IF((NotSame<T, Error>::value), void)>
    Promise(const T& val):mSharedObj(new SharedObj)
    {
        resolve(val);
    }
    Promise(T&& val):mSharedObj(new SharedObj)
    {
        resolve(std::forward<T>(val));
    }

    Promise(const Error& err):mSharedObj(new SharedObj)
    {
        reject(err);
    }
    Promise<T>& operator=(const Promise<T>& other)
    {
        reset(other.mSharedObj);
        return *this;
    }

    virtual ~Promise() { decRef(); }

    int done() const
    { return (mSharedObj ? (mSharedObj->mResolved) : PROMISE_RESOLV_NOT); }

protected:
    virtual PromiseBase* clone() const
    {    return new Promise<T>(*this);    }

/** Creates a wrapper function around a then() handler that handles exceptions and propagates
 * the result to resolve/reject chained promises
 */
    template <typename In, typename Out, class CB>
    ICallback<In>* createChainedCb(CB&& cb, Promise<Out>& next)
    {
        //cb must have the singature Promise<Out>(const In&)
        return createCb<In>([cb,next](const In& result) mutable->void
        {
            _Empty e;
            Promise<Out> promise(e);
            try
            {
                promise = cb(result);
            }
            catch(std::exception& e)
            {
                next.reject(Error(e.what(), 0, 1));
                return;
            }
            catch(Error& e)
            {
                next.reject(e);
                return;
            }
            catch(const char* e)
            {
                next.reject(Error(e, 0, 1));
                return;
            }
            catch(...)
            {
                next.reject(Error("(unknown exception type)", 0, 1));
                return;
            }
            if (!promise.hasCallbacks())
                std::swap(promise.mSharedObj->mCbs, next.mSharedObj->mCbs);
            else
            {
                //add our callbacks and errbacks to the returned promise
                auto& nextCbs = next.thenCbs();
                if (nextCbs.count())
                {
                    auto& promiseCbs = promise.thenCbs();
                    promiseCbs.addListMoveItems(nextCbs);
                }
                auto& nextEbs = next.failCbs();
                if (nextEbs.count())
                {
                    auto& promiseEbs = promise.failCbs();
                    promiseEbs.addListMoveItems(nextEbs);
                }
            }
            if (promise.mSharedObj->mPending)
                promise.doPendingResolve();
        });
    }
public:
/**
* The Out template argument is the return type of the provided callback \c cb
* It is the same as the argument type of the next chained then()
* callback(if any). The \c cb callback can return a value of type \c Out or a
* \c Promise<Out> instance
*/
    template <typename F>
    auto then(F&& cb)->Promise<typename ValueTypeFromCbRet<decltype(cb(mSharedObj->mResult))>::Type>
    {
        typedef typename ValueTypeFromCbRet<decltype(cb(mSharedObj->mResult))>::Type Out;
        Promise<Out> next;
        if (mSharedObj->mResolved == PROMISE_RESOLV_FAIL)
        {
            next.reject(mSharedObj->mError);
            return next;
        }

        std::unique_ptr<ISuccessCb> resolveCb(createChainedCb<T, Out>(std::forward<F>(cb), next));

        if (mSharedObj->mResolved == PROMISE_RESOLV_SUCCESS)
        {
            (*resolveCb)(mSharedObj->mResult);
        }
        else
        {
            assert((mSharedObj->mResolved == PROMISE_RESOLV_NOT));
            thenCbs().push(resolveCb, new Promise<Out>(next));
        }

        return next;
    }
/** Adds a handler to be executed in case the promise is rejected
* \note
* fail() must always return a promise of the same type as the one of the
* promise on which it is called (i.e. type T)
* This is because the next promise in the chain can have a then() handler,
* which in case of no success, will get its value from the promise before
* the fail(). In other words
* fail()-s in the chain must always preserve the result type from the last
* then()
*/
    template <typename F>
    auto fail(F&& eb)->Promise<T>
    {
        Promise<T> next;
        if (mSharedObj->mResolved == PROMISE_RESOLV_SUCCESS)
        {
            next.resolve(mSharedObj->mResult);
            return next;
        }
        std::unique_ptr<IFailCb> failCb(createChainedCb<Error, T>(std::forward<F>(eb), next));

        if(mSharedObj->mResolved == PROMISE_RESOLV_FAIL)
            (*failCb)(mSharedObj->mError);
        else
        {
            assert((mSharedObj->mResolved == PROMISE_RESOLV_NOT));
            failCbs().push(failCb, new Promise<T>(next));
        }

        return next;
    }
    inline bool hasCallbacks()
    {        return (mSharedObj->mCbs!=NULL);    }
    //V can be const& or &&
    template <typename V>
    void resolve(V val)
    {
        if (mSharedObj->mResolved)
            throw std::runtime_error("Already resolved/rejected");

        mSharedObj->mResolved = PROMISE_RESOLV_SUCCESS;
        if (hasCallbacks())
            doResolve(val);
        else
        {
            mSharedObj->mResult = std::move(val);
            mSharedObj->mPending = true;
        }
    }
protected:
    void doResolve(const T& val)
    {
        auto& cbs = thenCbs();
        int cnt = cbs.count();
        for (int i=0; i<cnt; i++)
            (*static_cast<ISuccessCb*>(cbs[i].callback))(val);
//now propagate the successful resolve skipping the fail() handlers to
//the handlers following them. The promises that follow the fail()
//are guaranteed to be of our type, because fail() callbacks
//preserve the type - return the same type as the promise they were
//called on (i.e. all fail promises, 1 or more in a row,
//have the type of the last then() callback)
        auto& ebs = failCbs();
        cnt = ebs.count();
        for (int i=0; i<cnt; i++)
        {
            auto& item = ebs[i];
            (static_cast<Promise<T>*>(item.promise))->resolve(val);
        }

//we dont need to explicitly keep the shared state in memory anymore,
//release the artificial reference, and the shared object will be deleted
//as soon as all user promise objects go out of scope
//        decRef();
    }
public:
    virtual void reject(const Error& err)
    {
        if (mSharedObj->mResolved)
            throw std::runtime_error("Alrady resolved/rejected");
        mSharedObj->mResolved = PROMISE_RESOLV_FAIL;
        if (hasCallbacks())
            doReject(err);
        else
        {
            mSharedObj->mError = err;
            mSharedObj->mPending = true;
        }
    }
    inline void reject(const std::string& msg)
    {
        reject(Error(msg));
    }
    inline void reject(const char* msg)
    {
        if (!msg)
            msg = "";
        reject(Error(msg));
    }
    inline void reject(int code, int type)
    {
        reject(Error("", code, type));
    }
protected:
    void doReject(const Error& err)
    {
        auto& ebs = failCbs();
        int cnt = ebs.count();
        for (int i=0; i<cnt; i++)
            (*static_cast<IFailCb*>(ebs[i].callback))(err);
//propagate past success handlers till a fail handler is found
        auto& cbs = thenCbs();
        cnt = cbs.count();
        for (int i=0; i<cnt; i++)
        {
            auto& item = cbs[i];
//we dont know the type of the promise, but the interface to reject()
//has a fixed type, hence the reject() is a vitual function of the base class
//and we can reject any promise without knowing its type
            item.promise->reject(err);
        }

//        decRef();
    }
public:
    void doPendingResolve()
    {
        if (!hasCallbacks())
            return;
        assert(mSharedObj->mPending);
        auto state = mSharedObj->mResolved;
        assert(state != PROMISE_RESOLV_NOT);
        if (state == PROMISE_RESOLV_SUCCESS)
            doResolve(mSharedObj->mResult);
        else if (state == PROMISE_RESOLV_FAIL)
            doReject(mSharedObj->mError);
        else
            throw std::runtime_error("Incorrect pending resolve type: "+std::to_string(state));
    }
};

template<typename T>
inline Promise<T> reject(const Error& err)
{
    return Promise<T>(err);
}

struct WhenStateShared
{
    int numready = 0;
    Promise<int> output;
    bool lastAdded = false;
    int totalCount = 0;
};

struct WhenState: public std::shared_ptr<WhenStateShared>
{
    WhenState():std::shared_ptr<WhenStateShared>(new WhenStateShared){}
};

template <class T>
inline void _when_add_single(WhenState& state, Promise<T>& pms)
{
    state->totalCount++;
    pms.then([state, pms](const T& ret)
    {
        int n = ++(state->numready);
        PROMISE_LOG_REF("%p: when: %p: numready = %d, pms state: %d\n", pms.mSharedObj, state.get(), state->numready, pms.mSharedObj->mResolved);
        if (!state->lastAdded || (n < state->totalCount))
            return ret;
        assert(n == state->totalCount);
        state->output.resolve(0);
        return ret;
    });
    pms.fail([state](const Error& err)
    {
        state->output.reject(err);
        return err;
    });
}

template <class T>
inline void _when_add(WhenState& state, Promise<T>& promise)
{
//this is called when the final promise is added. Now we know the actual count
    state->lastAdded = true;
    _when_add_single(state, promise);
}
template <class T, class...Args>
inline void _when_add(WhenState& state, Promise<T>& promise,
                      Args... promises)
{
    _when_add_single(state, promise);
    _when_add(state, promises...);
}

template<class... Args>
inline Promise<int> when(Args... inputs)
{
    WhenState state;
    _when_add(state, inputs...);
    return state->output;
}
}//end namespace promise
#endif
