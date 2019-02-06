/** @author Alexander Vassilev */

#ifndef _PROMISE_H
#define _PROMISE_H
#include <stdexcept>
#include <vector>
#include <string>
#include <utility>
#include <memory>
#include <assert.h>

/** @brief The name of the unhandled promise error handler. This handler is
 * called when a promise fails, but the user has not provided a fail() callback
 * to handle that error. Often this is unintentional and results in the program
 * stepping execution for no obvious reason.
 * The user can define this to customize the unhandled error trap.
 * The default just prints a warning to stderr
 */
#ifndef PROMISE_ON_UNHANDLED_ERROR
    #define PROMISE_ON_UNHANDLED_ERROR ErrorShared::defaultOnUnhandledError
#else // define the prototype, as it may come after the inclusion of promise.h
    void PROMISE_ON_UNHANDLED_ERROR(const std::string& msg, int type, int code);
#endif

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

template <class T>
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
    mutable bool mHandled = false;
    ErrorShared(const std::string& aMsg, int aCode=-1, int aType=0)
        :mMsg(aMsg),mCode(aCode),mType(aType){}
    ~ErrorShared()
    {
        if (!mHandled)
            PROMISE_ON_UNHANDLED_ERROR(mMsg, mType, mCode);
    }
    static void defaultOnUnhandledError(const std::string& msg, int type, int code)
    {
        fprintf(stderr, "WARNING: Unhandled promise fail. Error: '%s', type: %d, code: %d\n", msg.c_str(), type, code);
    }
};

enum
{
    kErrorTypeGeneric       =   1,
    kErrorUnknown           =  -1,
    kErrorArgs              =  -2,
    kErrorNoEnt             =  -9,
    kErrorAccess            = -11,
    kErrorAlreadyExist      = -12
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
    Error(const std::string& msg, int code=-1, int type=kErrorTypeGeneric)
        :Base(std::make_shared<ErrorShared>(msg, code, type))
    {}
    Error(const char* msg, int code=-1, int type=kErrorTypeGeneric)
        :Base(std::make_shared<ErrorShared>(msg?msg:"", code, type))
    {}
    using Base::operator=;
    const std::string& msg() const {return get()->mMsg;}
    const char* what() const {return get()->mMsg.c_str();}
    int type() const {return get()->mType;}
    int code() const {return get()->mCode;}
    void setHandled() const { get()->mHandled = true; }
    bool handled() const { return get()->mHandled; }
    std::string toString() const
    {
        return "Error: '"+get()->mMsg+"'\nType: "+
        std::to_string(get()->mType)+" Code: "+std::to_string(get()->mCode);
    }
    template <class T>
    friend class Promise;
};

class PromiseBase
{
protected:
public:
    virtual PromiseBase* clone() const = 0;
    virtual ~PromiseBase(){}
};

template <class C>
class CallbackList
{
protected:
    std::vector<C*> items;
public:
    CallbackList(){}
/**
 * Takes ownership of callback, copies the promise.
 * Accepts the callback as a smart pointer of type SP.
 * This is because the method can throw if the list is full,
 * and in this case the smartpointer will prevent the pointer leak.
*/
    template<class SP>
    inline void push(SP& cb)
    {
        items.push_back(cb.release());
    }

    inline C*& operator[](int idx)
    {
        assert((idx >= 0) && (idx < (int)items.size()));
        return items[idx];
    }
    inline const C*& operator[](int idx) const
    {
        assert((idx >= 0) && (idx < items.size()));
        return items[idx];
    }
    inline C*& first()
    {
        assert(!items.empty());
        return items[0];
    }
    inline int count() const
    {
        return items.size();
    }
    inline void addListMoveItems(CallbackList& other)
    {
        items.insert(items.end(), other.items.begin(), other.items.end());
        other.items.clear();
    }
    void clear()
    {
        static_assert(std::is_base_of<IVirtDtor, C>::value, "Callback type must be inherited from IVirtDtor");
        for (auto it = items.begin(); it != items.end(); it++)
        {
            delete ((IVirtDtor*)*it); //static_cast wont work here because there is no info that ICallback inherits from IVirtDtor
        }
        items.clear();
    }
    ~CallbackList()
    {
        assert(items.empty());
    }
};

struct _Empty{};
typedef _Empty Empty;

