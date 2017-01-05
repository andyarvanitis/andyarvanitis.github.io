---
layout: post
title: "An FFI example for PureScript C++: PCRE"
date: 2017-01-03 15:10
comments: true
categories: 
---

In case you haven’t read it, for a brief overview of the PureScript C++ FFI please see my guest post in “24 Days of PureScript 2016”, [Day 19 – Intro to the C++ FFI](https://github.com/paf31/24-days-of-purescript-2016/blob/master/19.markdown).

In this post, I'll demonstrate one way to tackle interop with a not-too-trivial C library, namely “Perl Compatible Regular Expressions”, a.k.a. [PCRE](http://www.pcre.org/). I don't actually have much experience with this library, but I was inspired to try creating bindings for it after recently re-reading Chapter 17: “Interfacing with C: The FFI” from [*Real World Haskell*](http://book.realworldhaskell.org/) by Bryan O'Sullivan, Don Stewart, and John Goerzen.

As this post borrows both the motivation and specific example from the book, please keep the following in mind:

1. This is **not** a “PureScript is better at this than Haskell” post
2. This is not necessarily the best way to use the PCRE library or create PureScript bindings for it
3. The referenced Haskell example from the book is not necessarily the only way to do things in Haskell (see item 1)

With that out of the way, the other thing I'd like to point out is that I will use the same C API of the PCRE library that was used in the book, even though I'm aware a newer version exists (pcre2), as does a C++ version. But again, please see items 1-3 above!

### The Goal

Create the minimal bindings needed to use functions `pcre_compile` and `pcre_exec` in a simple example.

### Declare the foreign types and functions on the PureScript side

This signature of C function `pcre_compile` looks like this: 

{% codeblock lang:c %}
pcre *pcre_compile(const char *pattern,
                   int options,
                   const char **errptr,
                   int *erroffset,
                   const unsigned char *tableptr); 
{% endcodeblock %}

The approach I've chosen to take is to create a simplified and pure<a href="#footnote"><sup>1</sup></a> PureScript C++ wrapper function for `pcre_compile` which simply takes a string and options, and returns either an error string or a valid (foreign, opaque) `pcre` object. PureScript strings are native strings, so that means the only foreign types we need to declare are for the options and the `pcre` compiled code object:

{% codeblock lang:haskell %}
foreign import data PCREOption :: *
foreign import data PCRECode :: *
{% endcodeblock %}

Note that `PCREOption` happens to be a C `int`, so we could have treated it that way on the PureScript side too and simply used `Int`, but I chose to abstract away that particular detail.

The `PCREOption` type can have a number of (constant) values, so let's declare a couple of them for this exercise:

{% codeblock lang:haskell %}
foreign import caseless :: PCREOption
foreign import dotall   :: PCREOption
{% endcodeblock %}

We can now declare our wrapper function in terms of these imported foreign types:

{% codeblock lang:haskell %}
foreign import compile :: String -> Array PCREOption -> Either String PCRECode
{% endcodeblock %}

We'll use a similar approach for the wrapper for `pcre_exec` and another function, `capturedCount`, but please see the full source later in the post for those details.

### Implement the foreign wrapper functions on the C++ side

Let's jump right to the code:

{% codeblock lang:c++ %}
enum {
  caseless = PCRE_CASELESS,
  dotall   = PCRE_DOTALL
};
{% endcodeblock %}

The foreign options we declared earlier are defined here as `enum`s (`int`s) taking on `#define` constant values from the PCRE library. If they weren't capitalized, we wouldn't have needed them at all; the PureScript imports alone would have been sufficient.

{% codeblock lang:c++ %}
auto compile( const char * pattern, const any::array& options ) -> any {
  const char * err = nullptr;
  int erroffset = 0;
  auto ptr = pcre_compile(pattern,
                          bitor_all(options),
                          &err,
                          &erroffset,
                          nullptr);
  return ptr ? Right(managed<pcre>(ptr, pcre_free)) : Left(err);
}
{% endcodeblock %}

The idea here is to have a straightforward-as-possible wrapper function that calls `pcre_compile`. We want to minimize any logic here, keeping any sophisticated decision-making on the PureScript side, where things are much more type safe. As mentioned earlier, I've also chosen to implement this wrapper as a pure function which does not mutate any non-local data.

The call to `pcre_compile` is pretty simple, but the last line has a couple of interesting things going on. The final return value depends on whether the pointer to the `pcre` object returned from `pcre_compile` is `NULL` or not:

* If it is `NULL`, the `err` (error) C-style string is implicitly copied and put into a PureScript `Left` data value from the familiar `Either` module. Both the new string and the `Left`'s memory are automatically managed by PureScript.
 
* If it isn't `NULL`, the `pcre` object is stored as a managed pointer, meaning that the PureScript runtime will take ownership of its lifetime. When it is time to release/destroy it, I've specified here that it will call `pcre_free` (from the PCRE library) to do the actual freeing. In turn, a `Right` is constructed with this value.

<div style="position: relative; top: -2em;">
Note that one of the nice things about using the <strong>Either</strong> type like this is that the Either PureScript module didn't explicitly expose these constructors via FFI functions – it's just a benefit of the compiler targeting/generating C++ code.
</div>

### Full source code and output

The rest of the implementation doesn't advance beyond these concepts, so I'll simply provide it without further comment. I hope this post highlighted some of the possibilities of PureScript's very flexible FFI in general, and specifically the C++ backend's flavor of it. 

{% include_code lang:haskell Main.purs %}

{% include_code lang:c++ Main.hh %}

{% include_code lang:c++ Main.cc %}

**Build and run**

{% codeblock lang:text %}
$ make debug -j4 LDFLAGS=-lpcre
...
$ make run
(Right [(Tuple 0 28),(Tuple 0 3),(Tuple 4 7)])
{% endcodeblock %}

<a name="footnote"></a></br>
<ol style="font-size:80%">
<li>It's also possible to go with “effectful” functions, and handle allocation and mutation in ways similar to what is done in the book, or with something like PureScript's <code>STRef</code>, or something else altogether.</li>
</ol>
