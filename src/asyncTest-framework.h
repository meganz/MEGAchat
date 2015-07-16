#ifndef ASYNCTEST_H
#define ASYNCTEST_H
#include<vector>
#include "asyncTest.h"

#define TEST_LOG_NO_EOL(fmtString,...) printf(fmtString, ##__VA_ARGS__)
#define TEST_LOG(fmtString,...) TEST_LOG_NO_EOL(fmtString "\n", ##__VA_ARGS__)


#define TESTS_INIT() \
namespace test { \
    int gNumFailed = 0;               \
    int gNumTests = 0;                \
    int gNumDisabled = 0;             \
    int gNumTestGroups = 0;           \
    Ts gTotalExecTime = 0;            \
    const char* kColorTag = "";       \
    const char* kColorSuccess = "";   \
    const char* kColorFail = "";      \
    const char* kColorNormal = "";    \
    const char* kColorWarning = "";   \
    struct TestInitializer {          \
        TestInitializer() { Test::initColors(); }    \
        ~TestInitializer() { Test::printTotals(); } \
    };                                               \
    TestInitializer _gsTestInit;                     \
}

namespace test
{
class Scenario;
typedef long long Ts;

extern const char* kColorTag;
extern const char* kColorSuccess;
extern const char* kColorFail;
extern const char* kColorNormal;
extern const char* kColorWarning;
extern int gNumFailed;
extern int gNumTests;
extern int gNumDisabled;
extern int gNumTestGroups;
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
    std::unique_ptr<ITestBody> body;
public:
    Scenario& scenario;
    std::string name;
    std::string errorMsg;
    Ts execTime = 0;
    std::unique_ptr<EventLoop> loop;
    bool isDisabled = false;
//===
    constexpr static const char* kLine =     "====================================================";
    constexpr static const char* kThinLine = "----------------------------------------------------";

    template<class CB>
    inline Test(Scenario& parent, const std::string& aName, CB&& aBody, EventLoop* aLoop=nullptr);
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
    }
    inline void run();
    inline Test& disable();
    bool hasError() const { return !errorMsg.empty(); }
    static void printTotals()
    {
        TEST_LOG("%s", kLine);
        if (!gNumFailed)
            TEST_LOG("All %d tests in %d groups %spassed%s (%lld ms)",
                gNumTests-gNumDisabled, gNumTestGroups, kColorSuccess, kColorNormal, gTotalExecTime);
        else
            TEST_LOG("Some tests failed: %d %sfailed%s / %d total in %d group%s (%lld ms)",
                gNumFailed, kColorFail, kColorNormal, gNumTests-gNumDisabled,
                gNumTestGroups, (gNumTestGroups==1)?"":"s", gTotalExecTime);
        if (gNumDisabled)
            TEST_LOG("(%d tests DISABLED)", gNumDisabled);
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
		catch(std::exception& e)
        {
            error(std::string("Exception during cleanup: ")+e.what());
        }
		catch(...)
        {
            error("Exception during cleanup");
        }
	}
};

template <class CB>
class TestBody: public ITestBody
{
protected:
    CB mCb;
    Test& mTest;
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
Test::Test(Scenario& parent, const std::string& aName, CB&& aBody, EventLoop* aLoop)
    :scenario(parent), name(aName), body(new TestBody<CB>(*this, std::forward<CB>(aBody))),
     loop(aLoop)
{
    gNumTests++;
}

class Scenario
{
public:
    typedef std::vector<std::shared_ptr<Test> > TestList;
	std::string name;
    std::string errorMsg;
	TestList tests;
    int numErrors = 0;
    int numDisabled = 0;
    int numTests = 0;
    Ts execTime = 0;
    std::function<void(Test&)> beforeEach;
	std::function<void()> allCleanup;
    std::function<void(Scenario&)> body;

