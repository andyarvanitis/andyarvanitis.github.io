---
layout: post
title: "Using Idris with Objective-C"
date: 2014-02-23 15:00
comments: true
categories: 
---

I've recently been playing around with [idris](http://www.idris-lang.org/), a functional language in
the [Haskell](http://www.haskell.org/haskellwiki/Haskell) family. I won't get into its design or features
here, but I'll simply state that, unlike Haskell, it has dependent types and is evaluated eagerly
(not lazily) by default. It offers a simple FFI to C, so I was curious what integration with Objective-C 
would look like. I haven't found any examples of that, so I thought I'd give it a try and share
my results.

One note: although Idris offers an LLVM backend, I only used the default C backend for this exercise.

My general strategy was to use the Objective-C runtime functions via the FFI, and to define Idris 
types to keep the operations as type-safe as possible. 

Basics
------

I started with just the basics -- getting access to Objective-C classes and selectors:

<!--?prettify lang=hs?-->

    module objective_c

    %include C "objc/runtime.h"
    %include C "objc_stubs.h"

    -------------------------------------------------------------------------------
    class Object a where
       getPtr : a -> IO Ptr

    -------------------------------------------------------------------------------
    getClass : String -> IO Ptr
    -------------------------------------------------------------------------------
    getClass name = mkForeign (FFun "objc_getClass" [FString] FPtr) name

    -------------------------------------------------------------------------------
    getSelector : String -> IO Ptr
    -------------------------------------------------------------------------------
    getSelector selname = mkForeign (FFun "sel_registerName" [FString] FPtr) selname 


This is a module defining an `Object` typeclass and two functions: one for getting classes and one for
getting selectors from the runtime. Idris' FFI defines a `Ptr` type, which represents, as you would expect,
a general (`void *`) C pointer. Every Objective-C object type in this exercise is an instance of
the `Object` typeclass, including classes themselves (more later), and function `getPtr` is used to 
retrieve the underlying foreign pointer when needed. Like function/method calls to Objective-C, and
C function calls in general, the retrieval of the pointer is IO monadic.

Also note the `%include` statements, which directly parse C headers. I ran into some problems
with the type signatures of a couple of functions, so I created stubs. The stub prototype used in 
this example module is:

    extern void* objc_msgSend(void* self, void* op, ...);

It should be possible to get rid of at least some of these stubs.


Objective-C classes
-------------------

Next up is an example of a specific Objective-C class. I started off with `NSObject`, putting it in its own
module:

<!--?prettify lang=hs?-->

    module NSObject

    import objective_c

    -------------------------------------------------------------------------------
    data NSObject'Class = Metaclass

    instance Object NSObject'Class where
       getPtr o = getClass "NSObject"

    -------------------------------------------------------------------------------
    metaclass : IO NSObject'Class
    -------------------------------------------------------------------------------
    metaclass = return Metaclass

    -------------------------------------------------------------------------------
    record NSObject : Type where
       asObject : (pointer : Ptr) -> NSObject

    instance Object NSObject where
       getPtr o = return (pointer o)

The type `NSObject'Class` represents the type of the class itself -- more precisely, in Objective-C terms, an instance of the object's metaclass. In other words, it's used to invoke class methods. To
get the metaclass instance, call the IO monadic `metaclass` function. Finally, type
`NSObject` represents an object, or instance of the objc class, used to invoke instance methods. You can
also see the for-internal-use implementations of `getPtr`.

I didn't bother to define any objc class or instance methods for `NSObject` here. Instead, 
I'll pick a more interesting example: `NSString`.

NSString
--------

I've fleshed out a few methods for `NSString` below so you'll be able to see a workable and 
hopefully slightly interesting "hello, world" (what else?) example. 

You might notice that the object arguments for each function (method) appear last. This allows the 
convenient use of Idris' bind operator, `>>=`, when calling them. You'll see examples of this 
in the final code snippet. I got the idea from the 
[HOC Haskell to Objective-C bindings](http://hoc.sourceforge.net/).

Idris provides a way to define functions for doing implicit type conversions. This is handy for safely using
our objc objects with superclass object functions (methods). Function `asNSObject` below
illustrates how we can do this.

Another thing to note is Idris' facility for named parameters/arguments, as seen in the definition
of function `stringByReplacingOccurrencesOfString` below. In the final code snippet, I'll show how it's used.

<!--?prettify lang=hs?-->

    module NSString

    import objective_c
    import NSObject 
    import NSRange

    -------------------------------------------------------------------------------
    data NSString'Class = Metaclass

    instance Object NSString'Class where
       getPtr o = getClass "NSString"

    -------------------------------------------------------------------------------
    metaclass : IO NSString'Class
    -------------------------------------------------------------------------------
    metaclass = return Metaclass

    -------------------------------------------------------------------------------
    record NSString : Type where
       asObject : (pointer : Ptr) -> NSString

    instance Object NSString where
       getPtr o = return (pointer o)

    -------------------------------------------------------------------------------
    implicit asNSObject : NSString -> NSObject
    -------------------------------------------------------------------------------
    asNSObject o = NSObject.asObject (NSString.pointer o)

    -------------------------------------------------------------------------------
    implicit liftNSString : NSString -> IO NSString
    -------------------------------------------------------------------------------
    liftNSString s = return s

    -------------------------------------------------------------------------------
    --------------------------- Class methods -------------------------------------
    -------------------------------------------------------------------------------

    -------------------------------------------------------------------------------
    stringWithUTF8String : String -> NSString'Class -> IO NSString
    -------------------------------------------------------------------------------
    stringWithUTF8String s c = do 
       obj <- getPtr c
       sel <- getSelector "stringWithUTF8String:" 
       result <- mkForeign (FFun "objc_msgSend" [FPtr, FPtr, FString] FPtr) 
                                                 obj   sel   s
       return (asObject result)

    -------------------------------------------------------------------------------
    --------------------------- Instance methods ----------------------------------
    -------------------------------------------------------------------------------

    -------------------------------------------------------------------------------
    length : NSString -> IO Int
    -------------------------------------------------------------------------------
    length o = do 
       obj <- getPtr o
       sel <- getSelector "length" 
       mkForeign (FFun "objc_msgSend" [FPtr, FPtr] FInt) obj sel

    -------------------------------------------------------------------------------
    UTF8String : NSString -> IO String
    -------------------------------------------------------------------------------
    UTF8String o = do 
       obj <- getPtr o
       sel <- getSelector "UTF8String" 
       mkForeign (FFun "objc_msgSend" [FPtr, FPtr] FString) obj sel

    -------------------------------------------------------------------------------
    stringByReplacingOccurrencesOfString : NSString -> (withString : NSString) ->
                                           NSString -> 
                                           IO NSString
    -------------------------------------------------------------------------------
    stringByReplacingOccurrencesOfString s r o = do 
       obj <- getPtr o
       sel <- getSelector "stringByReplacingOccurrencesOfString:withString:"
       old <- getPtr s
       new <- getPtr r
       result <- mkForeign (FFun "objc_msgSend" [FPtr, FPtr, FPtr, FPtr] FPtr) 
                                                 obj   sel   old   new      
       return (asObject result)

    -------------------------------------------------------------------------------
    substringWithRange : NSRange -> NSString -> IO NSString
    -------------------------------------------------------------------------------
    substringWithRange (NSMakeRange loc len) o = do
       obj <- getPtr o
       sel <- getSelector "substringWithRange:"
       result <- mkForeign (FFun "objc_msgSend" [FPtr, FPtr, FInt, FInt] FPtr) 
                                                 obj   sel   loc   len      
       return (asObject result)



Hello, world
------------

Putting it all together, we can now get a functioning Objective-C/Foundation example.

<!--?prettify lang=hs?-->

    module Main

    import NSString
    import NSFunctions

    -------------------------------------------------------------------------------
    %lib C "objc" 
    %link C "/System/Library/Frameworks/Foundation.framework/Foundation" 
    -------------------------------------------------------------------------------

    -------------------------------------------------------------------------------
    main : IO () 
    -------------------------------------------------------------------------------
    main = do

       recipient <- NSString.metaclass >>= stringWithUTF8String "world" 
       separator <- NSString.metaclass >>= stringWithUTF8String ", " 
   
       greeting <- NSString.metaclass >>= 
                      stringWithUTF8String "hello" >>=
                         stringByAppendingString separator >>=
                            stringByAppendingString recipient
   
       format <- NSString.metaclass >>= stringWithUTF8String "%@!"
   
       NSLog format greeting
   
       replacement <- NSString.metaclass >>= stringWithUTF8String "everyone" 
   
       greeting <- greeting >>= stringByReplacingOccurrencesOfString recipient {withString = replacement}

       NSLog format greeting

       substr <- greeting >>= substringWithRange (NSMakeRange 7 5)

       substrLen <- substr >>= length -- alternatively, "length substr"

       substring <- substr >>= UTF8String -- alternatively, "UTF8String substr"

       -- 'substring' is now a regular Idris string, and 'substrLen' is an Int
       putStrLn $ "Substring: " ++ substring
       putStrLn $ "Length: " ++ show substrLen   
   
       return ()


The output:

    2014-02-23 14:50:37.209 hello[1622:507] hello, world!
    2014-02-23 14:50:37.210 hello[1622:507] hello, everyone!
    Substring: every
    Length: 5


I intentionally use Idris' `>>=` bind in this code. I believe this works pretty well, overall.
You get something reminiscent of Objective-C message passing, you get nice chaining,
and you stick with idiomatic Idris monad conventions. Idris does allow some really cool 
user-defined syntax extensions, so if you wanted, you could actually do something like this:


    term syntax "[" [o] [s1] ":" [arg1] [s2] ":" [arg2] "]" = (return o) >>= (s1 arg1 {s2 = arg2})

    ...
    
    greeting <- [greeting stringByReplacingOccurrencesOfString: recipient 
                                                    withString: replacement]

This would effectively give you an EDSL for some very familiar-looking Objective-C code. As tempting
as something like this is, I would probably stay with an all- `>>=` approach.


Final thoughts
--------------

If you wanted to use standard Objective-C frameworks directly--i.e., not just a couple of adapter 
classes--you probably wouldn't want to code the bindings by hand. You would want a utility to 
parse the headers and generate something that looks like the `NSString` module example. This
shouldn't be too hard to do, actually. For example, given my experiences with 
[eero's source-to-source translator](https://github.com/eerolanguage/eero/wiki/Translator),
clang's rewriting facilities could probably handle it well. There would also be some work 
involved in getting things like memory management, returned structs, variadic functions, etc.
working seamlessly.

Also note that this is a contrived example. A good binding library would have more seamless
support for common conversions like native strings to/from Objective-C strings. And of course
you would want to do most of your string (or any other data) manipulations in Idris' pure 
functional world and just get the results over to Objective-C. This was just a 
convenient example.

You can find all of this sample code on [github](https://github.com/andyarvanitis/IdrisObjCExperiment).

Please feel free to 
[provide feedback via Twitter](https://twitter.com/intent/tweet?text=@andyarvanitis%20RE:%20idris%2Dobjc%20).





