/**
  @file Async unit testing framework
  @author Alexander Vassilev, July 2015
*/

#ifndef ASYNCTEST_H
#define ASYNCTEST_H

#include <vector>
#include <string>
#include <functional>

namespace test
{
//need to declare the color vars before including the event loop header
extern const char* kColorTag;
extern const char* kColorSuccess;
extern const char* kColorFail;
extern const char* kColorNormal;
extern const char* kColorWarning;
}
#define TEST_HAVE_COLOR_VARS
#include "asyncTest.h"

#define TEST_LOG_NO_EOL(fmtString,...) printf(fmtString, ##__VA_ARGS__)
#define TEST_LOG(fmtString,...) TEST_LOG_NO_EOL(fmtString "\n", ##__VA_ARGS__)


#define TESTS_INIT() \
namespace test { \
    unsigned gNumFailed = 0;          \
    unsigned gNumTests = 0;           \
    unsigned gNumDisabled = 0;        \
    unsigned gNumTestGroups = 0;      \
    Ts gTotalExecTime = 0;            \
    const char* kColorTag = "";       \
    const char* kColorSuccess = "";   \
    const char* kColorFail = "";      \
    const char* kColorNormal = "";    \
    const char* kColorWarning = "";   \
    int gDefaultDoneTimeout = 2000;   \
    struct TestInitializer {          \
        TestInitializer() { srand(time(nullptr)); Test::initColors(); }    \
        ~TestInitializer() { Test::printTotals(); } \
    };                                               \
    TestInitializer _gsTestInit;                     \
}

namespace test
{
class TestGroup;
typedef long long Ts;
/** Used to signal an error and immediately bail out, but not report the error
 * as exception from user code. Usage: instead of error() when one wants to bail out
 * of the test. \c error() followed by \c return may not do the job if we are inside
 * a lambda. Still, an exception may also not work in all cases, when the lambda is
 * run inside an exception handler, such as promise callback, where an exception will
 * not cause exit from the test
 */
struct BailoutException: public std::runtime_error
{  BailoutException(const std::string& msg): std::runtime_error(msg){} };

extern unsigned gNumFailed;
extern unsigned gNumTests;
extern unsigned gNumDisabled;
extern unsigned gNumTestGroups;
extern Ts gTotalExecTime;

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

class ITestBody
{
public:
    virtual void call() = 0;
    ~ITestBody(){}
};

class Test
{
    std::function<void()> cleanup;
public:
    TestGroup& group;
    std::string name;
    std::unique_ptr<ITestBody> body;
    std::string errorMsg;
    Ts execTime = 0;
    std::unique_ptr<EventLoop> loop;
    bool isDisabled = false;
//===
    constexpr static const char* kLine =     "====================================================";
    constexpr static const char* kThinLine = "----------------------------------------------------";

    template<class CB>
    inline Test(TestGroup& parent, const std::string& aName, CB&& aBody, EventLoop* aLoop=nullptr);
    void error(const std::string& msg)
    {
        if (!errorMsg.empty())
            return;
        gNumFailed++;
        errorMsg = kColorFail;
        errorMsg.append("fail").append(kColorNormal)
                .append(kColorTag).append(" '").append(name).append(kColorNormal)
                .append("' (").append(std::to_string(execTime)).append(" ms)")
                .append("\n* * * ").append(msg);
        TEST_LOG("%s", errorMsg.c_str());
        if (loop)
            loop->abort();
    }
    template <class...Args>
    void done(Args... args) { loop->done(args...); }

