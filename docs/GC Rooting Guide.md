## Introduction ##

This guide explains the basics of interacting with SpiderMonkey's
GC as a SpiderMonkey API user.
Since SpiderMonkey has a moving GC, it is very important that it knows
about each and every pointer to a GC thing in the system.
SpiderMonkey's rooting API tries to make this task as simple as
possible.

## What is a GC thing pointer? ##

"GC thing" is the term used to refer to memory allocated and managed by
the SpiderMonkey garbage collector.
The main types of GC thing pointer are:

- `JS::Value`
- `JSObject*`
- `JSString*`
- `JSScript*`
- `jsid`
- `JSFunction*`
- `JS::Symbol*`

Note that `JS::Value` and `jsid` can contain pointers internally even
though they are not a normal pointer type, hence their inclusion in this
list.

If you use these types directly, or create classes, structs or arrays
that contain them, you must follow the rules set out in this guide.
If you do not your program will not work correctly — if it works at all.

## GC thing pointers on the stack ##

### `JS::Rooted<T>` ###

All GC thing pointers stored **on the stack** (i.e., local variables and
parameters to functions) must use the `JS::Rooted<T>` class.
This is a template class where the template parameter is the type of the
GC thing it contains.
From the user perspective, a `JS::Rooted<T>` instance behaves exactly as
if it were the underlying pointer.

`JS::Rooted` must be constructed with a `JSContext*`, and optionally an initial value.

There are typedefs available for the main types.
Within SpiderMonkey, it is suggested that these are used in preference
to the template class (Gecko uses the template versions):

| Template class            | Typedef              |
| ------------------------- | -------------------- |
| `JS::Rooted<JS::Value>`   | `JS::RootedValue`    |
| `JS::Rooted<JSObject*>`   | `JS::RootedObject`   |
| `JS::Rooted<JSString*>`   | `JS::RootedString`   |
| `JS::Rooted<JSScript*>`   | `JS::RootedScript`   |
| `JS::Rooted<jsid>`        | `JS::RootedId`       |
| `JS::Rooted<JSFunction*>` | `JS::RootedFunction` |
| `JS::Rooted<JS::Symbol*>` | `JS::RootedSymbol`   |

For example, instead of this:

```c++
JSObject* localObj = JS_GetObjectOfSomeSort(cx);
```

You would write this:

```c++
JS::RootedObject localObj(cx, JS_GetObjectOfSomeSort(cx));
```

SpiderMonkey makes it easy to remember to use `JS::Rooted<T>` types
instead of a raw pointer because all of the API methods that may GC take
a `JS::Handle<T>`, as described below, and `JS::Rooted<T>` autoconverts
to `JS::Handle<T>` but a bare pointer does not.

### `JS::Handle<T>` ###

All GC thing pointers that are parameters to a function must be wrapped
in `JS::Handle<T>`. A `JS::Handle<T>` is a reference to a
`JS::Rooted<T>`, and is created implicitly by referencing a
`JS::Rooted<T>`: It is not valid to create a `JS::Handle<T>` manually
(the whole point of a Handle is that it only reference pointers that the
GC knows about so it can update them when they move).
Like `JS::Rooted<T>`, a `JS::Handle<T>` can be used as if it were the
underlying pointer.

Since only a `JS::Rooted<T>` will cast to a `JS::Handle<T>`, the
compiler will enforce correct rooting of any parameters passed to a
function that may trigger GC.
`JS::Handle<T>` exists because creating and destroying a `JS::Rooted<T>`
is not free (though it only costs a few cycles).
Thus, it makes more sense to only root the GC thing once and reuse it
through an indirect reference.
Like a reference, a `JS::Handle` is immutable: it can only ever refer to
the `JS::Rooted<T>` that it was created for.

Similarly to `JS::Rooted<T>`, there are typedefs available for the main
types:

| Template class            | Typedef              |
| ------------------------- | -------------------- |
| `JS::Handle<JS::Value>`   | `JS::HandleValue`    |
| `JS::Handle<JSObject*>`   | `JS::HandleObject`   |
| `JS::Handle<JSString*>`   | `JS::HandleString`   |
| `JS::Handle<JSScript*>`   | `JS::HandleScript`   |
| `JS::Handle<jsid>`        | `JS::HandleId`       |
| `JS::Handle<JSFunction*>` | `JS::HandleFunction` |
| `JS::Handle<JS::Symbol*>` | `JS::HandleSymbol`   |

