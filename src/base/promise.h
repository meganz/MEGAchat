/** @author Alexander Vassilev */
// This is the js spec https://promisesaplus.com which is followed as reasonable in c++
// if you see numbers as a comment they refer to the same numbers in the spec


#ifndef _PROMISE_H
#define _PROMISE_H
#include <stdexcept>
#include <vector>
#include <string>
#include <utility>
#include <memory>
#include <algorithm>
#include <assert.h>

namespace promise
{
    class Error;
}
/** @brief The name of the unhandled promise error handler. This handler is
 * called when a promise fails, but the user has not provided a fail() callback
 * to handle that error. Often this is unintentional and results in the program
 * stepping execution for no obvious reason.
 * The user can define this to customize the unhandled error trap.
 * The default just prints a warning to stderr
 */
#ifndef PROMISE_ON_UNHANDLED_ERROR
    #define PROMISE_ON_UNHANDLED_ERROR promise::Error::defaultOnUnhandledError
#else // define the prototype, as it may come after the inclusion of promise.h
    void PROMISE_ON_UNHANDLED_ERROR(const promise::Error& error) noexcept;
#endif

#define PROMISE_LOG(fmtString,...) printf("promise: " fmtString"\n", ##__VA_ARGS__)
#ifdef PROMISE_DEBUG_REFS
    #define PROMISE_LOG_REF(fmtString,...) PROMISE_LOG(fmtString, ##__VA_ARGS__)
#else
    #define PROMISE_LOG_REF(fmtString,...)
#endif

namespace promise
{
template <class T>
using remove_cvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
template <class T, class V>
using EnableIfSameT = typename std::enable_if<std::is_same<remove_cvref_t<T>, V>::value>::type;
template <class T, class V>
using DisableIfSameT = typename std::enable_if<!std::is_same<remove_cvref_t<T>, V>::value>::type;
template <class T, class V>
using EnableIfConvertibleT = typename std::enable_if<std::is_convertible<T, V>::value>::type;
template <class T, class V>
using DisableIfConvertibleT = typename std::enable_if<!std::is_convertible<T, V>::value>::type;

enum
{
    kErrorTypePromiseSpecViolation = 2,
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

// 1.5
class Error
{
    std::string mMsg;
    int mCode;
    int mType;


public:

    template <class T, class = EnableIfSameT<T, std::string>>
    Error(T&& msg, int code = -1, int type = kErrorTypeGeneric)
        :mMsg(std::forward<T>(msg)),mCode(code),mType(type) {}
    template <class ... Args>
    Error(const char* msg, Args&&... args)
        : Error(std::string{ msg ? msg : "" }, std::forward<Args>(args)...) {}
    Error() = default;
    Error(const Error&) = default;
    Error& operator =(const Error&) = default;
    Error(Error&&) = default;
    Error& operator =(Error&&) = default;
    ~Error() = default;

    const std::string& msg() const noexcept { return mMsg; }
    const char* what() const { return msg().c_str(); }
    int type() const noexcept { return mType; }
    int code() const noexcept { return mCode; }
    std::string toString() const
    {
        return "Error: '" + msg() + "'\nType: " +
        std::to_string(type()) + " Code: " + std::to_string(code());
    }
    static void defaultOnUnhandledError(const Error& error) noexcept
    {
        fprintf(stderr, "WARNING: Unhandled promise fail. Error: '%s', type: %d, code: %d\n",
            error.what(), error.type(), error.code());
    }
};

//===
struct _Void {};
typedef _Void Void;

template<typename V>
struct MaskVoid { typedef V type; };
template<>
struct MaskVoid<void> { typedef _Void type; };
template <class T>
using MaskVoidT = typename MaskVoid<T>::type;

//get function/lambda return type, regardless of argument count and types
template <class F>
struct FuncTraits : public FuncTraits<decltype(&F::operator())> {};

template <class R, class... Args>
struct FuncTraits <R(*)(Args...)> { typedef R RetType; enum { nargs = sizeof...(Args) }; };

template <class C, class R, class...Args>
struct FuncTraits <R(C::*)(Args...)> { typedef R RetType; enum { nargs = sizeof...(Args) }; };

template <class C, class R, class...Args>
struct FuncTraits <R(C::*)(Args...) const> { typedef R RetType; enum { nargs = sizeof...(Args) }; };

template <class T>
class Promise;

enum ResolvedState
{
    kNotResolved = 0,
    kSucceeded = 1,
    kFailed = 2
};

class PromiseBase
{
public:

