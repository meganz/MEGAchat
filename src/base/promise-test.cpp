//#define TESTLOOP_LOG_DONES
//#define TESTLOOP_DEBUG
//#define TESTLOOP_DEFAULT_DONE_TIMEOUT 4000
#include <asyncTest-framework.h>
#include <promise.h>
TESTS_INIT();
using namespace promise;
int main()
{

TestGroup("general")
{
  group.beforeEach = [](test::Test&) { printf("before each\n");};
  asyncTest("Promise<int>: resolve->then(ret error)->fail(recover)->then",
  {{"fail", "order", 2}, {"then1", "order", 1}, {"then2", "order", 3}})
  {
      Promise<int> pms;
      pms
      .then([&](int x)->Promise<int>
      {
          doneOrError(x == 12345678, "then1");
          Promise<int> ret;
          loop.schedCall([ret]() mutable
              { ret.reject("test error message"); }, 100);
          return ret;
      })
      .fail([&](const Error& err) mutable
      {
          doneOrError(err.msg() == "test error message", "fail");
          Promise<int> ret;
          loop.schedCall([ret]() mutable
              {  ret.resolve(87654321); }, 100);
          return ret;
      })
      .then([&](int x) mutable
      {
          doneOrError(x == 87654321, "then2");
      });

      loop.schedCall([pms]() mutable
          {  pms.resolve(12345678);  }, 100);
  });

  asyncTest("Promise<void>: resolve->then(ret error)->fail(recover)->then",
  {{"fail", "order", 2}, {"then1", "order", 1}, {"then2", "order", 3}})
  {
      Promise<void> pms;
      pms
      .then([&]()
      {
          loop.done("then1");
          Promise<void> ret;
          loop.schedCall([ret]() mutable
              {  ret.reject("test error message");  }, 100);
          return ret;
      })
      .fail([&](const Error& err) mutable
      {
          doneOrError(err.msg() == "test error message", "fail");
          Promise<void> ret;
          loop.schedCall([ret]() mutable
              {  ret.resolve();  }, 100);
          return ret;
      })
      .then([&]() mutable
      {
          loop.done("then2");
      });

      loop.schedCall([pms]() mutable
          {  pms.resolve();  }, 100);
  });
  asyncTest("rejected should skip all then()-s", {"fail1"})
  {
      Promise<void> pms;
      pms.then([&]()
      {
          test.error("then() must not be called by a rejected promise");
      })
      .then([&]()
      {
          test.error("second then() must not be called by a rejected promise");
      })
      .fail([&](const Error& err)
      {
          doneOrError(err.msg() == "test message", "fail1");
      });
      pms.reject("test message");
  });

  asyncTest("change type", {"then1", "then2", "then-final"})
  {
      Promise<int> pms;
      pms.then([&](int a)
      {
          doneOrError(a == 1212, "then1");
      })
      .then([&]()
      {
          loop.done("then2");
          return 1;
      })
      .then([&](int a)
      {
          doneOrError(a == 1, "then-final");
      });
      pms.resolve(1212);
  });
  asyncTest("should default-return resolved Promise<void>", {"then1", "then-final"})
  {
      Promise<int> pms;
      pms.then([&](int a) mutable
      {
          doneOrError(a == 1234, "then1");
      })
      .then([&]() mutable
      {
          test.done("then-final");
      });
      pms.resolve(1234);
  });
  asyncTest("should propagete failure through void callback (r180f82)", {"then"})
  {
      Promise<int> pms;
      pms.then([&](int a)
      {
          return Promise<void>(Error("message"));
      })
      .then([&]() mutable
      {
          test.error("then() called by a rejected promise");
      })
      .fail([&](const Error& err) mutable
      {
          doneOrError(err.msg() == "message", "then");
      });
      loop.schedCall([pms]() mutable { pms.resolve(1); });
  });
  asyncTest("return <resolved>.then() from then() should propagate (r6f550b)")
  {
      Promise<int> pms;
      pms.then([&](int a)
      {
          Promise<int> pms1(1);
          return pms1
          .then([](int a)
          {
              return a+1;
          });
      })
      .then([&](int a)
      {
          doneOrError(a == 2, );
      });
      pms.resolve(1);

  });
  asyncTest("return <resolved>.then() from then() from then()\n      should propagate (rb1209c)")
  {
      Promise<int> pms;
      pms.then([&](int a)
      {
          Promise<int> pms1(1);
          return pms1.then([&](int a)
          {
              Promise<int> pms2(1);
              return pms2
              .then([](int a)
              {
                  return a+1;
              });
          });
      })
      .then([&](int a)
      {
          doneOrError(a == 2, );
      });
      pms.resolve(1);
  });

  asyncTest("should propagate resolution from nested scope",
  {{"inner", "order", 1}, {"then-final", "order", 2}})
  {
      Promise<int> pms;
      pms.then([&](int a) mutable
      {
          Promise<int> pms2;
          loop.schedCall([pms2]() mutable { pms2.resolve(1234); }, 100);
          return pms2
          .then([&](int a)
          {
              Promise<int> pms3;
              loop.schedCall([pms3]() mutable { pms3.resolve(1234); }, 100);
              return pms3
              .then([&](int a)
              {
                  Promise<int> pms4;
                  loop.schedCall([pms4]() mutable { pms4.resolve(1234); }, 100);
                  return pms4
                  .then([&](int a)
                  {
                      Promise<int> pms5;
                      loop.schedCall([pms5]() mutable { pms5.resolve(1234); }, 100);
                      return pms5
                      .then([&](int a)
                      {
                          Promise<int> pms6;
                          loop.schedCall([pms6]() mutable { pms6.resolve(1234); }, 100);
                          return pms6
                          .then([&](int a)
                          {
                              test.done("inner");
                              return a+2;
                          });
                      });
                  });
              });
          });
      })
      .then([&](int a) mutable
      {
          doneOrError(a == 1236, "then-final");
      });
      pms.resolve(1);
  });
});
TestGroup("when() tests")
{
    asyncTest("when() with 2 already resolved promises")
    {
        Promise<int> pms1;
        pms1.resolve(1);
        Promise<void> pms2;
        pms2.resolve();
        when(pms1, pms2)
        .then([&]()
        {
            test.done();
        });
    });
    asyncTest("when() with 1 already-resolved and one async-resolved promise")
    {
        Promise<int> pms1;
        pms1.resolve(1);
        Promise<void> pms2;
        when(pms1, pms2)
        .then([&]()
        {
            test.done();
        });
        loop.schedCall([pms2]() mutable { pms2.resolve(); }, 100);
    });
    asyncTest("when() with 2 async-resolved promise")
    {
        Promise<int> pms1;
        Promise<void> pms2;
        when(pms1, pms2)
        .then([&]()
        {
            test.done();
        });
        loop.schedCall([pms1]() mutable { pms1.resolve(1); }, 100);
        loop.schedCall([pms2]() mutable { pms2.resolve(); }, 100);
    });
    asyncTest("when() with one already-failed and one async-resolved promise")
    {
        //when() should fail immediately without waiting for the unresolved promise
        Promise<void> pms1;
        pms1.reject("Test error message");
        Promise<int> pms2;
        when(pms1, pms2)
        .then([&]()
        {
            test.error("Rejected output promise must not call then()");
        })
        .fail([&](const Error& err)
        {
            doneOrError(err.msg() == "Test error message",);
        });
    });
    asyncTest("when() with one async-failed and one async-resolved promise",
    {{"first", "order", 1}, {"second", "order", 2}, {"output", "order", 3}})
    {
        Promise<void> pms1;
        Promise<int> pms2;
        when(pms1, pms2)
        .then([&]()
        {
            test.error("Rejected output promise must not call then()");
        })
        .fail([&](const Error& err)
        {
            doneOrError(err.msg() == "Test message", "output");
        });
        loop.schedCall([pms1, &test]() mutable { test.done("first"); pms1.resolve(); }, -100);
        loop.schedCall([pms2, &test]() mutable { test.done("second"); pms2.reject("Test message");}, -100);
    });

});
  return test::gNumFailed;
}
