# Asynchronous C++11 unit testing environment

## Overview

The async unit testing environment differs from the traditional C++ unit testing frameworks in the fact that it
incorporates a message loop and instrumentation to register and track conditions that should occur
(called 'done()-s' - after the name of the function that signals that the condition has occurred),
within a specified timeout, and in a given order, if necessary. It also provides a means to schedule a function call after
a speficied time relative to the moment of scheduling, or, in 'ordered mode' - relative to the last such 'ordered' call.
The async unit test is considered complete once all expected events have occurred or timed out, and all scheduled functions
have been called, or an error occurs, by either explicitly signalling it in the test code, when an event with specified
order occurs in unexpected order, or an unhandled exception occurs.  

In addition to asynchnorous tests, the environment supports ordinary synchronous tests, where the test is considered complete
once the code of the test returns, an error is explicitly signalled by the code, or an unhandled exception occurs.  

The framework is a header-only library. To use it, it is needed to only include the public header "asyncTest-framework.h",
and to insert the TESTS_INIT() macro in the global scope, before the main() function.  
 
## Simple example
```
#include <asyncTest-framework.h>

INIT_TESTS();
int main()
{

<global test initialization code goes here>
testGroup("group one")
{
    asyncTest("test one",
    {{"event 1", "order", 1}, {"event 2", "timeout", 4000, "order", 2}})
    {
        schedCall([&]()
        {
            test.done("event 1");
            schedCall([&]()
            {
                test.done("event 2");
            });
        });
     });
     asyncTest("test two", {"resolved", "bar"}
     {
        Promise<int> pms;
        pms.then([&](int a)
        {
            doneOrError(a == 34, "foo");
        })
        .then([&]()
        {
            test.done("bar");
        })
        .fail([&](promise::Error& err)
        {
            test.error("promise should not fail");
        });
        loop.schedCall([&]()
        {
            pms.resolve(34);
        }, 100);
     });
     syncTest("test three")
     {
         int a = 2;
         if(a != 2)
             test.error("a must be 2);
     }).disable(); //disabled the test

     <global cleanup code goes here>
     return test::gNumErrors;
} 
```

## Structure

Tests are grouped in 'test groups', each group having the option to define a function that can be executed before each of the
tests in the group.  
The framework does not take control over the main() function, so it is defined by the user. The user is free to insert any
initialization/cleanup code before/after each test group. For example, global initialization code can simply be put at the
beginning of the main() function, before the definition of the first test group. However, code before/after single tests
inside a group is not run in sequence with the tests.  

A test group is defined by the `testGroup()` macro, with the name of the test group as argument.
Mind the bracket and semicolon after the closing brace. The body of the test group is executed in sequence after
the last test of the previous group has finished execution (if there is one), and any user code before the 'addGroup()' call.
The body has a local variable `group` defined, that references the current group object. That object has the
following facilities:  
 - `group.beforeEach = <void(test::Test&) function>` If this property is set, then the specified function will be executed
before each test, passing to it the `test` object that represents that test. For more info about the `test` object see
the 'Local system variables' section.  

The group body can contain any code, but its purpose is to configure the test group and register tests in that group,
so normally it just contains group configuration code and a sequence of asyncTest() and syncTest() calls, which define
and add tests to the group, *but do not execute them yet*. The group conficuration code usually does operations on the `group`
object. After the group body execution completes, all registered tests are executed in the sequence in which they were
registered. Finally, the main() function can return the total number of failed tests, communicating that info to the
calling process.  

## Test definitions

### Async tests

Async tests are defined and registered inside a group body by  
```
asyncTest (name [,<list of done()-s>])
{
    <test body>
});
```
The name can be any string. The list of done-s is enclosed in braces, and each done() description is in the form:  
`{'tag' [, 'option1', val1 [, 'option2', val2 ]]}`  
The `tag` is the unique identifier of the done() item, which is used (in the `test.done(tag)` call) to specify that
condition has occurred. What follows are optional configuration parameters for that done(). They are specified as a string
option name followed by an integer value, then next option name, followed by an option value etc. Currenty there are
only two config parameters:  
 - 'timeout'  
 Specifies the time to wait for that condition (since the start of the test). If the condition does not occur
within that period, the test fails with a message identifying the condition that timed out. If this option is not specified,
 a default timeout of 2000ms is used.  
 - 'order'  
 The condition should occur in the specified order, relative to other such 'ordered' conditions (i.e. ones that
have the 'order' parameter). In other words, all conditions with that config option specified must occur in the specified
order relative to each other. If this option is not specified, then no order checking is done on that condition.  

### Synchronous tests

Synchronous tests are added by:
```
syncTest(name)
{
  <test body>
});
```  

### Disabling a test

Any syncronous or asynchronous test can be disabled by appending `.disable()` after the closing bracket of the test body
definition, see the example.  

### Local system variables

A test body has two local variables defined:  
 - `loop` (Only async tests).  
 The event loop inside which the asynchronous test runs (instance of `test::EventLoop`).
This object has the following methods:   
    * `loop.addDone({tag [,option1, val1 [, option2, val2]]})`  
       Dynamically adds a done() condition to the test. The timeout starts to run since the moment the `loop.addDone()`
       is called.  
    * `loop.done(tag)`  
       Signals that a `done()` condition has occurred. The tag identifies the condition that was specified  
    * `loop.schedCall(func, delay)`  
       Schedules a call to the specified function after the specified period (in milliseconds). If `delay` is negative,
       then the delay is relative to the time of the last such call with negative delay. Thus, sequences of
       function calls with specific delays between them can be scheduled. If `delay` is positive, it is relative to
       the current moment.
 - `test`  
    The test object (instance of class `test::Test`) of that test. This object has the following methods:  
    * `test.error(message)`  
    Records that an error has occurred, but does not actually abort the test. After that call, normally the test should be
    aborted by the user via an early return, or by throwing an exception. However, throwing an exception would cause the
    error report to state that an exception has occurred, which can be misleading because the exception is used only to
    bail out. For this purpose, you can use the `test::BailoutException` class, which will be recognized by the framework
    and not reported.  
    * `test.done(tag)` (Only async tests)  
    Same as `loop.done(tag)`

## Convenience macros
There are a few convenience macros defined by the framework, and it's a good idea to include the public header of the framework
last to avoid potential conflict of these or any other macros from the framework with code in other headers.  

 - `check(cond)`  
    Similar to `assert()` - if the condition returns `false`, test.error() is called, after which
`test::BailoutException` is thrown. The error message shows the condition that failed, and the source file and line.  
 - `doneOrError(cond, tag)` (Only in async tests)  
    Calls `check(cond)` and after that `test.done(tag)`. Therefore it can be
    used to resolve a `done()` condition, but only in case a condition is true, and signal error if the condition is false.

 