You should use `JS::Handle<T>` for all function parameters taking GC
thing pointers (except out-parameters, which are described below).
For example, instead of:

```c++
JSObject*
someFunction(JSContext* cx, JSObject* obj) {
    // ...
}
```

You should write:

```c++
JSObject*
someFunction(JSContext* cx, JS::HandleObject obj) {
    // ...
}
```

There is also a static constructor method `Handle::fromMarkedLocation()`
that creates a `JS::Handle<T>` from an arbitrary location.
This is used to make `JS::Handle`s for things that aren't explicitly
rooted themselves, but are always reachable from the stack roots.
Every use of these should be commented to explain why they are
guaranteed to be rooted.

### JS::MutableHandle<T> ###

All GC thing pointers that are used as out-parameters must be wrapped in
a `JS::MutableHandle<T>`.
A `JS::MutableHandle<T>` is a reference to a `JS::Rooted<T>` that,
unlike a normal handle, may modify the underlying `JS::Rooted<T>`.
All `JS::MutableHandle<T>`s are created through an explicit `&` —
address-of operator — on a `JS::Rooted<T>` instance.
`JS::MutableHandle<T>` is exactly like a `JS::Handle<T>` except that it
adds a `.set(T &t)` method and must be created from a `JS::Rooted<T>`
explicitly.

There are typedefs for `JS::MutableHandle<T>`, the same as for the other
templates:

| Template class                   | Typedef                     |
| -------------------------------- | --------------------------- |
| `JS::MutableHandle<JS::Value>`   | `JS::MutableHandleValue`    |
| `JS::MutableHandle<JSObject*>`   | `JS::MutableHandleObject`   |
| `JS::MutableHandle<JSString*>`   | `JS::MutableHandleString`   |
| `JS::MutableHandle<JSScript*>`   | `JS::MutableHandleScript`   |
| `JS::MutableHandle<jsid>`        | `JS::MutableHandleId`       |
| `JS::MutableHandle<JSFunction*>` | `JS::MutableHandleFunction` |
| `JS::MutableHandle<JS::Symbol*>` | `JS::MutableHandleSymbol`   |

`JS::MutableHandle<T>` should be used for all out-parameters, for
example instead of:

```c++
bool
maybeGetValue(JSContext* cx, JS::Value* valueOut) {
    // ...
    if (!wasError)
        *valueOut = resultValue;
    return wasError;
}

void
otherFunction(JSContext* cx) {
    JS::Value value;
    bool success = maybeGetValue(cx, &value);
    // ...
}
```

You should write:

```c++
bool
maybeGetValue(JSContext* cx, JS::MutableHandleValue valueOut) {
    // ...
    if (!wasError)
        valueOut.set(resultValue);
    return wasError;
}

void
otherFunction(JSContext* cx) {
    JS::RootedValue value(cx);
    bool success = maybeGetValue(cx, &value);
    // ...
}
```

### Return values ###

It's ok to return raw pointers!
These do not need to be wrapped in any of rooting classes, but they
should be immediately used to initialize a `JS::Rooted<T>` if there is
any code that could GC before the end of the containing function; a raw
pointer must never be stored on the stack during a GC.

### AutoRooters ###

GC thing pointers that appear as part of a stack-allocated aggregates
(array, structure, class, union) should use `JS::Rooted<T>` when
possible.

There are some situations when using `JS::Rooted<T>` is not possible, or
is undesirable for performance reasons.
To cover these cases, there are various `AutoRooter` classes that can be
used.

Here are the main AutoRooters defined:

| Type                        | AutoRooter class       |
| --------------------------- | ---------------------- |
| `JS::AutoVector<JS::Value>` | `JS::AutoValueVector`  |
| `JS::AutoVector<jsid>`      | `JS::AutoIdVector`     |
| `JS::AutoVector<JSObject*>` | `JS::AutoObjectVector` |

If your case is not covered by one of these, it is possible to write
your own by deriving from `JS::CustomAutoRooter` and overriding the
virtual `trace()` method.
The implementation should trace all the GC things contained in the
object by calling `JS::TraceEdge`.

### Common Pitfalls ###