    virtual ~PromiseBase() {}


protected:

    struct CallCbHandleVoids
    {
        template<class CbOut, class In, class CB, class = typename std::enable_if<!std::is_same<In, _Void>::value && !std::is_same<CbOut, void>::value, int>::type>
        static CbOut call(CB& cb, const In& val) { return cb(val); }

        template<class CbOut, class In, class CB, class = typename std::enable_if<std::is_same<In, _Void>::value && !std::is_same<CbOut, void>::value, int>::type>
        static CbOut call(CB& cb, const _Void& /*val*/) { return cb(); }

        template<class CbOut, class In, class CB, class = typename std::enable_if<!std::is_same<In, _Void>::value && std::is_same<CbOut, void>::value, int>::type>
        static Void call(CB& cb, const In& val) { cb(val); return _Void(); }

        template<class CbOut, class In, class CB, class = typename std::enable_if<std::is_same<In, _Void>::value && std::is_same<CbOut, void>::value, int>::type>
        static Void call(CB& cb, const _Void& /*val*/) { cb(); return _Void(); }
    };

    template <typename Ret>
    struct RemovePromise
    {
        typedef typename std::remove_const<Ret>::type Type;
    };
    template<typename Ret>
    struct RemovePromise<Promise<Ret> >
    {
        typedef typename std::remove_const<Ret>::type Type;
    };
    template <class T>
    using RemovePromiseT = typename RemovePromise<T>::Type;

    template<class T>
    using RetTypeT = RemovePromiseT<typename FuncTraits<T>::RetType>;

    class SharedError : public Error
    {
        bool mHandled = false;


    public:

        template <class ... Args>
        SharedError(Args&&... args)
            : Error{ std::forward<Args>(args)... } {}

        ~SharedError()
        {
            if (!mHandled)
                PROMISE_ON_UNHANDLED_ERROR(*this);
        }

        void setHandled() noexcept
        {
            mHandled = true;
        }
    };
    using SharedErrorPtr = std::shared_ptr<SharedError>;

    template <class TResultType>
    class SharedData
    {
        using Type = TResultType;
        using Result = MaskVoidT<typename std::remove_const<Type>::type>;
        class Data
        {
            class FutureList
            {
                struct IFuture
                {
                    virtual ~IFuture() = default;
                    virtual void resolve(const Result& val) noexcept = 0;
                    virtual void reject(const SharedErrorPtr& errPtr) noexcept = 0;
                };

                using FuturePtr = std::unique_ptr<IFuture>;
                using Container = std::vector<FuturePtr>;
                Container mList;


            public:

                void push(FutureList&& other)
                {
                    std::move(other.mList.begin(), other.mList.end(), std::back_inserter(mList));
                }

                template <class TFuture, class ... Args>
                const typename TFuture::Promise& emplace(Args&&... args)
                {
                    struct TypeErasedFuture : IFuture
                    {
                        TFuture future;
                        void resolve(const Result& val) noexcept override
                        {
                            future.resolve(val);
                        }
                        void reject(const SharedErrorPtr& err) noexcept override
                        {
                            future.reject(err);
                        }
                        TypeErasedFuture(Args&&... args)
                            : future{ std::forward<Args>(args)... } {}
                    };
                    // should be std::make_unique<TypeErasedFuture>(std::forward<Args>(args)...);
                    // but it doesn't available in c++11
                    auto ptr = std::unique_ptr<TypeErasedFuture>(new TypeErasedFuture{ std::forward<Args>(args)... });
                    const auto& promise = ptr->future.promise();
                    mList.emplace_back(std::move(ptr));
                    return promise;
                }

                void reject(const SharedErrorPtr& err) noexcept
                {
                    Container list;
                    // 2.2.3.3 even if by mistake this method will be called recursively
                    // for the same promise while executing one of the future callback
                    // following line guaranteed to call each future once only
                    std::swap(list, mList);
                    // 2.2.6.2
                    std::for_each(list.begin(), list.end(), [&err](FuturePtr& futurePtr) noexcept
                    {
                        futurePtr->reject(err);
                    });
                }