template<typename T>
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
        virtual void operator()(const P& arg) { mCb(arg, *this); }
        Callback(CB&& cb, const Promise<TP>& next)
            :ICallbackWithPromise<P, TP>(next), mCb(std::forward<CB>(cb)){}
        CB& callback() { return mCb; }
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
  * Callback object with that type. We cannot do that by directly calling the Callback constructor
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
            CallbackList<ISuccessCb> mSuccessCbs;
            CallbackList<IFailCb> mFailCbs;
        };
        int mRefCount;
        CbLists* mCbs;
        ResolvedState mResolved;
        bool mPending;
        Promise<T> mMaster;
        typename MaskVoid<typename std::remove_const<T>::type>::type mResult;
        Error mError;
        SharedObj()
        :mRefCount(1), mCbs(NULL), mResolved(kNotResolved),
         mPending(false), mMaster(_Empty())
        {
            PROMISE_LOG_REF("%p: addRef -> 1 (SharedObj ctor)", this);
        }
        void ref()
        {
            mRefCount++;
            PROMISE_LOG_REF("%p: ref -> %d", this, mRefCount);
        }
        void unref()
        {
            if (--mRefCount > 0)
            {
                PROMISE_LOG_REF("%p: unref -> %d", this, mRefCount);
                return;
            }
            assert(mRefCount == 0);
            PROMISE_LOG_REF("%p: unref -> 0 (deleting SharedObj)", this);
            delete this;
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
        static Promise<Out> call(CB& cb, const _Void& /*val*/) {  return cb();   }

        template<class Out, class CbOut, class In, class CB, class=typename std::enable_if<!std::is_same<In,_Void>::value && std::is_same<CbOut,void>::value, int>::type>
        static Promise<void> call(CB& cb, const In& val){ cb(val); return _Void(); }

        template<class Out, class CbOut, class In, class CB, class=typename std::enable_if<std::is_same<In,_Void>::value && std::is_same<CbOut,void>::value, int>::type>
        static Promise<void> call(CB& cb, const _Void& /*val*/) { cb(); return _Void(); }
    };
//===
    void reset(SharedObj* other=NULL)
    {
        if (mSharedObj)
        {
            mSharedObj->unref();
        }
        mSharedObj = other;
        if (mSharedObj)
        {
            mSharedObj->ref();
        }
    }
    inline CallbackList<ISuccessCb>& thenCbs() {return mSharedObj->cbs().mSuccessCbs;}
    inline CallbackList<IFailCb>& failCbs() {return mSharedObj->cbs().mFailCbs;}
    SharedObj* mSharedObj;
    template <class FT> friend class Promise;
