---
layout: post
title: '“Hello, world” with PureScript C++'
date: 2016-11-16 13:36
comments: true
categories: 
---

This is a short-as-possible demonstration of using [PureScript's C++11 backend](https://github.com/pure11/pure11) (a.k.a. pure11) to build a native command-line application.

To keep things simple, in this post I'll assume:

* You're running macOS El Capitan (10.11) or later
* You're already familiar with PureScript, and have a recent version installed
* You have either a recent version of Xcode (7.3+) or its command-line tools installed (`git`, `clang`, and `make` need to be there)

I'll go through the steps, and then provide a few extra details afterwards.

**Step 1: Install the PureScript-to-C++11 compiler,** `pcc`

1. Go to the [pure11 repo releases](https://github.com/pure11/pure11/releases) and download the latest `pcc.zip` file
2. Unzip and copy to a location on your path

**Step 2: Write your “hello, world” program**

1. Make a working directory anywhere you like, named anything you like
2. Make a subdirectory there called “`src`”
3. Create your source file in `src` with contents:

``` haskell
module Main where

import Prelude
import Control.Monad.Eff (Eff)
import Control.Monad.Eff.Console (CONSOLE, log)

main :: Eff (console :: CONSOLE) Unit
main = do
log "hello, world!"
```

**Step 3: Generate configuration files (**`Makefile` **and** `psc-package.json`**)**

From your working directory run the `pcc` command with no arguments:

    $ pcc

You should see something like:

    Generating Makefile... pcc executable location /Users/andy/.local/bin/pcc
    Done

    Run 'make' or 'make release' to build an optimized release build.
    Run 'make debug' to build an executable with assertions enabled and
    suitable for source-level debugging.

    The resulting binary executable will be located in output/bin (by default).

**Step 4: Build and run**

1. Again from your working directory, run the `make` command:

        $ make

    You should see something like:

        Getting packages using 'psc-package'...
        Updating 3 packages...
        Updating console
        Updating eff
        Updating prelude
        Update complete
        ...
        Compiling Data.NaturalTransformation
        Compiling Data.Show
        Compiling Data.Boolean
        Compiling Control.Semigroupoid
        Compiling Control.Category
        Compiling Data.Void
        Compiling Data.Unit
        ...
        Compiling Main
        Creating output/Control/Applicative/Applicative.o
        Creating output/Control/Apply/Apply.o
        Creating output/Control/Apply/Apply_ffi.o
        Creating output/Control/Bind/Bind.o
        ...
        Linking output/bin/main


2.  Run your new program:

        $ ./output/bin/main 
        hello, world!

And that's it!

**Next steps**

As you make any changes to your source (`.purs`) file(s), simply run `make` again to rebuild.

Using PureScript's new `psc-package` utility, you can install more packages (libraries of modules) from a verified package set.

This is by no means the only thing you can do with the tools, nor is it the only plaform supported -- I regularly test on Linux (gcc) and occasionally Windows 10 (Visual Studio 2015) -- but it should give you the basics of getting up and running.