                void resolve(const Result& val) noexcept
                {
                    Container list;
                    // 2.2.2.3
                    std::swap(list, mList);
                    // 2.2.6.1
                    std::for_each(list.begin(), list.end(), [&val](FuturePtr& futurePtr) noexcept
                    {
                        futurePtr->resolve(val);
                    });
                }
            };

            ResolvedState mState = kNotResolved;
            FutureList mFutures;
            // 2.1.2.2
            Result mResult;
            // 2.1.3.2
            SharedErrorPtr mError;

            void doResolvePendingFutures() noexcept { mFutures.resolve(mResult); }
            void doRejectPendingFutures() noexcept { mFutures.reject(mError); }


        public:

            template <class T>
            Data(T&& arg) { resolve(std::forward<T>(arg)); }
            template <class ... Args>
            Data(Args&&... args) { reject(std::forward<Args>(args)...); }

            Data() = default;
            ~Data() = default;
            Data(const Data&) = delete;
            Data(Data&&) = delete;
            Data& operator =(const Data&) = delete;
            Data& operator =(Data&&) = delete;

            ResolvedState state() const noexcept { return mState; }
            const SharedErrorPtr& error() const noexcept { return mError; }
            const Result& result() const noexcept { return mResult; }

            template <class TFuture, class ... Args>
            const typename TFuture::Promise& emplacePending(Args&&... args)
            {
                return mFutures.template emplace<TFuture>(std::forward<Args>(args)...);
            }

            void pushPendingFutures(Data&& promise)
            {
                // 2.2.6
                mFutures.push(std::move(promise.mFutures));
            }

            // 2.3.2
            void resolvePendingFutures() noexcept
            {
                switch (mState)
                {
                case kNotResolved: return; // nothing to do 2.3.2.1
                case kFailed: return doRejectPendingFutures(); // 2.3.2.2
                case kSucceeded: return doResolvePendingFutures(); // 2.3.2.3
                }
            }
            template <class T> EnableIfConvertibleT<T, Result>
            /*void*/ resolve(T&& val) noexcept
            {
                mState = kSucceeded;
                mResult = std::forward<T>(val);
                doResolvePendingFutures();
            }
            template <class T> DisableIfConvertibleT<T, Result>
            /*void*/ resolve(T&& err) noexcept
            {
                reject(std::forward<T>(err));
            }
            template <class T> EnableIfSameT<T, SharedErrorPtr>
            /*void*/ reject(T&& errPtr) noexcept
            {
                mState = kFailed;
                mError = std::forward<T>(errPtr);
                doRejectPendingFutures();
            }
            template <class T> EnableIfSameT<T, Error>
            /*void*/ reject(T&& err) noexcept
            {
                reject(std::make_shared<SharedError>(std::forward<T>(err)));
            }
            template <class ... Args>
            void reject(Args&&... args) noexcept
            {
                reject(std::make_shared<SharedError>(std::forward<Args>(args)...));
            }
        };
        using DataPtr = std::shared_ptr<Data>;

        DataPtr mData;

        // 2.3.2
        template <class T> EnableIfSameT<T, Promise<Type>>
        /*void*/ doResolve(T&& promise) noexcept
        {
            // 2.3.1
            if (promise.mSharedObj->mData == mData)
                reject("Promise must not be resolved by its self", kErrException, kErrorTypePromiseSpecViolation);

            // move our pending futures to the promise
            promise.mSharedObj->mData->pushPendingFutures(std::move(*mData));
            // adopting promise state, now there is no difference between this and promise
            mData = promise.mSharedObj->mData;
            mData->resolvePendingFutures();
        }
        // 2.3.4
        template <class T> DisableIfSameT<T, Promise<Type>>
        /*void*/ doResolve(T&& arg)
        {
            mData->resolve(std::forward<T>(arg));
        }


    public:

        SharedData() : mData{ std::make_shared<Data>() } {}
        SharedData(SharedData&&) = default;
        SharedData(const SharedData&) = default;
        SharedData& operator =(SharedData&&) = default;
        SharedData& operator =(const SharedData&) = default;
        ~SharedData() = default;