public:
    typedef T Type;
    /** @brief Creates an uninitialized promise.
     *  @attention Use with care - only when subsequent re-assigning
     *  is guaranteed.
     */
    Promise(_Empty): mSharedObj(NULL){}
    Promise(): mSharedObj(new SharedObj){}
    Promise(const Promise& other): mSharedObj(other.mSharedObj)
    {
        if (mSharedObj)
            mSharedObj->ref();
    }
    template <class=typename std::enable_if<!std::is_same<T, Error>::value, int>::type>
    Promise(const typename MaskVoid<T>::type& val): mSharedObj(new SharedObj)
    {
        resolve(val);
    }
    Promise(typename MaskVoid<T>::type&& val): mSharedObj(new SharedObj)
    {
        resolve(std::forward<typename MaskVoid<T>::type>(val));
    }

    Promise(const Error& err): mSharedObj(new SharedObj)
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
            mSharedObj->unref();
        }
    }
    int done() const
    {
        if (!mSharedObj)
            return kNotResolved;
        return getMaster().mSharedObj->mResolved;
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
    //The master of a promise is the actual promise that gets resolved,
    //similar to the 'deferred' object in some promise libraries.
    //It contains the callbacks and the state. Promises that have a
    //master just forward the attached callbacks to the master. These promises
    //are generated during the chaining process - all of them attach to the
    //initial, master promise.
    const Promise<T>& getMaster() const
    {
        assert(mSharedObj);
        auto& master = mSharedObj->mMaster;
        auto& ret = master.mSharedObj ? master : *this;
        assert(!ret.hasMaster());
        return ret;
    }
    //non-const version of the method
    Promise<T>& getMaster()
    {
        assert(mSharedObj);
        auto& master = mSharedObj->mMaster;
        auto& ret = master.mSharedObj ? master : *this;
        assert(!ret.hasMaster());
        return ret;
    }

/** Creates a wrapper function around a then() or fail() handler that handles exceptions and propagates
 * the result to resolve/reject chained promises. \c In is the type of the callback's parameter,
 * \c Out is its return type, \c CB is the type of the callback itself.
 */
    template <typename In, typename Out, typename RealOut, class CB>
    ICallback<In>* createChainedCb(CB&& cb, Promise<Out>& next)
    {
        //cb must have the singature Promise<Out>(const In&)
        return createCb<In>(
            [cb](const In& result, ICallbackWithPromise<typename MaskVoid<In>::type, Out >& handler)
            mutable->void
        {
            Promise<Out>& next = handler.nextPromise; //the 'chaining' promise
            Promise<Out> promise((_Empty())); //the promise returned by the user callback
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

// connect the promise returned by the user's callback (actually its master)
// to the chaining promise, returned earlier by then() or fail()
            Promise<Out>& master = promise.getMaster(); //master is the promise that actually gets resolved, equivalent to the 'deferred' object
            assert(!next.hasMaster());
            next.mSharedObj->mMaster = master; //makes 'next' attach subsequently added callbacks to 'master'
            assert(next.hasMaster());
            // Move the callbacks and errbacks of 'next' to 'master'
            if (!master.hasCallbacks())
            {
                master.mSharedObj->mCbs = next.mSharedObj->mCbs;
                next.mSharedObj->mCbs = nullptr;
            }
            else
            {
                auto& nextCbs = next.thenCbs();
                if (nextCbs.count())
                    master.thenCbs().addListMoveItems(nextCbs);

                auto& nextEbs = next.failCbs();
                if (nextEbs.count())
                    master.failCbs().addListMoveItems(nextEbs);
            }
            //====
            if (master.mSharedObj->mPending)
                master.doPendingResolveOrFail();
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
        auto& master = mSharedObj->mMaster;
        if (master.mSharedObj) //if we are a slave promise (returned by then() or fail()), forward callbacks to our master promise
            return master.fail(std::forward<F>(eb));

        if (mSharedObj->mResolved == kSucceeded)
            return mSharedObj->mResult; //don't call the errorback, just return the successful resolve value

        Promise<T> next;
        std::unique_ptr<IFailCb> failCb(createChainedCb<Error, T,
            typename FuncTraits<F>::RetType>(std::forward<F>(eb), next));

        if (mSharedObj->mResolved == kFailed)
        {
            (*failCb)(mSharedObj->mError);
            mSharedObj->mError.setHandled();
        }
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

        mSharedObj->mResult = std::forward<V>(val);
        mSharedObj->mResolved = kSucceeded;

        if (hasCallbacks())
            doResolve(mSharedObj->mResult);
        else
            mSharedObj->mPending = true;
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
            throw std::runtime_error("Already resolved/rejected");

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
            (*static_cast<IFailCb*>(ebs.first()))(err);
            err.setHandled();
            for (int i=1; i<cnt; i++)
                (*static_cast<IFailCb*>(ebs[i]))(err);
        }
//propagate past success handlers till a fail handler is found
        auto& cbs = thenCbs();
        cnt = cbs.count();
        if (cnt)
        {
            cbs.first()->rejectNextPromise(err);
            for (int i=1; i<cnt; i++)
            {
//we dont know the type of the promise, but the interface to reject()
//has a fixed type, hence the reject() is a vitual function of the base class
//and we can reject any promise without knowing its type
                cbs[i]->rejectNextPromise(err);
            }
        }
    }
    void doPendingResolveOrFail()
    {
        if (!hasCallbacks())
            return;
        assert(mSharedObj->mPending);
        auto state = mSharedObj->mResolved;
        assert(state != kNotResolved);
        if (state == kSucceeded)
            doResolve(mSharedObj->mResult);
        else
        {
            assert(state == kFailed);
            doReject(mSharedObj->mError);
        }
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
    bool addLast = false;
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
        if (state->addLast && (n >= state->totalCount))
        {
            assert(n == state->totalCount);
            if (!state->output.done())
                state->output.resolve();
        }
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
        if (state->addLast && (n >= state->totalCount))
        {
            assert(n == state->totalCount);
            if (!state->output.done())
                state->output.resolve();
        }
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
    state->addLast = true;
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

template <class P>
inline Promise<void> when(std::vector<Promise<P>>& promises)
{
    if (promises.empty())
        return Void();

    WhenState state;
    size_t countMinus1 = promises.size()-1;
    for (size_t i=0; i < countMinus1; i++)
    {
        _when_add_single<P>(state, promises[i]);
    }
    state->addLast = true;
    _when_add_single<P>(state, promises[countMinus1]);
    return state->output;
}

}//end namespace promise
#endif
