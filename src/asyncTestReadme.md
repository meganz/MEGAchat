# Asynchronous C++11 unit testing environment

## Overview

This async unit testing environment differs from the traditional C++ unit testing frameworks in the fact that it
incorporates a message loop and instrumentation to register and track conditions that should occur
(called 'done-s' - after the name of the function that signals that the condition has occurred),
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
#include <promise.h>

//good to have the test framework header included last,
//to minimize the chance of macro conflicts
#include <asyncTest-framework.h>

INIT_TESTS();

int main()
{
    <global test initialization code (if any) goes here>
    testGroup("group one")
    {
        group.beforeEach = [&](test::Test& t){ printf("beforeEach\n"); };

        asyncTest("test one",
        {{"event 1", "order", 1}, {"event 2", "timeout", 4000, "order", 2}})
        {
            loop.jitterPct = 40; //set the default schedCall() delay fuziness. By default it's 50%
            schedCall([&]()
            {
                test.done("event 1");
                schedCall([&]()
                {
                    test.done("event 2");
                });
            });
         });
         asyncTest("test two", {"foo", {"bar", "timeout", 2000}})
         {
             promise::Promise<int> pms;
             pms.then([&](int a)
             {
                //if a is not 34, calls test.error() and throws a test::BailoutException. Otherwise, calls test.done("foo")
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
             check(a == 2); // if a is not 2, calls test.error() and throws test::BailoutException
         }).disable(); //disables the test
    });
    <global cleanup code (if any) goes here>
    return test::gNumFailed; //return the total error count to the calling process. Useful for automation
}
```

## Structure

Tests are grouped in 'test groups', each group having the option to define a function that can be executed before each of the
tests in the group.  
The framework does not take control over the main() function, so it is defined by the user. The user is free to insert any
initialization/cleanup code before/after each test group. For example, global initialization code can simply be put at the
beginning of the main() function, before the definition of the first test group. However, code inside a group is not run
in sequence with the tests - tests are executed after the body of the group completes execution.  

A test group is defined by the `testGroup(name) { <group body> });` macro, with the name of the test group as argument.
Mind the bracket and semicolon after the closing brace. The body of the test group is executed in sequence after
the last test of the previous group has finished execution (if there is one), and any user code before the `addGroup()` call.
The body has a local variable `group` defined, that references the current group object. That object has the
following facilities:  
 - `group.beforeEach = <void(test::Test&) function>`  
   If this property is set, then the specified function will be executed before each test, passing to it the `test` object
   that represents that test. For more info about the `test` object see the 'Local system variables' section.  
 - `group.afterEach = <void(test::Test&) function>`  
   If this property is set, then the specified function will be executed after each test, passing to it the `test` object
   representing that test. The function is guaranteed to be executed even if the test completed with error or exception.
   All exceptions that may occur in `afterEach` are caught and silently ignored.  

The group body can contain any code, but its purpose is to configure the test group and register tests in that group,
so normally it just contains group configuration code and a sequence of asyncTest() and syncTest() calls, which define
and add tests to the group, *but do not execute them yet*. The group configuration code usually does operations on the `group`
object. After the group body execution completes, all registered tests are executed in the sequence in which they were
registered. Finally, the main() function can return the total number of failed tests, communicating that info to the
calling process.  

## Test definitions

### Async tests

Async tests are defined and registered inside a group body by:
```
asyncTest (name [,<list of done-s>])
{
    <test body>
});
```
Mind the closing bracket and semicolon at the end.  
The name can be any string. The list of 'done' items is enclosed in braces, and each item description is in the form:  
```{tag [, optname1, val1 [, optname2, val2 ]]}```  
or, if no options are needed, it can be just a string for the tag, with no enclosing braces.

The `tag` is the unique identifier of the 'done' item, which is used (in the `test.done(tag)` call) to specify that
condition has occurred. What follows are optional configuration parameters for that item. They are specified as a string
option name followed by an integer value, then next option name, followed by an option value etc. Currenty there are
only two config parameters:  
 - 'timeout'  
   Specifies the time to wait for that condition (since the start of the test). If the condition does not occur
   within that period, the test fails with a message identifying the condition that timed out. If this option is not
   specified, a default timeout of 2000ms is used.  
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
Mind the closing bracket and semicolon at the end.  

### Disabling a test

Any synchronous or asynchronous test can be disabled by appending `.disable()` after the closing bracket of the test body
definition, see the example.  

### Test-specific cleanup ###

To specify a cleanup function, specific for a given test (vs `group.afterEach`), you can:
 - append `.cleanup = <void() function>` after the closing bracket of a `syncTest` or `asyncTest` definition:

```
asyncTest('foo', {"one", "two"})
{
 <test body>
})
.cleanup = [&]()
{
<cleanup code>
};
```
 - call `test.cleanup = <void() function>` inside the test body (see the documentation of the `test` object below). This way
   has the ability to capture variables local to the test body inside the cleanup function.  

The test cleanup function is guaranteed to be called no matter how the test completed (error, exception, etc).
This function is executed *before* the group's `afterEach` (if such is defined).

### Local system variables

A test body has two local variables defined:  
 - `loop` (Only async tests)  
   The event loop inside which the asynchronous test runs (instance of `test::EventLoop`).
   This object has the following methods:   
    * `loop.addDone({tag [,option1, val1 [, option2, val2]]})`  
       Dynamically adds a 'done' condition to the test. The timeout starts to run since the moment the `loop.addDone()`
       is called.  
    * `loop.done(tag)`  
       Signals that a 'done' condition has occurred. The tag identifies the condition that was specified.  
    * `loop.jitterPct`
       The default fuzziness percent of schedCall() delays. If not set, it is 50%. See below the description
       of `loop.schedCall()`
    * `loop.schedCall(func, delay [, jitterPct])`  
       Schedules a call to the specified function after the specified period (in milliseconds), with some random variance.
       If `delay` is negative, then the delay is relative to the time of the last such call (with negative delay). 
       This allows easy setup of function call sequences by specifying the delays between them instead of all delays relative
       to one single point in time. If `delay` is positive, it is relative to the current moment. `jitterPct` is the fuzziness
       of the actual delay as percent of the given value, i.e. the actual value randomly varies around `delay` with max
       deviation of `delay *(jitterPct/100)`.  
       If `jitterPct` is not specified, the loop's default (if no default set, then 50%) will be used.  
 - `test`  
    The object (instance of class `test::Test`) representing that test. This object has the following methods:  
    * `test.error(message)`  
      Records that an error has occurred, but does not actually abort the test. After that call, normally the test should be
      aborted by the user via an early return, or by throwing an exception. However, throwing an exception would cause the
      error report to state that an exception has occurred, which can be misleading because the exception is used only to
      bail out. For this purpose, you can use the `test::BailoutException` class, which will be recognized by the framework
      and not reported.  
    * `test.done(tag)` (Only async tests)  
      Same as `loop.done(tag)`
    * `test.cleanup = <void() function>`
    Registers a cleanup function that will be run after the body of the test is completed, even if an error/exception
    occurred.

## Convenience macros
There are a few convenience macros defined by the framework, and it's a good idea to include the public header of the
framework last to avoid potential conflict of these or any other macros from the framework with code in other headers.  

 - `check(cond)`  
    Similar to `assert()` - if the condition returns `false`, `test.error()` is called, after which
    `test::BailoutException` is thrown. The error message shows the condition that failed, and the source file and line.  
 - `doneOrError(cond, tag)` (Only in async tests)  
    Calls `check(cond)` and after that `test.done(tag)`. Therefore it can be
    used to resolve a 'done' condition, but only in case a condition is true, and signal error if the condition is false.

## Macros for debug, verbosity and defaults
There are several macros that enable additional output, and set defaults. To be used, they should be defined before the test
framework header is included:
- `TESTLOOP_LOG_DONES` - if defined, every resolved 'done' condition will be logged
- `TESTLOOP_DEBUG` - if defined, enables debug info output, related to the event loop
- `TESTLOOP_DEFAULT_DONE_TIMEOUT` -  Sets the default timeout (in milliseconds) of 'done' conditions. If not set, the
  default is 2000ms