        template <class T, class = EnableIfSameT<T, Promise<Type>>>
        SharedData(T&& promise) : SharedData{ *promise.mSharedObj } {}
        template <class T, class ... Args, class = DisableIfSameT<T, SharedData>>
        SharedData(T&& arg, Args&&... args)
            : mData{ std::make_shared<Data>(std::forward<T>(arg), std::forward<Args>(args)...) } {}

        ResolvedState state() const noexcept { return mData->state(); }
        bool succeeded() const noexcept { return state() == kSucceeded; }
        bool failed() const noexcept { return state() == kFailed; }
        bool resolved() const noexcept { return state() != kNotResolved; }
        bool pending() const noexcept { return state() == kNotResolved; }
        const SharedErrorPtr& error() const noexcept
        {
            assert(failed());
            return mData->error();
        }
        const Result& result() const noexcept
        {
            assert(succeeded());
            return mData->result();
        }

        // 2.3
        template <class T>
        void resolve(T&& val)
        {
            // 2.3.3.3.3
            if (resolved())
                return;
            // 2.2.7.1
            doResolve(std::forward<T>(val));
        }
        template <class ... Args>
        void reject(Args&&... args)
        {
            // 2.3.3.3.3
            if (resolved())
                return;
            // 2.2.7.2
            mData->reject(std::forward<Args>(args)...);
        }

        // 2.2.2
        template <typename F>
        SharedData<RetTypeT<F>> then(F&& cb)
        {
            using Out = RetTypeT<F>;
            struct ThenFuture : CallbackFuture<Result, Out, F>
            {
                using Base = CallbackFuture<Result, Out, F>;

                const SharedData<Out>& resolve(const Result& val) noexcept
                {
                    // 2.2.7.1
                    Base::resolve(val);
                    return Base::promise();
                }
                void reject(const SharedErrorPtr& errPtr) noexcept
                {
                    // 2.2.7.4
                    Base::reject(errPtr);
                }
                ThenFuture(F&& cb)
                    : Base(std::forward<F>(cb)) {}
            };

            switch (mData->state())
            {
                // 2.2.7.4 sharing error state with resulting promise
            case kFailed: return mData->error();
                // 2.2.6.1
            case kSucceeded: return ThenFuture(std::forward<F>(cb)).resolve(mData->result());
                // 2.2.2.2
            case kNotResolved: return mData->template emplacePending<ThenFuture>(std::forward<F>(cb));
            default: throw std::logic_error("unknown promise state");
            }
        }

        // 2.2.3
        template <typename F>
        SharedData fail(F&& eb)
        {
            struct FailFuture : CallbackFuture<Error, Type, F>
            {
                using Base = CallbackFuture<Error, Type, F>;

                void resolve(const Result& val) noexcept
                {
                    // 2.2.7.3
                    Base::fullfill(val);
                }
                const SharedData& reject(const SharedErrorPtr& errPtr) noexcept
                {
                    // 2.2.7.1
                    Base::resolve(*errPtr);
                    errPtr->setHandled();
                    return Base::promise();
                }
                FailFuture(F&& cb)
                    : Base{ std::forward<F>(cb) } {}
            };

            switch (mData->state())
            {
                // 2.2.7.3 don't call the error callback, just return the successful resolve value
            case kSucceeded: return *this;
                // 2.2.6.2
            case kFailed: return FailFuture(std::forward<F>(eb)).reject(mData->error());
                // 2.2.3.2
            case kNotResolved: return mData->template emplacePending<FailFuture>(std::forward<F>(eb));
            default: throw std::logic_error("unknown promise state");
            }
        }
    };

    template <class In, class Out>
    class Future
    {
        SharedData<Out> mPromise;


    public:

        using Promise = SharedData<Out>;

        const Promise& promise() const noexcept { return mPromise; }
        template <class T>
        void fullfill(T&& val) noexcept
        {
            mPromise.resolve(std::forward<T>(val));
        }
        template <class ... Args>
        void reject(Args&&... args) noexcept
        {
            mPromise.reject(std::forward<Args>(args)...);
        }
    };

    template <class In, class Out, class CB>
    class CallbackFuture : public Future<In, Out>
    {
        remove_cvref_t<CB> mCb;


    public:

        using Future<In, Out>::fullfill;
        using Future<In, Out>::reject;

        template <class F>
        CallbackFuture(F&& cb) 
            : mCb{ std::forward<F>(cb) } {}