    inline void run();
    inline Test& disable();
    bool hasError() const { return !errorMsg.empty(); }
    static void printTotals()
    {
        TEST_LOG("%s", kLine);
        if (!gNumFailed)
            TEST_LOG("All %u tests in %u groups %spassed%s (%lld ms)",
                gNumTests-gNumDisabled, gNumTestGroups, kColorSuccess, kColorNormal, gTotalExecTime);
        else
            TEST_LOG("Some tests failed: %u %sfailed%s / %u total in %u group%s (%lld ms)",
                gNumFailed, kColorFail, kColorNormal, gNumTests-gNumDisabled,
                gNumTestGroups, (gNumTestGroups==1)?"":"s", gTotalExecTime);
        if (gNumDisabled)
            TEST_LOG("(%u tests DISABLED)", gNumDisabled);
        TEST_LOG("%s", kLine);
    }
    static inline Ts getTimeMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    static void initColors()
    {
        if (!isatty(1))
            return;
        kColorSuccess = "\033[1;32m";
        kColorFail = "\033[1;31m";
        kColorNormal = "\033[0m";
        kColorTag = "\033[34m";
        kColorWarning = "\033[33m";
    }
    void doCleanup()
	{
        if (!cleanup)
            return;
		try
		{
            cleanup();
		}
        catch (BailoutException& e)
        {  error(std::string("Error during cleanup: ") + e.what());  }
		catch(std::exception& e)
        {  error(std::string("Exception during cleanup: ") + e.what());  }
		catch(...)
        {  error("Non-standard exception during cleanup");  }
	}
};

template <class CB>
class TestBody: public ITestBody
{
protected:
    Test& mTest;
    CB mCb;
    template <class E=CB>
    typename std::enable_if<FuncTraits<E>::nargs == 1, void>::type doCall()
    {  mCb(mTest);  }
    template <class E=CB>
    typename std::enable_if<FuncTraits<E>::nargs == 2, void>::type doCall()
    {  mCb(mTest, *mTest.loop);  }
public:
    TestBody(Test& aTest, CB&& cb): mTest(aTest), mCb(std::forward<CB>(cb)){}
    virtual void call(){ doCall(); }
};

template <class CB>
Test::Test(TestGroup& parent, const std::string& aName, CB&& aBody, EventLoop* aLoop)
    :group(parent), name(aName), body(new TestBody<CB>(*this, std::forward<CB>(aBody))),
     loop(aLoop)
{
    gNumTests++;
}

class TestGroup
{
public:
    typedef std::vector<std::shared_ptr<Test> > TestList;
	std::string name;
    std::string errorMsg;
	TestList tests;
    unsigned numErrors = 0;
    unsigned numDisabled = 0;
    unsigned numTests = 0;
    Ts execTime = 0;
    std::function<void(Test&)> beforeEach;
    std::function<void(Test&)> afterEach;
    std::function<void()> allCleanup;
    std::function<void(TestGroup&)> body;

