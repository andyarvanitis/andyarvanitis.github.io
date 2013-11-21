---
layout: post
title: "Safe Categories for Objective-C"
date: 2013-11-20 11:47
comments: true
categories: 
---

My [previous blog post](/lazy-initialization-for-objective-c) introduced a technique for
[lazy initialization](http://en.wikipedia.org/wiki/Lazy_initialization) in Objective-C. 
Its main advantage is the simple way of using it: instead of sending `alloc` or `new` to a
class to create an instance, you send `lazy`, which could be added to `NSObject` 
via a category. 

There is, of course, a big problem with adding a “nice and clean” method to 
a class with a category, especially `NSObject` — and that’s silent name collisions. 
To make matters worse, you can't even predict which method you'll end up with when multiple versions
exist.
The solution, as we all know, is to ~~uglify~~ try to make the name unique, typically by adding
a prefix like "axa_", yielding something like `axa_lazy`. Yuck.

It would be nice if we could have "safe" categories. Like just about everything else,
it's been discussed before (e.g., [Graham Lee's recent blog post](http://blog.securemacprogramming.com/2013/04/can-objective-c-be-given-safe-categories/)), and there are some good implementations out there (e.g., [jspahrsummers / libextobjc](https://github.com/jspahrsummers/libextobjc)). This is another shot at it. The goal is something
that is lightweight enough to avoid a library/framework (or lots of macros), and yet as safe as
possible -- maybe even completely safe.

There's only so much we can do here at compile time, so the general strategy will be to use the 
[Objective-C runtime facilities](https://developer.apple.com/library/mac/documentation/cocoa/Conceptual/ObjCRuntimeGuide/Introduction/Introduction.html#//apple_ref/doc/uid/TP40008048) to extend classes, and use runtime assertions to catch/report conflicts. 

Let's start with the interface. We can just use a standard category declaration (the same used in the previous blog post):
    @interface NSObject (AXALazyInitCategory)
    + (instancetype)lazy;
    @end

For the implementation, our approach uses a subclass of the target class:

    #import <objc/runtime.h>
    #import "Object+AXALazyInitialization.h"

    @interface AXALazyInitCategory : NSObject // superclass is the category class
    @end

    static Class __targetClass;  // static, so private to this file
    static Class __sourceClass;  //

    @implementation AXALazyInitCategory

    + (void)load {
      __targetClass = [self superclass];
      __sourceClass = [self class];
      // do normal category-like +load stuff here, if desired...
    }

    + (instancetype)lazy {
      // see previous blog post on lazy initialization...
    }

    @end


So we've created a subclass of the desired category class, and during class load we saved off a static 
reference to the target (in this case, `NSObject`) and source (implementations provider) classes.

Now, in the same source file, we define two functions. One runs on program startup, the other on 
program termination:

    //-----------------------------------------------------------------------------
    // Constructor code -- runs afer all +load calls are made
    //-----------------------------------------------------------------------------
    __attribute__((constructor)) static void pre_run_add_category_methods() {
      @autoreleasepool {
        const SEL selector = @selector(lazy);
        const Method method = class_getClassMethod(__sourceClass, selector);

        NSCAssert(!class_getClassMethod(__targetClass, selector),
                  @"Safe category: redefined method '%@' found in class '%@'",
                  NSStringFromSelector(selector), __targetClass);

        class_addMethod(object_getClass(__targetClass), selector,
                        method_getImplementation(method),
                        method_getTypeEncoding(method));
      }
    }
    //-----------------------------------------------------------------------------
    // Destructor code -- runs during program termination
    //-----------------------------------------------------------------------------
    __attribute__((destructor)) static void post_run_check_category_methods() {
      @autoreleasepool {
        const SEL selector = @selector(lazy);
        const Method sourceMethod = class_getClassMethod(__sourceClass, selector);
        Method targetMethod = class_getClassMethod(__targetClass, selector);

        NSCAssert(method_getImplementation(targetMethod) == 
                  method_getImplementation(sourceMethod),
                  @"Safe category: redefined method '%@' found in class '%@'",
                  NSStringFromSelector(selector), __targetClass);
      }
    }

As noted, the function with the `constructor` attribute gets called after all `+load` methods have run. 
Using `class_getClassMethod`, we first check to make sure this class -- or any of its superclasses -- doesn't already have an implementation of the method. If the test passes, we
add it to the class, just as real categories add methods to classes. An alternative approach
might be to simply check the result of the `class_addMethod` call. However, it only fails if the class
*itself* already had the method; otherwise, it overrides any version provided by a superclass. I want to be
extra-careful with my technique, so I have it fail if one exists up the chain.

The assertion will fail (causing the program to terminate with an error message) if our method 
of interest was:

1. defined in the class (or a superclass) to start with, or
2. added by any "standard" category (since they are loaded before this code is run), or
3. added by any other runtime technique that *just happened to run before this one*.

Cases 1 and 2 are pretty solid, but of course we can't rely on 3. Thus we need to do another check
later, specifically in the function with the `destructor` attribute, which runs on program termination. 
What happens here is we check the registered method implementation for the class again. If it's not 
the one we added in this source file, we raise an error. This will catch any runtime method changes
that any source file or library might have done in our executable.

This is pretty good, but still not foolproof. I want to be extra-extra-careful (paranoid?). What if 
someone sneaked in a version of the method not in the class itself, but a subclass? 
Technically, it's a valid override, but I'm adding a method that I want to ensure is a safe
extension to the standard frameworks, with no unknown conflicts. The same goes for the superclasses
up the inheritance chain -- what if I don't want to accidentally override anything here? 

The following code is intended to handle all these situations. While we're at it, we should have 
a general way of adding and checking all of the methods in the "implementation provider" class, 
rather than doing it manually.

    // File SafeCategories.h
    #import <Foundation/Foundation.h>

    //---------------------------------------------------------------------------------------
    // Pseudo function that must be called in "category" +load method. 
    // The only public interface here.
    //---------------------------------------------------------------------------------------
    #define load_as_safe_category()        \
      __targetClass = [self superclass]; \
      __sourceClass = [self class];

    //---------------------------------------------------------------------------------------
    // Private static variable and function declarations
    //---------------------------------------------------------------------------------------
    static Class __targetClass = Nil;
    static Class __sourceClass = Nil;

    static NSString* const __RedefinedMethodFormattedErrorMessage =
        @"Safe category: redefined method '%@' found in class '%@'";

    static NSArray* get_related_classes(Class baseClass);
    static void process_methods(const Class sourceClass, const Class targetClass,
                                void (^method_operation)(Class, Method));

    //---------------------------------------------------------------------------------------
    // Constructor code -- runs afer all +load calls are made
    //---------------------------------------------------------------------------------------
    __attribute__((constructor)) static void pre_run_add_category_methods() {
      @autoreleasepool {
        process_methods(__sourceClass, __targetClass, ^(Class cls, Method method) {
          const SEL sel = method_getName(method);
          NSCAssert(!class_getInstanceMethod(cls, sel), // works with metaclasses too
                    __RedefinedMethodFormattedErrorMessage,
                    NSStringFromSelector(sel), cls);
          class_addMethod(cls, sel, method_getImplementation(method), method_getTypeEncoding(method));
        });
      }
    }

    #if !defined(NS_BLOCK_ASSERTIONS) // We don't need any of this if assertions are disabled

    //---------------------------------------------------------------------------------------
    // Destructor code -- runs during program termination
    //---------------------------------------------------------------------------------------
    __attribute__((destructor)) static void post_run_check_category_methods() {
      @autoreleasepool {
        for (Class relatedClass in get_related_classes(__targetClass)) {
          process_methods(__sourceClass, relatedClass, ^(Class cls, Method method) {
            const SEL selector = method_getName(method);
            unsigned int methodsCount = 0;
            Method* methods = class_copyMethodList(cls, &methodsCount); // doesn't search superclasses
            for (unsigned int i = 0; i < methodsCount; i++) {
              if (method_getName(methods[i]) == selector) {
                NSCAssert(method_getImplementation(methods[i]) == method_getImplementation(method),
                          __RedefinedMethodFormattedErrorMessage,
                          NSStringFromSelector(selector), cls);
                break;
              }
            }
            free(methods);
          });
        }
      }
    }

    //---------------------------------------------------------------------------------------
    // Look through all of the classes registered with the runtime for super and subclasses.
    //---------------------------------------------------------------------------------------
    static NSArray* get_related_classes(Class baseClass) {
      NSMutableArray* relatedClasses = [NSMutableArray array];
      // First get the base and all its superclasses
      for (Class superClass = baseClass;
           superClass != Nil;
           superClass = class_getSuperclass(superClass)) {
        [relatedClasses addObject: superClass];
      }
      // Now get all subclasses of the base class
      int count = objc_getClassList(NULL, 0);
      Class* runtimeClasses = (__unsafe_unretained Class*)malloc(sizeof(Class) * count);
      count = objc_getClassList(runtimeClasses, count);
      for (NSInteger i = 0; i < count; i++) {
        Class superClass = runtimeClasses[i];
        do {
          superClass = class_getSuperclass(superClass);
        } while (superClass && superClass != baseClass);

        if (superClass != nil) {
          [relatedClasses addObject: runtimeClasses[i]];
        }
      }
      free(runtimeClasses);
      return relatedClasses;
    }

    #endif // !defined(NS_BLOCK_ASSERTIONS)

    //---------------------------------------------------------------------------------------
    static void process_methods(const Class sourceClass, const Class targetClass,
                                void (^method_operation)(Class, Method)) {
      void (^iterate_methods)(Class) = ^(Class cls) {
        const BOOL isMetaClass = class_isMetaClass(cls);
        unsigned int count = 0;
        Method* sourceMethods =
            class_copyMethodList(isMetaClass ? object_getClass(sourceClass) : sourceClass, &count);
        for (unsigned int i = 0; i < count; i++) {
          if (!isMetaClass || method_getName(sourceMethods[i]) != @selector(load)) {
            method_operation(cls, sourceMethods[i]);
          }
        }
        free(sourceMethods);
      };
      iterate_methods(object_getClass(targetClass));  // class methods
      iterate_methods(targetClass);                   // instance methods
    }

And as an example of how it's actually used, here's the original example rewritten:

    #import <objc/runtime.h>
    #import "Object+AXALazyInitialization.h"
    #import "SafeCategories.h"

    @interface AXALazyInitCategory : NSObject // superclass is the category class
    @end

    @implementation AXALazyInitCategory

    + (void)load {
      load_as_safe_category();
    }

    + (instancetype)lazy {
      // see previous blog post on lazy initialization...
    }
    @end


I feel this approach covers all the possible scenarios, but please feel free to [provide feedback via twitter](https://twitter.com/intent/tweet?text=@andyarvanitis%20RE:%20safe%20categories%20) if you think I've missed something.

In case you're wondering: yes, I am considering adding direct language support for something like this to 
[eero](http://eerolanguage.org) (though not quite yet).

