        // 2.3.3
        void resolve(const In& arg) noexcept
        {
            try
            {
                using RealOut = typename FuncTraits<CB>::RetType;
                // NOTE that at the time of calling callback the promise could already
                // being resolved in this case the resulting value or exception
                // got by calling callback is ignored. That is what Promise/A+ specify
                // see 2.3.3.3.3
                fullfill(CallCbHandleVoids::template call<RealOut, In>(mCb, arg));
            }
            // 2.3.3.3.4
            catch (std::exception& e)
            {
                reject(e.what(), kErrException);
            }
            catch (Error& e)
            {
                reject(e);
            }
            catch (const char* e)
            {
                reject(e, kErrException);
            }
            catch (...)
            {
                reject("(unknown exception type)", kErrException);
            }
        }
    };
};

template<typename TResultType>
class Promise : public PromiseBase
{
    static_assert(!std::is_reference<TResultType>::value, "Promise can't hold a reference");
    static_assert(!std::is_same<typename std::remove_const<TResultType>::type, Error>::value,
        "promise can't hold an Error");
    using SharedObj = SharedData<TResultType>;
    friend class SharedData<TResultType>;

    // Effectively this is double shared_ptr. Original implementation used an idea of
    // master promise. This is basically the same idea but if the master promise is always
    // presented. This eliminates a need to constantly check do we have master promise.
    std::shared_ptr<SharedObj> mSharedObj;


public:

    typedef TResultType Type;

    Promise(Promise&& other) = default;
    Promise(const Promise& other) = default;
    Promise& operator=(const Promise& other) = default;
    Promise& operator=(Promise&& other) = default;
    ~Promise() = default;

    Promise() 
        : mSharedObj{ std::make_shared<SharedObj>() } {};
    template <class T, class = DisableIfSameT<T, Promise>>
    Promise(T&& arg)
        : mSharedObj{ std::make_shared<SharedObj>(std::forward<T>(arg)) } {}
    template <class ... Args>
    explicit Promise(Args&&... args)
        : mSharedObj{ std::make_shared<SharedObj>(std::forward<Args>(args)...) } {}

    ResolvedState done() const noexcept { return mSharedObj->state(); }
    bool succeeded() const noexcept { return  mSharedObj->succeeded(); }
    bool failed() const noexcept { return mSharedObj->failed(); }
    bool resolved() const noexcept { return mSharedObj->resolved(); }
    bool pending() const noexcept { return mSharedObj->pending(); }
    const Error& error() const
    {
        if (pending())
            throw std::runtime_error("Promise is pending");
        return *mSharedObj->error();
    }
    template <class RetType = Type>
    const typename std::enable_if<!std::is_same<RetType, void>::value, RetType>::type& value() const
    {
        if (pending())
            throw std::runtime_error("Promise is pending");
        return mSharedObj->result();
    }

    // 2.2.1.2 2.2.7
    template <typename F>
    Promise<RetTypeT<F>> then(F&& cb)
    {
        using RetType = RetTypeT<F>;
        static_assert(!std::is_same<RetType, Error>::value,
            "then continuation must not return an Error, instead it should return rejected promise or throw an exception");

        return mSharedObj->then(std::forward<F>(cb));
    }
    // 2.2.1.1 2.2.7
    template <typename F>
    Promise fail(F&& eb)
    {
        using RetType = RetTypeT<F>;
        // on fail callback must provide RECOVERY semantics it means that
        // the return type of eb MUST be convertible to current promise type or be Error.
        // In the later case it means that eb "rethrowing" an error (most possibly the current).
        static_assert(std::is_convertible<RetType, Type>::value
            || std::is_same<RetType, Error>::value, "read comment above");

        return mSharedObj->fail(std::forward<F>(eb));
    }

    template <class T>
    void resolve(T&& val)
    {
        if (resolved())
            throw std::runtime_error("Already resolved/rejected");
        mSharedObj->resolve(std::forward<T>(val));
    }
    template <class T = Type, class = EnableIfSameT<T, void>>
    void resolve()
    {
        resolve(Void{});
    }
    template <class ... Args>
    void reject(Args&&... args)
    {
        if (resolved())
            throw std::runtime_error("Already resolved/rejected");
        mSharedObj->reject(std::forward<Args>(args)...);
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