The C++ type system allows us to eliminate the possibility of most
common errors; however, there are still a few things that you can get
wrong that the compiler cannot help you with.
There is basically never a good reason to do any of these.
If you think you do need to do one of these, ask on one of
SpiderMonkey's support forums: maybe we've already solved your problem
using a different mechanism.

- Storing a `JS::Rooted<T>` on the heap.
  It would be very easy to violate the LIFO constraint if you did this.
  Use `JS::Heap<T>` (see below) if you store a GC thing pointer on the
  heap.
- Storing a `JS::Handle<T>` on the heap.
  It is very easy for the handle to outlive its root if you do this.
- Returning a `JS::Handle<T>` from a function.
  If you do this, a handle may outlive its root.

### Performance Tweaking ###

If the extra overhead of exact rooting does end up adding an
unacceptable cost to a specific code path, there are some tricks you can
use to get better performance at the cost of more complex code.

- Move `JS::Rooted<T>` declarations above loops.
  Modern C++ compilers are not smart enough to do LICM on
  `JS::Rooted<T>`, so forward declaring a single `JS::Rooted<T>` above
  the loop and re-using it on every iteration can save some cycles.
- Raw pointers.
  If you are 100% sure that there is no way for SpiderMonkey to GC while
  the pointer is on the stack, this is an option. Note: SpiderMonkey can
  GC because of any error, GC because of timers, GC because we are low
  on memory, GC because of environment variables, GC because of cosmic
  rays, etc.
  This is not a terribly safe option for embedder code, so only consider
  this as a very last resort.

## GC thing pointers on the heap ##

### `JS::Heap<T>` ###

GC thing pointers on the heap must be wrapped in a `JS::Heap<T>`.
The only exception to this is if they are added as roots with the
`JS::PersistentRooted` class, but don't do this unless it's really
necessary.
**`JS::Heap<T>` pointers must also continue to be traced in the normal
way**, and how to do that is explained below.
That is, wrapping the value in `JS::Heap<T>` only protects the pointer
from becoming invalid when the GC thing it points to gets moved.
It does not protect the GC thing from being collected by the GC!

`JS::Heap<T>` doesn't require a `JSContext*`, and can be constructed
with or without an initial value parameter.
Like the other template classes, it functions as if it were the GC thing
pointer itself.

One consequence of having different rooting requirements for heap and
stack data is that a single structure containing GC thing pointers
cannot be used on both the stack and the heap.
In this case, separate structures must be created for the stack and the
heap.

There are currently no convenience typedefs for `JS::Heap<T>`.

For example, instead of this:

```c++
struct HeapStruct
{
    JSObject*  mSomeObject;
    JS::Value  mSomeValue;
};
```

You should write:

```c++
struct HeapStruct
{
    JS::Heap<JSObject*>  mSomeObject;
    JS::Heap<JS::Value>  mSomeValue;
};
```

### Tracing ###

#### Simple JSClasses and Proxies ####

All GC pointers stored on the heap must be traced.
For simple JSClasses without a private struct, or subclasses of
`js::BaseProxyHandler`, this is normally done by storing them in
reserved slots, which are automatically traced by the GC.

```c++
JSClass FooClass = {
    "FooPrototype",
    --JSCLASS_HAS_PRIVATE |-- JSCLASS_HAS_RESERVED_SLOTS(1),
    &FooClassOps
};
JS::RootedObject obj(cx, JS_NewObject(cx, &FooClass));
JS::RootedValue v(cx, JS::ObjectValue(*otherGCThing));
js::SetReservedSlot(obj, 0, v);
```

Do not use `JS_SetPrivate()` to store a pointer to a GC thing.
The private field is frequently used to store pointers that are not
GC things, so the GC cannot automatically handle this slot.
This means it must be manually traced by the object's owner: this is
both fragile and more expensive than using an extra reserved slot, or
even just putting a new property on the object.

#### JSClass ####

