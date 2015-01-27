# C/C++ Code safety guideline #
This guide describes the code safety principles that were and *must* be followed in the Karere C++ code  
These principles are not about code readability, but to minimize bugs, memory leaks, crashes
and provide good self-diagnostics with minimum effort on the programmer's side. Following these principles should
greatly increase passive code safety, and actually make the code simpler and, as a side effect, more readable.

## Memory and resource management ##
* Get familiar with the C++ RAII concept, if you are not already. It's extensively used and referenced below.
* Do not use `operator delete` or any resource deallocation function directly, even in destructors. This is unless you
are writing a smart pointer or some other RAII class, or in a specific case where this is absolutely necessary. Using `operator
delete` or a resource deallocation function directly means that you are doing manual resource management, and this is
exactly what we want to avoid. Instead:
  - Use stack-allocated objects/structures wherever possible. This is also much more efficient.
  - Use smart, rather than raw pointers for dynamically-allocated objects/structures. This applies also to class members.
It is very easy to forget to NULL-initialize a raw pointer member in the constructor (if it's not allocated there),
or forget to delete it in the destructor. Especially if you add that member later.  
A smart pointer solves both problems - it auto-initializes to NULL, unless given an actual dynamic object,
and automatically frees the object on class deletion.
 * When dealing with a non-memory resource, such as some kind of handle (i.e. OS handle), use an universal 'smart handle'
RAII template class if possible, similar to a smart pointer, that is instructed how to delete this type of resource.
One such class, already used in the code, is `MyAutoHandle` in `src/AutoHandle.h`. The template takes 4 parameters - the handle type,
the signature of the 'free' function, pointer to the 'free' function itself, and an 'invalid value' - if the handle has that
value, it is not freed. Only if such an universal 'smart handle' class cannot be used conveniently, write your own for the specific case.
The fewer implementations of such classes are used, the less chance of a buggy such implementation.
 * Assume any line of code can throw an exception or do an early return, even if you are sure it doesn't at the moment.
Later someone may change the code to throw or add an early return. As an example, imagine you are allocating a resource near
the start of a function, and manually freeing it near the end. If something throws or does an early return in the middle of
that function, your resource freeing code will not get executed, and you will have a resource leak.
Using RAII for resource management, as described above, solves this problem.

## Error handling ##
To automate error handling, and make it non-intrusive, error reporting is done via exceptions, and not via return codes, wherever
possible. Unfortunately this is not possible/safe across a DLL border or when the exception would pass through OS callbacks or
stack frames of another library. In other words, exceptions must not propagate into the OS or a third-party library.

 * Use exceptions to signal errors wherever possible, but do not abuse them. Exceptions signal infrequent conditions that
are not to be handled by the normal code flow. In other words - exceptions signal 'exceptional' conditions, and not ones that
may be expected by the normal code flow. For example, a _universal_ string search function should not throw in case the searched
string was not found, because this, in the general case, can be expected. However, in a specific string search, where the subsequent
code relies on the fact that the substring _is_ found, an exception can be town to bail out. In that case, the code logic
doesn't need to care about 'what if' abnormal conditions.
 * If you can throw exceptions, but a function that you call returns error codes, such as a plain C API function,
and you leave handling that error to the caller, throw an error when it returns an error condtion. If there are multiple such
calls, implement the error check and throw in an inline static function or a macro, and wrap all calls in that function/macro.
Macros should be avoided, but in this case a macro has access to the line number where the actual call happened, and a
function doesn't. Also, a macro can log its parameters as strings (i.e. it can easily log an enum by its name).
See the macro `_curleopt` in `base/services_http.hpp` about an example.
 * When a function has to check for conditions before continuing with next steps, never use nested `if`-s (Microsoft-style),
unless necessary in that specific case. Instead, check for that condition, and if it's not met, do an early return
from that function.  
For example compare the following two code snippets:
```
    if (param != NULL) //function parameter validation
    {
        <do some stuff1>
        ....
        bool okToContinue1 = <some check expression>;
        if (okToContinue1)
        {
            <do some more stuff2>
            ....
            bool okToContinue2 = <some check expression>;
            if (okToContinue2)
            {
                 <do some more stuff3>
                 ....
                 bool okToContinue3 = <some check expression>;
                 if (okToContinue3)
                 {
                      <do some final stuff4>
                 }
            }
        }
   }

   <cleanup code> //needs additional state info to know if something failed and whether

   //what are we actually returning here? A complete or a half-baked result?
   //How do we signal what actually went wrong from this single place? would need additional state to do that.
   return xxx; 
```
This code looks even less readable in K&R indentation style. Very often code written like this fails silently without even giving
a clue that something went wrong, let alone signaling _exactly_ what went wrong. I leave it to the reader to imagine how easy
is to debug such code.  

Instead, do:
```
    if (!param) //function parameter validation
        <throw or return some error: informative about what exactly happened>
    <do some stuff1>
    bool okToContinue1 = <some check expression>;
    if (!okToContinue1)
        <throw or return some error: informative about what exactly happened>
    <do some stuff2>
    bool okToContinue2 = <some check expression>;
    if (!okToContinue2)
        <throw or return some error: informative about what exactly happened>
    <do some stuff3>
    bool okToContinue3 = <some check expression>;
    if (!okToContinue3)
        <throw or return some error: informative about what exactly happened>
    <do some final stuff4>
    <do cleanup>
    return xxx; //guaranteed to be the result of the complete operation
```  
  * If you need to do cleanup before _each_ of the returns (both the early returns or the final one), _and_ that cleanup
 cannot be done automatically with RAII, _and_ the cleanup code before all returns is mostly the same, _and_ is it not practical
to implement that cleanup code as a function(due to using a lot of local variables), _and_ it actually makes code shorter,
better structured and readable - _then and only then_, you can use `goto`! These are very rare cases in C++ (currently there is
no such use in the C++ Karere codebase), and not so rare in plain C (because it can't cleanup using RAII).
One thing you must be very careful about if using `goto` in this case: you should not have `goto` skip the initialization
of variables that you use after the `goto`. This should never happen if you always initialize variables at the place of
declaration, and the compiler should issue a warning about that, but still be very careful. A passive safety measure against
that is to have the variable scopes not larger than necessary. This is a common good coding practice and should be done anyway,
anywhere in the code. Example:  
```
    <allocate resource 1>
    .....
    <allocate resource n>

    string errMsg;
    if (!param) 
    {
        errMsg = "Function parameter is NULL";
        goto cleanup;
    }
    <do some stuff1>
    bool okToContinue1 = <some check expression>;
    if (!okToContinue1) 
    {
        errMsg = "Was not ok to continue to step 1";
        goto cleanup;
    }
    <do some stuff2>
    bool okToContinue2 = <some check expression>;
    if (!okToContinue2)
    {
        errMsg = "Was not ok to continue to step 2";
        goto cleanup;
    }
    <do some stuff3>
    bool okToContinue3 = <some check expression>;
    if (!okToContinue3)
    {
       errMsg = "Was not ok to continue to step 3";
       goto cleanup;
    }
    <do some final stuff4>
    :cleanup
        <free resource n>
        ....
        <free resource 1>
    if (!errMsg.empty())
        throw std::runtime_error("func failed: "+errMsg);
    return xxx; //guaranteed to be the result of the complete operation
}
```

## Variable declaration and initialization ##
* Use descriptive names, even for local variables with small scope. No matter the scope, people will read that code.
* Declare local variables right before the place where they are used, and *not* in the beginning of the function.
* Always initialize the variable at the place of declaration.
* Avoid double-initialization of local variables wherever possible:  
Incorrect:  
```
    int a = 10;
    if (someCondition)
    {
       a = 40;
    }
```

Correct:  
```
int a = someCondition ? 40 : 10; 
```

However if the `if (someCondition)` has to execute other code as well, it may not be as clean and practical to avoid double
initialization.

## Raw pointer handling ##
* When an object has to keep a reference to some other object and that reference never changes throughout the lifetime of the
object whose member it is, do not do it with a pointer, but with a reference. That is, the type of the member 
will be not `SomeObject*`, but `SomeObject&`.
The reference must be passed to the constructor and the member initialized in the ctor initialization list, otherwise the
code will not compile. In this way the reference is guaranteed to be non-NULL, cannot be changed during the lifetime of the
object, and is more convenient to dereference with `.` rather than `->`.
* Pass references rather than pointers as function parameters whenever NULL is not used. This guarantees that the parameter
is never NULL, and is easier to dereference with a `.` rather than `->`.

## Forward declarations ##
In a header, whenever you are using _only_ pointers or references to an externall class (i.e. not defined in that header),
but not accessing its members or doing any operations with it, do not include the header that defines that external class,
but rather do a forward declaration. At this point the compiler only needs to know that this is a class, and nothing more.
Including the header may make you life easier but will slow down compilation and doing it many times can _greatly_ slow down
compilation.

## Templates ##
Templates are both type-safe and very efficient because the compiler knows all types and code at compile times and can do a
lot of optimizations. The safety of templates can be very roughly be described with this: "If it compiles, it will most
probably just work correctly". One of the reasons for this is that the compiler knows the exact types of everything,
(rather than doing type conversions to match declared types), and has a more complete picture of what you are trying to do.
* You can get familiar with the CRTP (Curiously Recurreing Template Pattern) pattern and use it whenever you need polymorphism
and can do with static polymorphism.
