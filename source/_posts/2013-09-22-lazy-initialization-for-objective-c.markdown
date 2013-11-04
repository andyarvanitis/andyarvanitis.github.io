---
layout: post
title: "Lazy initialization for Objective-C"
date: 2013-09-22 14:47
comments: true
categories: 
---

[Lazy initialization](http://en.wikipedia.org/wiki/Lazy_initialization) is a 
technique for delaying a computationally or memory-intensive operation until 
it's absolutely necessary. It's helpful for getting an object's expensive 
creation or initialization out of the critical path, improving responsiveness or
memory use where it matters most.

It's a useful coding pattern. So useful, in fact, that some languages provide
direct support for it. Having used it many times in my career -- and most
recently in some Android Java code -- I've thought about it in the context of 
my [Eero language project](http://eerolanguage.org/). I wondered if I should 
consider adding support for it in the Eero language itself (if not now, then 
maybe in a future revision). But before I went that route, I thought I should 
have a go at figuring out a clean way to do it in standard Objective-C (and Eero,
in its current form). My goal was to find a way to do it as unobtrusively as
possible. I wanted minimal impact to the class/object to which it would be 
applied, making it akin to built-in language support, if at all possible. I'd 
settle for either a simple and clean pattern, or a small and non-fragile library.

This is what I came up with. It's probably been done before, in a similar, or 
perhaps even the exact same way, by someone else. But not having spotted it 
documented anywhere else, I thought I'd share my results.

Note that, beyond what I've already said, I won't provide a detailed argument 
for lazy initialization's utility, or say much about how it shouldn’t be
overused (*it shouldn't be overused!*). I just want to document what I think is 
a nice way to do it. 

## What using it looks like

I'll jump right to the final results here, at least from the perspective of the
user of the pattern/library. 

Given a *type-safe* class method in a category has been defined on NSObject,
like so:

    @interface NSObject (LazyInitialization)
      + (instancetype)lazy;
    @end

Class method `lazy` takes the place of both `alloc` and `new` when creating a
lazy instance. It can be chained with an `init` method or used on its own.

    SomeClass *object = [SomeClass lazy];

    // ... do stuff
    // ... sometime later...

    [object doSomething]; // object will get initialized here, 
                          // and then perform 'doSomething'

That's it. No special checks before or within the first method call.
The lazy object can be of any class type. The method called can be a
regular instance method, or a property getter or setter. Any notation (e.g., 
property dot-notation) can be used. Either ARC or MRC is also fine.

As mentioned, class method `lazy` can be used standalone (acting like `new` when
the object goes active), chained with `init`, or chained with an `initWith` method. 
The last case requires a bit of caution for pointers. Specifically, Objective-C 
objects passed in get a `retain` and automatic `release`; C strings (any 
`char*`, actually) get *copied* and freed automatically; and any other pointer 
types are left alone. So for objects and C strings, there's nothing special you
need to do (although you’ll want to watch out for large string buffer copies). 
But for pointers to any other type of data, you need to make sure the data stays
valid until the object fully initializes.

    NSMutableString *str = [NSMutableString new];
    [str appendString:@"Hello"];
    ClassX *obj1 = [[ClassX lazy] initWithString:str]; // str retained

    ClassX *obj2 = [[ClassX lazy] initWithCString:"Hi"]; // char* copied

    void *buf = ...
    ClassX *obj3 = [[ClassX lazy] initWithBuffer:buf]; // nothing done


## How it all works

Class method `lazy` returns an instance of a proxy object for the specified 
class. The proxy then uses standard message forwarding to cache the `init` 
invocation for later, and to create the real object instance at the appropriate 
time and forward all subsequent messages. Memory management messages are not 
forwarded, since they need to manage the lifetime of the proxy, which owns the 
"real" object.

The base class of the proxy object is Foundation's `NSProxy`. As you would 
imagine, it's designed for just this sort of purpose. 

As for the forwarding mechanism, Foundation and the runtime provide two 
different means: `forwardingTargetForSelector`, and `methodSignatureForSelector`
combined with `forwardInvocation`. Of the two, `forwardingTargetForSelector` is
faster. In the following code, the second, slower method(s) is only used to 
cache the `init` method. Once the init happens, the faster method is used 
exclusively and for the majority of the object's active life.

So here's the final code in Objective-C:

*lazyinit.h*
    #import <Foundation/Foundation.h>

    @interface NSObject (LazyInitialization)
      + (instancetype)lazy;
    @end

*lazyinit.m*
    #import <Foundation/Foundation.h>
    #import "lazyinit.h"

    #if !__has_feature(objc_arc)
       #error ARC must be enabled for this source file (but clients can use MRC).
    #endif

    @interface LazyProxy : NSProxy
    - (instancetype)initWithClass:(Class)class;
    @end

    @implementation NSObject (LazyInitialization)
    + (instancetype)lazy {
      return (id)[LazyProxy.alloc initWithClass: [self class]];
    }
    @end

    @implementation LazyProxy {
      id _object;
      Class _objectClass;
      NSInvocation *_initInvocation;
    }

    - (instancetype)initWithClass:(Class)cls {
      _objectClass = cls;
      return self;
    }

    - (void)instantiateObject {
      _object = [_objectClass alloc];
      if (_initInvocation == nil) {  // allow SomeClass.lazy (no explicit init)
        _object = [_object init];
      } else {
        [_initInvocation invokeWithTarget:_object];
        [_initInvocation getReturnValue:&_object];
        _initInvocation = nil;
      }
    }

    - (id)forwardingTargetForSelector:(SEL)selector {
      if (_object == nil) {  // once set, fast forwarding is in effect
        if (![NSStringFromSelector(selector) hasPrefix:@"init"]) {
          [self instantiateObject];
        }
      }
      return _object;
    }

    - (NSMethodSignature *)methodSignatureForSelector:(SEL)selector {
      NSMethodSignature *signature =
          [_objectClass instanceMethodSignatureForSelector:selector];
      return signature;
    }

    // If we got here, it had to be from an init method
    - (void)forwardInvocation:(NSInvocation *)invocation {
      _initInvocation = invocation;
      [_initInvocation setTarget:nil];  // not needed, and we don't want to retain
      [_initInvocation retainArguments];
      // For the immediate init(With...) call, return the proxy itself
      [_initInvocation setReturnValue:(void *)&self];
    }

    //----------------------------------------------------------------------------
    // Implemented by NSProxy, so we need to forward these manually
    //----------------------------------------------------------------------------

    - (Class)class {
      if (_object == nil) {
        [self instantiateObject];
      }
      return [_object class];
    } 

    - (Class)superclass {
      if (_object == nil) {
        [self instantiateObject];
      }
      return [_object superclass];
    }

    - (BOOL)conformsToProtocol:(Protocol *)aProtocol {
      if (self->_object == nil) {
        [self instantiateObject];
      }
      return [self->_object conformsToProtocol:aProtocol];
    }

    - (NSString *)description {
      if (self->_object == nil) {
        [self instantiateObject];
      }
      return [self->_object description];
    }

    - (NSUInteger)hash {
      if (self->_object == nil) {
        [self instantiateObject];
      }
      return [self->_object hash];
    }

    - (BOOL)isEqual:(id)obj {
      if (self->_object == nil) {
        [self instantiateObject];
      }
      return [self->_object isEqual:obj];
    }

    - (BOOL)isKindOfClass:(Class)aClass {
      if (self->_object == nil) {
        [self instantiateObject];
      }
      return [self->_object isKindOfClass:aClass];
    }

    - (BOOL)isMemberOfClass:(Class)aClass {
      if (self->_object == nil) {
        [self instantiateObject];
      }
      return [self->_object isMemberOfClass:aClass];
    }

    - (BOOL)respondsToSelector:(SEL)selector {
      if (self->_object == nil) {
        [self instantiateObject];
      }
      return [self->_object respondsToSelector:selector];
    }

    @end

## Eero version

The Objective-C version can of course be 
[used directly in Eero](http://eerolanguage.org/documentation/index.html#importsincludes), 
but since this started out as Eero code, you'll find the Eero version below.

(By the way, the ObjC example above was automatically converted to Objective-C 
using [Eero's source-source translator](https://github.com/eerolanguage/eero/wiki/Translator)
and formatted using [clang-format](http://clang.llvm.org/docs/ClangFormat.html)).

*lazyinit.eeh*
    #import <Foundation/Foundation.h>

    interface Object (LazyInitialization)
      +lazy, return instancetype
    end

*lazyinit.eero*
    #import <Foundation/Foundation.h>
    #import 'lazyinit.eeh'

    #if !__has_feature(objc_arc)
      #error ARC must be enabled for this source file (but clients can use MRC).
    #endif

    interface LazyProxy : Proxy
      initWithClass: Class, return instancetype   
    end

    implementation Object (LazyInstantiation)
      +lazy, return instancetype
        return (id)LazyProxy.alloc.initWithClass: self.class
    end

    implementation LazyProxy 
      id _object
      Class _objectClass
      Invocation _initInvocation

      initWithClass: Class cls, return instancetype
        _objectClass = cls
        return self

      instantiateObject
        _object = _objectClass.alloc
        if _initInvocation == nil // allow SomeClass.lazy (no explicit init)
          _object = _object.init
        else
          _initInvocation.invokeWithTarget: _object
          _initInvocation.getReturnValue: &_object
          _initInvocation = nil

      forwardingTargetForSelector: SEL, return id
        if _object == nil // once set, fast forwarding is in effect
          if not StringFromSelector(selector).hasPrefix: 'init'
            self.instantiateObject
        return _object

      methodSignatureForSelector: SEL, return MethodSignature
        signature := _objectClass.instanceMethodSignatureForSelector: selector 
        return signature

      // If we got here, it had to be from an init method
      forwardInvocation: Invocation
        _initInvocation = invocation
        _initInvocation.setTarget: nil // not needed, and we don't want to retain
        _initInvocation.retainArguments
        // For the immediate init(With...) call, return the proxy itself
        _initInvocation.setReturnValue: (void*)&self

      //--------------------------------------------------------------------------
      // Implemented by NSProxy, so we need to forward these manually
      //--------------------------------------------------------------------------

      -class, return Class
        if _object == nil  
          self.instantiateObject
        return _object.class

      -superclass, return Class
        if _object == nil  
          self.instantiateObject
        return _object.superclass

      conformsToProtocol: Protocol aProtocol, return BOOL
        if _object == nil  
          self.instantiateObject
        return _object.conformsToProtocol: aProtocol

      description, return String
        if _object == nil  
          self.instantiateObject
        return _object.description

      hash, return UInteger
        if _object == nil  
          self.instantiateObject
        return _object.hash

      isEqual: id obj, return BOOL
        if _object == nil  
          self.instantiateObject
        return _object.isEqual: obj

      isKindOfClass: Class aClass, return BOOL
        if _object == nil  
          self.instantiateObject
        return _object.isKindOfClass: aClass

      isMemberOfClass: Class aClass, return BOOL
        if _object == nil  
          self.instantiateObject
        return _object.isMemberOfClass: aClass

      respondsToSelector: SEL, return BOOL
        if _object == nil  
          self.instantiateObject
        return _object.respondsToSelector: selector

    end

So there you have it. Please feel free to 
[provide feedback via twitter](https://twitter.com/intent/tweet?text=@andyarvanitis%20).
I also plan to submit the resulting library to [Cocoapods](http://cocoapods.org/).
