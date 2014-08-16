#ifndef ASYNCTEST_H
#define ASYNCTEST_H

#include<vector>
class Test
{
	std::function<void(const std::exception*)> cleanup;
	std::function<void()> body;
	Test(const std::string& aName)
		:name(aName){}
	void run()
	{
		try
		{
			body();
		}
		catch(std::exception& e)
		{
			TEST_LOG("EXCEPTION in test '%s':\n", name.c_str(), e.what());
			doCleanup(&e);
			continue;
		}
		catch(std::exception& e)
		{
			TEST_LOG("Non-standard EXCEPTION in test '%s'", name.c_str());
			doCleanup(&e);
			continue;
		}
		if (cleanup)
			doCleanup(NULL);
	}

	void doCleanup(std::exception* excep)
	{
		try
		{
			cleanup(excep);
		}
		catch(std::exception& e)
		{TESTS_LOG("EXCEPTION during test '%s' cleanup:\n%s", name.c_str(), e->what());	}
		catch(...)
		{TESTS_LOG("NOn-standard EXCEPTION during test '%s' cleanup", name.c_str());}
	}
};

class Scenario
{
	typedef std::vector<Test*> TestList;
	std::string name;
	TestList tests;
	std::function<void()> beforeEach;
	std::function<void()> allCleanup;
	std::function<void()> body;
	void addTest(Test* t, std::function<void()>&& lambda)
	{
		t->beforeEach = beforeEach;
		t->body = lambda;
		mTests.push(t);
	}
	Scenario(std::string& aName)
	 :name(aName){}
	initAndExecute(std::function<void()>&& aBody)
	{
		body = aBody;
		try
		{
			body();
		}
		catch(std::exception& e)
		{
			TESTS_LOG("EXCEPTION while parsing scenario '%s':\n%s", name.c_str(), e.what());
			return;
		}
		catch(...)
		{
			TESTS_LOG("Non-standard exception while parsing scenarion '%s'", name.c_str());
			return;
		}
		for (int i=0; i<mTests.size(); i++)
		{
			Test& t = mTests[i];
			test = t;
			t.run();
		}
		if (allCleanup)
			allCleanup();
	}
};

#define Describe(name)\
{\
	Scenario group;\
	group.initAndExecute(#name, [&]

#define it(name)\
	Test& test = group.test;
	group.addTest(#name, [&]

#endif // ASYNCTEST_H
