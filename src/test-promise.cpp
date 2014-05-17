//#define PROMISE_DEBUG_REFS 1
#include "promise.h"
#include <stdio.h>
#include "asyncTest.h"
#include "guiCallMarshaller.h"
#include "webrtcAdapter.h"

using namespace promise;

Promise<int> retPromise()
{
	return Promise<int>();
}

int main()
{
	rtcModule::init(NULL);
	auto devices = rtcModule::getInputDevices(rtcModule::DeviceManager());
	for (auto& dev: devices->audio)
		printf("Audio: %s\n", dev.name.c_str());

	for (auto& dev: devices->video)
		printf("Video: %s\n", dev.name.c_str());

  {
	Async async({{"then1",{{"order",1}}}, {"then2",{}}, {"fail2",{{"timeout",10000}}}});
	auto p = Promise<int>();
	p
	.then([&](const int& a)
	{
		printf("then1 called with %d\n", a);
		async.done("then1");
		Promise<int> p;
		async.schedCall([=]()mutable{p.resolve(10);});
		return p;
//	  return reject<int>("test error");
	})
	.fail([&](const Error& err)
	{
		printf("fail1 called with %s\n", err.msg().c_str());
		async.done("x-fail1");
		Promise<int> p;
		async.schedCall([p]()mutable{p.reject("asdasd");});
		return p;
	})

	.fail([&](const Error& err)
	{
		  printf("second fail in the row called\n");
		  async.done("x-fail2");
		  return reject<int>("third reject");
	})
	.then([&](int val)
	{
		  printf("then() after the row of fails called with %d\n", val);
		  async.done("then2");
		  Promise<int> p;
		  async.schedCall([=]()mutable{p.reject("error from then2");});
		  return p;
	})
	.fail([&](const Error& err)
	{
		  printf("final fail() wih error: %s\n", err.msg().c_str());
		  async.done("fail2");
		  return 0;
	});

	async.schedCall([p]()mutable{p.resolve(10);});
	int code = async.run();
	printf("Finished: %s\n", Async::completeCodeToString(code));
  }

  {
	Async async({"when"});
	Promise<bool> p1;
	Promise<std::string> p2;
	Promise<int> p3;
	when(p1,p2,p3).then([&](int)mutable
	{
		printf("when resolved\n");
		async.done("when");
		return 0;
	});
	async.schedCall([p1]()mutable
	{
		p1.resolve(1);
	});
	async.schedCall([p2]()mutable
	{
		p2.resolve("sdfsdf");
	});
	async.schedCall([p3]()mutable
	{
		p3.resolve(3);
	});


	int code = async.run();
	printf("Finished: %s\n", Async::completeCodeToString(code));
	getchar();
  }
}