    template <class CB>
    Test& addTest(std::string&& name, EventLoop* aLoop, CB&& lambda)
	{
        tests.emplace_back(std::make_shared<Test>(
            *this, std::forward<std::string>(name), std::forward<CB>(lambda), aLoop));
        return *tests.back();
	}
    template <class CB>
    Scenario(const std::string& aName, CB&& aBody)
        :name(aName), body(std::forward<CB>(aBody))
    {
        gNumTestGroups++;
        run();
    }
    void run()
	{
        TEST_LOG("%s", Test::kLine);
        TEST_LOG_NO_EOL("RUN   Group '%s%s%s'", kColorTag, name.c_str(), kColorNormal);
		try
		{
            body(*this);
            numTests = tests.size() - numDisabled;
            TEST_LOG_NO_EOL(" (%zu test%s", numTests, (numTests == 1) ? "" : "s");
            if (numDisabled)
            {
                TEST_LOG_NO_EOL(", %d disabled", numDisabled);
            }
            TEST_LOG(")...\n%s", Test::kThinLine);
		}
		catch(std::exception& e)
		{
            error("EXCEPTION while setting up scenario '"+name+"':"+e.what());
			return;
		}
		catch(...)
		{
            error("Non-standard exception while setting up scenario '"+name+"'");
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
            catch(std::exception& e)
            {
                error("EXCEPTION in cleanup of scenario '"+name+"':"+e.what());
            }
            catch(...)
            {
                error("Non standard exception in cleanup of scenarion '"+name+"'");
            }
        }
        printSummary();
    }
    bool hasError() const { return !errorMsg.empty(); }
    void error(const std::string& msg)
    {
        if (hasError())
            return;
        numErrors++;
        errorMsg = msg;
	}
    void printSummary()
    {
        if (!numErrors)
        {
            TEST_LOG("%sPASS%s  Group '%s%s%s': 0 errors / %zu test%s (%lld ms)",
                kColorSuccess, kColorNormal, kColorTag, name.c_str(), kColorNormal,
                numTests, (numTests==1)?"":"s", execTime);
        }
        else
        {
            TEST_LOG("%sFAIL%s  Group '%s%s%s': %d error%s / %d test%s (%lld ms)",
                kColorFail, kColorNormal, kColorTag, name.c_str(), kColorNormal,
                numErrors, (numErrors==1)?"":"s", numTests,
                (numTests==1)?"":"s", execTime);
        }
    }

};
//we need the complete definition of class Scenario, so we defined run() outside the Test class
void Test::run()
{
    TEST_LOG("run  '%s%s%s'...", kColorTag, name.c_str(), kColorNormal);
    const char* execState = "before-each";
    Ts start = 0;
    try
    {
        if (scenario.beforeEach)
            scenario.beforeEach(*this);

        start = getTimeMs();
        if (loop)
        {
            execState = "eventloop-setup";
            body->call();
            execState = nullptr; //dont add info
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
    catch(std::exception& e)
    {
        execTime = getTimeMs() - start;
        if (execState)
            error(std::string("Exception in ")+execState+": "+e.what());
        else
            error(std::string("Exception: ")+e.what());

    }
    catch(...)
    {
        execTime = getTimeMs() - start;
        error(std::string("Non-standard exception in ")+execState);
    }
    gTotalExecTime += execTime;
    doCleanup();
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
    scenario.numDisabled++;
    return *this;
}

} //end namespace

#define TEST_DO_TOKENPASTE(a, b) a##b
#define TEST_TOKENPASTE(a, b) TEST_DO_TOKENPASTE(a,b)
#define TestGroup(name)\
    TEST_TOKENPASTE(test::Scenario scenario, __LINE__) (#name, [&](test::Scenario& group)

#define it(name)\
    group.addTest(#name, nullptr, [&](test::Test& test)

#define async(name,...)\
    group.addTest(#name, new EventLoop(__VA_ARGS__), [&](test::Test& test, EventLoop& loop)

#endif // ASYNCTEST_H
