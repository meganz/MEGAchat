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
enum ResolvedState
{
    kNotResolved = 0,
    kSucceeded = 1,
    kFailed = 2
};

#define PROMISE_LOG(fmtString,...) printf("promise: " fmtString"\n", ##__VA_ARGS__)
#ifdef PROMISE_DEBUG_REFS
    #define PROMISE_LOG_REF(fmtString,...) PROMISE_LOG(fmtString, ##__VA_ARGS__)
#else
    #define PROMISE_LOG_REF(fmtString,...)
#endif

static const char* kNoMoreCallbacksMsg =
  "No more space for promise callbacks, please increase the N template argument";

//===
struct _Void{};
typedef _Void Void;

template<typename V>
struct MaskVoid { typedef V type;};
template<>
struct MaskVoid<void> {typedef _Void type;};

//get function/lambda return type, regardless of argument count and types
template <class F>
struct FuncTraits: public FuncTraits<decltype(&F::operator())>{};

template <class R, class... Args>
struct FuncTraits <R(*)(Args...)>{ typedef R RetType; enum {nargs = sizeof...(Args)};};

template <class C, class R, class...Args>
struct FuncTraits <R(C::*)(Args...)> { typedef R RetType; enum {nargs = sizeof...(Args)};};

template <class C, class R, class...Args>
struct FuncTraits <R(C::*)(Args...) const> { typedef R RetType; enum {nargs = sizeof...(Args)};};
//===
struct IVirtDtor
{  virtual ~IVirtDtor() {}  };

template <class T, int L>
class Promise;

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

enum
{
    kErrorTypeGeneric = 1
};
enum
{
    kErrException = 1,
    kErrAbort = 2,
    kErrTimeout = 3
};

class Error: protected std::shared_ptr<ErrorShared>
{
protected:
    Error(): Base(nullptr){}
public:
    typedef std::shared_ptr<ErrorShared> Base;
    Error(const std::string& msg, int code=0, int type=kErrorTypeGeneric)
        :Base(std::make_shared<ErrorShared>(msg, code, type))
    {}
    Error(const char* msg, int code=0, int type=kErrorTypeGeneric)
        :Base(std::make_shared<ErrorShared>(msg?msg:"", code, type))
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
    template <class T, int L>
    friend class Promise;
};

class PromiseBase
{
protected:
public:
    virtual PromiseBase* clone() const = 0;
    virtual ~PromiseBase(){}
};

template <int L, class C>
class CallbackList
{
protected:
    C* items[L];
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
    inline void push(SP& cb)
    {
        if (mCount >= L)
            throw std::runtime_error(kNoMoreCallbacksMsg);
        items[mCount++] = cb.release();
    }

    inline C*& operator[](int idx)
    {
        assert((idx >= 0) && (idx <= mCount));
        return items[idx];
    }
    inline const C*& operator[](int idx) const
    {
        assert((idx >= 0) && (idx <= mCount));
        return items[idx];
    }
    inline C*& first()
    {
        assert(mCount > 0);
        return items[0];
    }
    inline int count() const {return mCount;}
    inline void addListMoveItems(CallbackList& other)
    {
        int cnt = other.count();
        if (mCount+cnt > L)
            throw std::runtime_error(kNoMoreCallbacksMsg);
        for (int i=0; i<cnt; i++)
            items[mCount++] = other.items[i];
        other.mCount = 0;
    }
    void clear()
    {
        static_assert(std::is_base_of<IVirtDtor, C>::value, "Callback type must be inherited from IVirtDtor");
        for (int i=0; i<mCount; i++)
            delete ((IVirtDtor*)items[i]); //static_cast wont work here because there is no info that ICallback inherits from IVirtDtor
        mCount = 0;
    }
    ~CallbackList() {assert(mCount == 0);}
};

struct _Empty{};
typedef _Empty Empty;

template<typename T, int L=4>
class Promise: public PromiseBase
{
public:
protected:
    template<class P>
    struct ICallback: public IVirtDtor
    {
        virtual void operator()(const P&) = 0;
        virtual void rejectNextPromise(const Error&) = 0;
    };

    template <class P, class TP>
    struct ICallbackWithPromise: public ICallback<P>
    {
    public:
        Promise<TP> nextPromise;
        virtual void rejectNextPromise(const Error& err) { nextPromise.reject(err); }
        ICallbackWithPromise(const Promise<TP>& next): nextPromise(next){}
    };