When defining a JSClass with a private struct that contains `Heap<T>`
fields, [set the trace
hook](https://searchfox.org/mozilla-central/search?q=JSTraceOp)
to invoke a function that traces those fields.

#### General Classes/Structures ####

For a regular `struct` or `class`, tracing must be triggered manually.
The usual way is to define a
`void trace(JSTracer* trc, const char* name)` method on the class —
which is already enough be able to create a `JS::Rooted<YourStruct>` on
the stack — and then arrange for it to be called during tracing.
If a pointer to your structure is stored in the private field of a
JSObject, the usual way would be to define a trace hook on the JSObject
(see above) that casts the private pointer to your structure and invokes
`trace()` on it:

```c++
class MyClass {
    Heap<JSString*> str;

  public:
    static void trace(JSTracer* trc, JSObject* obj) {
        auto* mine = static_cast<MyClass*>(JS_GetPrivate(obj));
        mine->trace(trc, "MyClass private field");
    }

    void trace(JSTracer* trc, const char* name) {
        JS::TraceEdge(trc, &str, "my string");
    }
};
```

If a pointer to your structure is stored in some other structure, then
its `trace()` method should invoke yours:

```c++
struct MyOwningStruct {
    MyClass* mything;
    void trace(JSTracer* trc, const char* name) {
        if (mything)
            mything->trace(trc, "my thing");
    }
};
```

If the toplevel structure is not stored in a `JSObject`, then how it
gets traced depends on why it should be alive.
The simplest approach is to use `JS::PersistentRooted` (usable on
anything with a trace method with the appropriate signature):

```c++
JS::PersistentRooted<MyOwningStruct> immortalStruct;
```

But note that `JS::PersistentRooted` in a struct or class is a rather
dangerous thing to use — it will keep a GC thing alive, and most GC
things end up keeping their global alive, so if your class/struct is
reachable in any way from that global, then nothing will ever be cleaned
up by the GC.

It's also possible to add a custom tracer using
`JS_AddExtraGCRootsTracer()`.
Each tracer that gets added needs to be removed again later with
`JS_RemoveExtraGCRootsTracer()`.
Obviously it's faster to add (and later remove) one function that gets called during GC and loops over many objects than adding (and removing) many objects to the GC root set.

## Testing Rooting ##

### JS_GC_ZEAL (increased GC frequency) ###

This is a debugging feature to increase the frequency of garbage
collections.
It should reveal issues that would only show up in rare cases under
normal circumstances.
If the feature is enabled in the SpiderMonkey build (`--enable-gczeal`),
you can set the environment variable `JS_GC_ZEAL` to configure
debugging.
Set it to -1 to print a table of possible settings (or look up that
table in GCEnum.h).

The most useful settings probably are:

- `Alloc`: Collect every N allocations (default: 100)
- `VerifierPre`: Verify pre write barriers between instructions
- `GenerationalGC`: Collect the nursery every N nursery allocations
- `IncrementalMarkingValidator`: Verify incremental marking

You can append a number separated by a comma to specify N (like
`Alloc,1` to GC after every allocation or `GenerationalGC,10` to do a
minor GC every 10 nursery allocations).
With some settings the program gets extremely slow.

### Static rooting analysis ###

The static rooting analysis uses a [GCC
plugin](https://hg.mozilla.org/users/sfink_mozilla.com/sixgill) to dump
possible callstacks that can cause GC and statically (at compile time)
analyse this data for rooting hazards.

The main differences to dynamic rooting analysis are:

- Covers all compiled code at once during compile time. There's no need to actually execute these codepaths in the game.
- Setup is more complicated
- Only covers stack based rooting
- There can be false positives

More information and instructions (possibly outdated) on [this wiki
page](https://trac.wildfiregames.com/wiki/StaticRootingAnalysis).

## Summary ##

- Use `JS::Rooted<T>` typedefs for local variables on the stack.
- Use `JS::Handle<T>` typedefs for function parameters.
- Use `JS::MutableHandle<T>` typedefs for function out-parameters.
- Use an implicit cast from `JS::Rooted<T>` to get a `JS::Handle<T>`.
- Use an explicit address-of-operator on `JS::Rooted<T>` to get a
  `JS::MutableHandle<T>`.
- Return raw pointers from functions.
- Use `JS::Rooted<T>` fields when possible for aggregates, otherwise use
  an AutoRooter.
- Use `JS::Heap<T>` members for heap data. **Note: `Heap<T>` are not
  "rooted": they must be traced!**
- Do not use `JS::Rooted<T>`, `JS::Handle<T>` or `JS::MutableHandle<T>`
  on the heap.
- Do not use `JS::Rooted<T>` for function parameters.
- Use `JS::PersistentRooted<T>` for things that are alive until the
  process exits (or until you manually delete the PersistentRooted for a
  reason not based on GC finalization.)