    template <class CB>
    Test& addTest(std::string&& name, EventLoop* aLoop, CB&& lambda)
	{
        tests.emplace_back(std::make_shared<Test>(
            *this, std::forward<std::string>(name), std::forward<CB>(lambda), aLoop));
        return *tests.back();
	}
    template <class CB>
    TestGroup(const std::string& aName, CB&& aBody)
        :name(aName), body(std::forward<CB>(aBody))
    {
        gNumTestGroups++;
        run();
    }
    void run()
	{
        TEST_LOG("%s", Test::kLine);
		try
		{
            body(*this);
            numTests = tests.size() - numDisabled;
            TEST_LOG_NO_EOL("RUN   Group '%s%s%s' (%u test%s", kColorTag,
                name.c_str(), kColorNormal, numTests, (numTests == 1) ? "" : "s");
            if (numDisabled)
            {
                TEST_LOG_NO_EOL(", %u disabled", numDisabled);
            }
            TEST_LOG(")...\n%s", Test::kThinLine);
		}
        catch(BailoutException& e)
        {
            error(std::string("Error at test group setup: ")+e.what());
            return;
        }
		catch(std::exception& e)
		{
            error(std::string("Exception during test group setup:")+e.what());
			return;
		}
		catch(...)
		{
            error("Non-standard exception during test group setup");
			return;
		}
        for (auto& test: tests)
		{
            if (test->isDisabled)
            {
                TEST_LOG("%sdis%s  '%s%s%s'\n%s", kColorWarning, kColorNormal,
                    kColorTag, test->name.c_str(), kColorNormal, Test::kThinLine);
                continue;
            }
            test->run();
            TEST_LOG("%s", Test::kThinLine);
            execTime += test->execTime;
            if (test->hasError())
            {
                error(test->errorMsg);
            }
        }

		if (allCleanup)
        {
            try
            {
                allCleanup();
            }
            catch(BailoutException& e)
            { error(e.what()); }
            catch(std::exception& e)
            {  error(std::string("Exception in cleanup of test group: ")+e.what());  }
            catch(...)
            {  error("Non standard exception in cleanup of test group");  }
        }
        printSummary();
    }
    bool hasError() const { return !errorMsg.empty(); }
    void error(const std::string& msg)
    {
        numErrors++;
        if (hasError())
            return;
        errorMsg = msg;
	}
    void printSummary()
    {
        if (!numErrors)
        {
            TEST_LOG("%sPASS%s  Group '%s%s%s': 0 errors / %u test%s (%lld ms)",
                kColorSuccess, kColorNormal, kColorTag, name.c_str(), kColorNormal,
                numTests, (numTests==1)?"":"s", execTime);
        }
        else
        {
            TEST_LOG("%sFAIL%s  Group '%s%s%s': %u error%s / %u test%s (%lld ms)",
                kColorFail, kColorNormal, kColorTag, name.c_str(), kColorNormal,
                numErrors, (numErrors==1)?"":"s", numTests,
                (numTests==1)?"":"s", execTime);
        }
    }

};
//we need the complete definition of class TestGroup, so we defined run() outside the Test class
void Test::run()
{
    TEST_LOG("run  '%s%s%s'...", kColorTag, name.c_str(), kColorNormal);
    const char* execState = "'before-each'";
    Ts start = 0;
    try
    {
        if (group.beforeEach)
            group.beforeEach(*this);

        start = getTimeMs();
        if (loop)
        {
            execState = nullptr; //dont log error location
            loop->schedCall([this]()
            {
                body->call();
            });
            loop->run();
            execTime = getTimeMs() - start;
            if (!loop->errorMsg.empty())
                error(loop->errorMsg);
        }
        else
        {
            execState = nullptr;
            body->call();
            execTime = getTimeMs() - start;
        }
    }
    catch(BailoutException& e)
    {
        execTime = getTimeMs() - start;
        if (execState)
            error(std::string("Error during ")+execState+": "+e.what());
        else
            error(e.what());
    }
    catch(std::exception& e)
    {
        execTime = getTimeMs() - start;
        if (execState)
            error(std::string("Exception during ")+execState+": "+e.what());
        else
            error(std::string("Exception: ")+e.what());
    }
    catch(...)
    {
        execTime = getTimeMs() - start;
        error(std::string("Non-standard exception during ")+execState);
    }
    gTotalExecTime += execTime;
    doCleanup();
    if (group.afterEach)
    {
        try { group.afterEach(*this); } catch(...){}
    }
    if(errorMsg.empty())
    {
        TEST_LOG("%spass%s '%s%s%s' (%lld ms)", kColorSuccess, kColorNormal,
                 kColorTag, name.c_str(), kColorNormal, execTime);
    }
}
inline Test& Test::disable()
{
    isDisabled = true;
    gNumDisabled++;
    group.numDisabled++;
    return *this;
}

} //end namespace

#define TEST_DO_TOKENPASTE(a, b) a##b
#define TEST_TOKENPASTE(a, b) TEST_DO_TOKENPASTE(a,b)
#define TEST_STRLITERAL2(a) #a
#define TEST_STRLITERAL(a) TEST_STRLITERAL2(a)

#define TestGroup(name)\
    TEST_TOKENPASTE(test::TestGroup group, __LINE__) (name, [&](test::TestGroup& group)

#define syncTest(name)\
    group.addTest(name, nullptr, [&](test::Test& test)

#define asyncTest(name,...)\
    group.addTest(name, new test::EventLoop(__VA_ARGS__), [&](test::Test& test, test::EventLoop& loop)


//check convenience macros

#define check(cond) \
do {                                 \
  if ((cond)) break;                 \
  static const char* msg = "check(" #cond ") failed at " __FILE__ \
  ":" TEST_STRLITERAL(__LINE__);    \
  test.error(msg);                   \
  throw test::BailoutException(msg); \
} while(0)

#define doneOrError(cond, name) do { check((cond)); loop.done(name); } while(0)

#endif // ASYNCTEST_H