    template <class P, class CB, class TP=int>
    struct Callback: public ICallbackWithPromise<P, TP>
    {
    protected:
        CB mCb;
    public:
        virtual void operator()(const P& arg) { mCb(arg); }
        Callback(CB&& cb, const Promise<TP>& next)
            :ICallbackWithPromise<P, TP>(next), mCb(std::forward<CB>(cb)){}
    };
    typedef ICallback<typename MaskVoid<T>::type> ISuccessCb;
    typedef ICallback<Error> IFailCb;
    typedef ICallbackWithPromise<Error, T> IFailCbWithPromise;

    template <class CB, class TP>
    struct SuccessCb: public Callback<typename MaskVoid<T>::type, CB, TP>
    {
        SuccessCb(CB&& cb, const Promise<TP>& next) //can't use ctotr inharitance because MSVC 2013 does not support it
            :Callback<typename MaskVoid<T>::type, CB, TP>(std::forward<CB>(cb), next){}
    };

    template <class CB>
    struct FailCb: public Callback<Error, CB, T>
    {
        FailCb(CB&& cb, const Promise<T>& next)
        :Callback<Error, CB, T>(std::forward<CB>(cb), next){}
    };
/** Helper funtion to be able to deduce the callback type of the passed lambda and create and
  * Callback object with that type. We cannot do that by derectly callind the Callback constructor
  */
    template <class P, class CB, class TP>
    ICallback<typename MaskVoid<P>::type>* createCb(CB&& cb, Promise<TP>& next)
    {
        return new Callback<typename MaskVoid<P>::type, CB, TP>(std::forward<CB>(cb), next);
    }
//===
    struct SharedObj
    {
        struct CbLists
        {
            CallbackList<L, ISuccessCb> mSuccessCbs;
            CallbackList<L, IFailCb> mFailCbs;
        };
        int mRefCount;
        CbLists* mCbs;
        ResolvedState mResolved;
        bool mPending;
        Promise<T,L> mMaster;
        typename MaskVoid<typename std::remove_const<T>::type>::type mResult;
        Error mError;
        SharedObj()
        :mRefCount(1), mCbs(NULL), mResolved(kNotResolved),
         mPending(false), mMaster(_Empty())
        {
            PROMISE_LOG_REF("%p: addRef->1", this);
        }

        ~SharedObj()
        {
            if (mCbs)
            {
                mCbs->mSuccessCbs.clear();
                mCbs->mFailCbs.clear();
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
    struct RemovePromise
    {  typedef typename std::remove_const<Ret>::type Type; };
    template<typename Ret>
    struct RemovePromise<Promise<Ret> >
    {  typedef typename std::remove_const<Ret>::type Type;  };

//===
    struct CallCbHandleVoids
    {
        template<class Out, class CbOut, class In, class CB, class=typename std::enable_if<!std::is_same<In,_Void>::value && !std::is_same<CbOut, void>::value, int>::type>
        static Promise<Out> call(CB& cb, const In& val) {  return cb(val);  }

        template<class Out, class CbOut, class In, class CB, class=typename std::enable_if<std::is_same<In,_Void>::value && !std::is_same<CbOut, void>::value, int>::type>
        static Promise<Out> call(CB& cb, const _Void& val) {  return cb();   }

        template<class Out, class CbOut, class In, class CB, class=typename std::enable_if<!std::is_same<In,_Void>::value && std::is_same<CbOut,void>::value, int>::type>
        static Promise<void> call(CB& cb, const In& val){ cb(val); return _Void(); }

        template<class Out, class CbOut, class In, class CB, class=typename std::enable_if<std::is_same<In,_Void>::value && std::is_same<CbOut,void>::value, int>::type>
        static Promise<void> call(CB& cb, const _Void& val) { cb(); return _Void(); }
    };
//===
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
            int cnt = mSharedObj->mRefCount;
            PROMISE_LOG_REF("%p: decRef->%d", mSharedObj, cnt);
            if (cnt <= 1)
            {
                assert(cnt == 1);
                PROMISE_LOG_REF("%p: delete", mSharedObj);
                delete mSharedObj;
            }
            else
            {
                --(mSharedObj->mRefCount);
            }
        }
        mSharedObj = other;
        addRef();
    }
    inline CallbackList<L, ISuccessCb>& thenCbs() {return mSharedObj->cbs().mSuccessCbs;}
    inline CallbackList<L, IFailCb>& failCbs() {return mSharedObj->cbs().mFailCbs;}
    SharedObj* mSharedObj;
    template <class FT,int FL> friend class Promise;
public:
    typedef T Type;
    Promise(_Empty) : mSharedObj(NULL){} //Use with care - only when subsequent re-assing is guaranteed.
    Promise() : mSharedObj(new SharedObj){}
    Promise(const Promise& other):mSharedObj(NULL)
    {
        reset(other.mSharedObj);
    }
    template <class=typename std::enable_if<!std::is_same<T, Error>::value, int>::type>
    Promise(const typename MaskVoid<T>::type& val):mSharedObj(new SharedObj)
    {
        resolve(val);
    }
    Promise(typename MaskVoid<T>::type&& val):mSharedObj(new SharedObj)
    {
        resolve(std::forward<typename MaskVoid<T>::type>(val));
    }

