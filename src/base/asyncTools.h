#ifndef _ASYNC_TOOLS_H
#define _ASYNC_TOOLS_H

#include <promise.h>
#include <gcmpp.h>
#include <type_traits>

class Loop
{
protected:
    bool mBreak = false;
    size_t mCount = 0;
public:
    void breakLoop() { mBreak = true; }
    size_t i() const { return mCount; }
    Loop(size_t aCount): mCount(aCount){}
};

template <class F, class C>
typename std::result_of<F(Loop&)>::type
asyncLoop(F&& func, C&& cond, size_t initial=0)
{
    typedef typename std::result_of<F(Loop&)>::type::Type P;
    struct State: public Loop
    {
        promise::Promise<P> mOutput;
        F mFunc;
        C mCondition;
        State(F&& aFunc, C&& aCond, size_t aCount): Loop(aCount), mFunc(std::forward<F>(aFunc)),
            mCondition(std::forward<C>(aCond)){}
        void nextIter()
        {
            mFunc(*this)
            .fail([this](const promise::Error& err)
            {
                mOutput.reject(err);
                delete this;
                return err;
            })
            .then([this](const P& result)
            {
                if (mBreak)
                    goto bail;

                mCount++;
                if (mCondition(mCount))
                {
                    mega::marshallCall([this](){nextIter();});
                    return;
                }
bail:
                mOutput.resolve(result);
                delete this;
            });
        }
    };

    if (!cond(initial))
        return promise::Promise<P>(P());

    auto state = new State(std::forward<F>(func), std::forward<C>(cond), initial);
    auto output = state->mOutput; //state may get deleted on the first nextIter()
    state->nextIter();
    return output;
}

#endif
