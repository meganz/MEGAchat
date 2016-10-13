#ifndef _ASYNC_TOOLS_H
#define _ASYNC_TOOLS_H

#include <promise.h>
#include <gcmpp.h>
#include <type_traits>

namespace async
{
template <class I>

template <class P>
promise::Promise<P> defaultVal() { return P(); }

template<>
promise::Promise<void> defaultVal() { return promise::Void(); }

template <class I>
class Loop
{
protected:
    bool mBreak = false;
public:
    Loop(I aInitial): i(aInitial){}
    void breakLoop() { mBreak = true; }
    I i;
};

template <class I, class X, class C, class F>
class StateBase: public Loop<I>
{
protected:
    typedef typename std::result_of<F(Loop&)>::type::Type P;
    promise::Promise<P> mOutput;
    C mCondition;
    X mIncrement;
    F mFunc;
    StateBase(I aInitial, C&& aCond, X&& aInc, F&& aFunc)
    : Loop<I>(aInitial), mCondition(std::forward<C>(aCond)),
      mIncrement(std::forward<X>(aInc)), mFunc(std::forward<F>(aFunc)){}
};

template <class I, class C, class X, class F, class V>
struct State: public StateBase<F,C>
{
    typedef StateBase<I,C,X,F> Base;
    using StateBase<I,C,X,F>::StateBase;
    void nextIter()
    {
        this->mFunc(*this)
        .fail([this](const promise::Error& err)
        {
            this->mOutput.reject(err);
            delete this;
            return err;
        })
        .then([this](typename Base::P result)
        {
            if (this->mBreak)
                goto bail;

            this->mIncrement();
            if (this->mCondition(this->i))
            {
                karere::marshallCall([this](){nextIter();});
                return;
            }
bail:
            this->mOutput.resolve(result);
            delete this;
        });
    }
};

template <class I, class C, class X, class F, class V>
struct State<I,C,X,F,V>: public StateBase<I,C,X,F>
{
    typedef StateBase<I,C,X,F> Base;
    using StateBase<I,C,X,F>::StateBase;
    void nextIter()
    {
        this->mFunc(*this)
        .fail([this](const promise::Error& err)
        {
            this->mOutput.reject(err);
            delete this;
            return err;
        })
        .then([this]()
        {
            if (this->mBreak)
                goto bail;

            this->mIncrement();
            if (this->mCondition(this->i))
            {
                karere::marshallCall([this](){nextIter();});
                return;
            }
bail:
            this->mOutput.resolve();
            delete this;
        });
    }
};

template <class I, class C, class X, class F>
typename std::result_of<F(Loop&)>::type
loop(I initial, C&& cond, X&& inc, F&& func)
{
    typedef typename std::result_of<F(Loop&)>::type::Type P;

    if (!cond(initial))
        return defaultVal<P>();

    auto state = new State<I,C,X,F,P>(
        initial, std::forward<C>(cond),
        std::forward<X>(inc), std::forward<F>(func)
    );

    //keep reference to state, as it may get deleted on the first nextIter()
    auto output = state->mOutput;
    state->nextIter();
    return output;
}
}

#endif
