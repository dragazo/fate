# FATE
Have you ever looked at RAII code and wondered: "Can I somehow *use* **and** *not use* RAII simultaneously??"

Do you have a **deep-seated emotional response** to the words "unique" and "ptr"?

Does the concept of `std::vector` having the **audacity to manage itself** make your stomach churn?

It was **FATE** that you'd stumble onto this page.

**Introducing ... FATE!!!**

FATE *(function at the end)* is a generic RAII wrapper that calls a function-like object at the end of its lifetime.

All implementation is supplied by [`fate.h`](fate.h).

## fate

Template class `fate` is defined that takes the type of function-like object to bind.
Any function-like object that accepts zero arguments is a valid target for a `fate` object to bind to.

The template argument is the type to store internally, so binding a raw function e.g. `void foo();` would require `fate<void(*)()>`.

A `fate` object at all times either contains a valid function-like object or is "empty".
An empty `fate` object does not contain a function-like object, but you are guaranteed there will be no dynamic allocations.

The following concern `fate`'s C++ intrinsics:
* `fate` is default-constructible to empty, and is guaranteed to be noexcept.
* `fate` is explicitly-constructible from an argument that can be forwarded to the `T` constructor. If this fails, the fate object is empty and the exception is rethrown.
* The `fate` destructor invokes the stored function-like object (if any). If an exception is throw, it is caught and ignored. The `fate` instance is left in an undefined state.
* Like all unique contract RAII types, `fate` is not copy constructible or copy assignable.
* However, due to the oddity of `fate`'s contract, it is also not move assignable.
* `fate` is move constructible. On success, the destination has the source's function-like object (if any) and the source is empty. The `noexcept` specifier will be deduced based on the transfer method selected. The following rules apply to this action:
  * If T is nothrow move constructible:
    * The function-like object is transfered via move constructor.
    * This is a nothrow operation with the nofail guarantee.
  * Otherwise, if T has a copy constructor:
    * The function-like object is transfered via copy constructor.
    * This is a potentially-throwing operation with at least the strong guarantee if T has at least the strong guarantee.
  * Otherwise:
    * The function-like object is transfered via move constructor.
    * This is a potentially-throwing operation with at least the basic guarantee if T has at least the basic guarantee.
    * This is the only way a `fate` contract can be violated.

A `fate` object is explicitly convertible to `bool`, which represents the fate instance currently containing a function-like object.
The `empty()` method returns the opposite.

The `release()` method can be used to ~destroy destiny~ cause the fate object to enter the empty state without executing its stored function (if any).

Nearly all functions are constant expressions.
Functions that can deduce their noexcept specifiers do so.

## make_fate

Because having to specify the template argument is nasty, I recommend you use `auto` type deduction and the `make_fate()` helper function to create fate objects, especially if you want to bind lambdas.

Not using `make_fate()`:
```c++
fate<void(*)()> fate1 = foo;

auto lambda = []{ /* do stuff */ };
fate<decltype(lambda)> fate2 = std::move(lambda);
```

Using `make_fate()`:
```c++
auto fate1 = make_fate(foo);
auto fate2 = make_fate([]{ /* do stuff */ });
```

## Examples

Let's start out with a contrived example: what if we had a grudge against C++11
```c++
{
    // create a widget object in dynamic memory.
    Widget *widget = new Widget;
    // create a fate object that will delete it for us when fate leaves scope.
    auto widget_deleter = make_fate([=]{ delete widget; });

    /* stuff happens, maybe some exceptions, a lot of branching, who knows... */
    /* the return tree is all over the place */

} // BOOM - widget_deleter goes out of scope and the widget is deleted
```

Now something a bit more practical: Let's say there's some synchronized resource that requires a mutex be locked before use and unlocked after use. Now let's say we have functions to lock/unlock, but no access to the actual mutex. Well our usual go-tos `std::unique_lock` and `std::lock_guard` are out of the picture since we can't touch the actual mutex. This is easily solved by FATE:
```c++
// defined in some library you're using
void lock();
void unlock();

{
    // lock so we can use the resource
    lock();
    // create a fate object that will unlock it when we leave scope
    auto unlocker = make_fate(unlock);
    
    /* stuff happens, maybe some exceptions, a lot of branching, who knows... */
    /* the return tree is all over the place */
    /* boy, i sure hope we remember to unlock the mutex at all these potential fail/return points */
    /* it'd be a shame if we deadlocked ourselves on the next invocation of this function */
    
} // BOOM - unlocker goes out of scope and the mutex is unlocked
```

## Afterword

If you couldn't tell, at least 90% of the reason I made this was because I liked the name.