    Promise(const Error& err):mSharedObj(new SharedObj)
    {
        assert(err);
        reject(err);
    }
    Promise<T>& operator=(const Promise<T>& other)
    {
        reset(other.mSharedObj);
        return *this;
    }

    virtual ~Promise()
    {
        if (mSharedObj)
        {
            int cnt = mSharedObj->mRefCount;
            PROMISE_LOG_REF("%p: decRef->%d", mSharedObj, cnt);
            if (cnt <= 1)
            {
                assert(cnt == 1);
                PROMISE_LOG_REF("%p: delete", mSharedObj);
                delete mSharedObj;
            }
            else
            {
                mSharedObj->mRefCount--;
            }
        }
    }
    int done() const
    {
        if (!mSharedObj)
            return kNotResolved;
        auto& master = mSharedObj->mMaster;
        return master.mSharedObj
                ? master.mSharedObj->mResolved
                : mSharedObj->mResolved;
    }
    bool succeeded() const { return done() == kSucceeded; }
    bool failed() const { return done() == kFailed; }
    const Error& error() const
    {
        assert(mSharedObj);
        assert(done() == kFailed);
        auto& master = mSharedObj->mMaster;
        return master.mSharedObj
                ? master.mSharedObj->mError
                : mSharedObj->mError;
    }
    template <class Ret=T>
    const typename std::enable_if<!std::is_same<Ret, void>::value, Ret>::type& value() const
    {
        assert(mSharedObj);
        auto master = mSharedObj->mMaster;
        if (master.mSharedObj)
        {
            assert(master.done());
            return master.mSharedObj->mResult;
        }
        else
        {
            assert(done() == kSucceeded);
            return mSharedObj->mResult;
        }
    }
protected:
    virtual PromiseBase* clone() const
    {    return new Promise<T>(*this);    }
    bool hasMaster() const { return (mSharedObj->mMaster.mSharedObj != nullptr) ; }

/** Creates a wrapper function around a then() or fail() handler that handles exceptions and propagates
 * the result to resolve/reject chained promises. \c In is the type of the callback's parameter,
 * \c Out is its return type, \c CB is the type of the callback itself.
 */
    template <typename In, typename Out, typename RealOut, class CB>
    ICallback<In>* createChainedCb(CB&& cb, Promise<Out>& next)
    {
        //cb must have the singature Promise<Out>(const In&)
        return createCb<In>([cb,next](const In& result) mutable->void
        {
            Promise<Out> promise((_Empty()));
            try
            {
                promise = CallCbHandleVoids::template call<Out, RealOut, In>(cb, result);
            }
            catch(std::exception& e)
            {
                next.reject(Error(e.what(), kErrException));
                return;
            }
            catch(Error& e)
            {
                next.reject(e);
                return;
            }
            catch(const char* e)
            {
                next.reject(Error(e, kErrException));
                return;
            }
            catch(...)
            {
                next.reject(Error("(unknown exception type)", kErrException));
                return;
            }

            Promise<Out>* master =&(promise.mSharedObj->mMaster); //master is the promise that actually gets resolved, equivalent to the 'deferred' object
            if (!master->mSharedObj)
            {
                master = &promise;
            }
            assert(!master->hasMaster());
            next.mSharedObj->mMaster = *master; //makes 'next' attach subsequently added callbacks to 'master'

            if (!master->hasCallbacks())
            {
                master->mSharedObj->mCbs = next.mSharedObj->mCbs;
                next.mSharedObj->mCbs = nullptr;
            }
            else
            {
                //move the callbacks and errbacks of 'next' to 'master'
                auto& nextCbs = next.thenCbs();
                if (nextCbs.count())
                    master->thenCbs().addListMoveItems(nextCbs);

                auto& nextEbs = next.failCbs();
                if (nextEbs.count())
                    master->failCbs().addListMoveItems(nextEbs);
            }
            if (master->mSharedObj->mPending)
                master->doPendingResolve();
        }, next);
    }

public:
/**
* The Out template argument is the return type of the provided callback \c cb
* It is the same as the argument type of the next chained then()
* callback(if any). The \c cb callback can return a value of type \c Out or a
* \c Promise<Out> instance
*/
    template <typename F>
    auto then(F&& cb)->Promise<typename RemovePromise<typename FuncTraits<F>::RetType>::Type >
    {
        if (mSharedObj->mMaster.mSharedObj) //if we are a slave promise (returned by then() or fail()), forward callbacks to our master promise
            return mSharedObj->mMaster.then(std::forward<F>(cb));

        if (mSharedObj->mResolved == kFailed)
            return mSharedObj->mError;

        typedef typename RemovePromise<typename FuncTraits<F>::RetType>::Type Out;
        Promise<Out> next;

        std::unique_ptr<ISuccessCb> resolveCb(createChainedCb<typename MaskVoid<T>::type, Out,
            typename FuncTraits<F>::RetType>(std::forward<F>(cb), next));

        if (mSharedObj->mResolved == kSucceeded)
        {
            (*resolveCb)(mSharedObj->mResult);
        }
        else
        {
            assert((mSharedObj->mResolved == kNotResolved));
            thenCbs().push(resolveCb);
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
        if (mSharedObj->mMaster.mSharedObj) //if we are a slave promise (returned by then() or fail()), forward callbacks to our master promise
            return mSharedObj->mMaster.fail(std::forward<F>(eb));

        if (mSharedObj->mResolved == kSucceeded)
            return mSharedObj->mResult;

        Promise<T> next;
        std::unique_ptr<IFailCb> failCb(createChainedCb<Error, T,
            typename FuncTraits<F>::RetType>(std::forward<F>(eb), next));

        if(mSharedObj->mResolved == kFailed)
            (*failCb)(mSharedObj->mError);
        else
        {
            assert((mSharedObj->mResolved == kNotResolved));
            failCbs().push(failCb);
        }

        return next;
    }
    //val can be a by-value param, const& or &&
    template <typename V>
    void resolve(V&& val)
    {
        if (mSharedObj->mResolved)
            throw std::runtime_error("Already resolved/rejected");

        mSharedObj->mResolved = kSucceeded;
        if (hasCallbacks())
            doResolve(val);
        else
        {
            mSharedObj->mResult = std::forward<V>(val);
            mSharedObj->mPending = true;
        }
    }
    template <typename V=T, class=typename std::enable_if<std::is_same<V,void>::value, int>::type>
    void resolve()
    {
        resolve(_Void());
    }

protected:
    inline bool hasCallbacks() const { return (mSharedObj->mCbs!=NULL); }
    void doResolve(const typename MaskVoid<T>::type& val)
    {
        auto& cbs = thenCbs();
        int cnt = cbs.count();
        if (cnt)
        {
            if (cnt == 1) //optimize for single callback
            {
                (*cbs.first())(val);
            }
            else
            {
                for (int i=0; i<cnt; i++)
                    (*cbs[i])(val);
            }
        }
//now propagate the successful resolve skipping the fail() handlers to
//the handlers following them. The promises that follow the fail()
//are guaranteed to be of our type, because fail() callbacks
//preserve the type - return the same type as the promise they were
//called on (i.e. all fail promises, 1 or more in a row,
//have the type of the last then() callback)
        auto& ebs = failCbs();
        cnt = ebs.count();
        if(cnt)
        {
            if (cnt == 1)
            {
                static_cast<IFailCbWithPromise*>(ebs.first())->nextPromise.resolve(val);
            }
            else
            {
                for (int i=0; i<cnt; i++)
                {
                    auto& item = ebs[i];
                    static_cast<IFailCbWithPromise*>(item)->nextPromise.resolve(val);
                }
            }
        }
    }
public:
    void reject(const Error& err)
    {
        assert(err);
        if (mSharedObj->mResolved)
            throw std::runtime_error("Alrady resolved/rejected");

        mSharedObj->mError = err;
        mSharedObj->mResolved = kFailed;

        if (hasCallbacks())
            doReject(err);
        else
            mSharedObj->mPending = true;
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
    inline void reject(const std::string& msg, int code, int type)
    {
        reject(Error(msg, code, type));
    }

protected:
    void doReject(const Error& err)
    {
        assert(mSharedObj->mError);
        assert(mSharedObj->mResolved == kFailed);
        auto& ebs = failCbs();
        int cnt = ebs.count();
        if (cnt)
        {
            if (cnt == 1) //optimize
            {
                (*static_cast<IFailCb*>(ebs.first()))(err);
            }
            else
            {
                for (int i=0; i<cnt; i++)
                    (*static_cast<IFailCb*>(ebs[i]))(err);
            }
        }
//propagate past success handlers till a fail handler is found
        auto& cbs = thenCbs();
        cnt = cbs.count();
        if (cnt)
        {
            if (cnt == 1)
            {
                cbs.first()->rejectNextPromise(err);
            }
            else
            {
                for (int i=0; i<cnt; i++)
                {
//we dont know the type of the promise, but the interface to reject()
//has a fixed type, hence the reject() is a vitual function of the base class
//and we can reject any promise without knowing its type
                    cbs[i]->rejectNextPromise(err);
                }
            }
        }
    }
    void doPendingResolve()
    {
        if (!hasCallbacks())
            return;
        assert(mSharedObj->mPending);
        auto state = mSharedObj->mResolved;
        assert(state != kNotResolved);
        if (state == kSucceeded)
            doResolve(mSharedObj->mResult);
        else if (state == kFailed)
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
    Promise<void> output;
    bool lastAdded = false;
    int totalCount = 0;
};

struct WhenState: public std::shared_ptr<WhenStateShared>
{
    WhenState():std::shared_ptr<WhenStateShared>(new WhenStateShared){}
};

template <class T, class=typename std::enable_if<!std::is_same<T,void>::value, int>::type>
inline void _when_add_single(WhenState& state, Promise<T>& pms)
{
    state->totalCount++;
    pms.then([state](const T& ret)
    {
        int n = ++(state->numready);
        PROMISE_LOG_REF("when: %p: numready = %d", state.get(), state->numready);
        if (!state->lastAdded || (n < state->totalCount))
            return ret;
        assert(n == state->totalCount);
        if (!state->output.done())
            state->output.resolve();
        return ret;
    });
    pms.fail([state](const Error& err)
    {
        if (!state->output.done())
            state->output.reject(err);
        return err;
    });
}

template <class T, class=typename std::enable_if<std::is_same<T,void>::value, int>::type>
inline void _when_add_single(WhenState& state, Promise<void>& pms)
{
    state->totalCount++;
    pms.then([state]()
    {
        int n = ++(state->numready);
        PROMISE_LOG_REF("when: %p: numready = %d", state.get(), state->numready);
        if (!state->lastAdded || (n < state->totalCount))
            return;
        assert(n == state->totalCount);
        if (!state->output.done())
            state->output.resolve();
    });
    pms.fail([state](const Error& err)
    {
        if (!state->output.done())
            state->output.reject(err);
        return err;
    });
}

template <class T>
inline void _when_add(WhenState& state, Promise<T>& promise)
{
//this is called when the final promise is added. Now we know the actual count
    state->lastAdded = true;
    _when_add_single<T>(state, promise);
}
template <class T, class...Args>
inline void _when_add(WhenState& state, Promise<T>& promise,
                      Args... promises)
{
    _when_add_single<T>(state, promise);
    _when_add(state, promises...);
}

template<class... Args>
inline Promise<void> when(Args... inputs)
{
    WhenState state;
    _when_add(state, inputs...);
    return state->output;
}
}//end namespace promise
#endif
