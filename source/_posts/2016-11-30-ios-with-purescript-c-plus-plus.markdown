---
layout: post
title: "iOS (and macOS) with PureScript C++"
date: 2016-11-30 9:34
comments: true
categories: 
---

Thanks to Xcode's support for custom script build phases and environment variables compatible with GNU Make, you can integrate your PureScript code fairly painlessly. In this post I'll show you the basics of using PureScript in an Xcode project. I'm just going to focus on that, so you won't get any details on things like the (bidirectional) FFI here -- I plan to cover that in another post.

To keep things simple, in this post I'll assume:

* You're running macOS El Capitan (10.11) or later
* You're already familiar with PureScript, and have a recent version installed
* You have a recent version of Xcode (7.3+) installed
* You're familiar with the basics of iOS/macOS development using Xcode

These instructions should apply to all Xcode-based targets, although I've only tested iOS and macOS for now.

**Step 1: Install the PureScript C++ compiler**

See [my previous post](/hello-world-purescript-cpp/), but for proper operation with Xcode, make sure the `pcc` program is installed in the same directory as the other PureScript utilities (particulary `psc-package`). This restriction might be removed later. Also be sure to use a version dated **2016-11-22** or later, since that's when direct support for Xcode was added.

**Step 2: Create a new Xcode iOS project in the standard way**

1. Using Xcode's menus:

    File ->  
    New ->  
    Project...->  
    iOS ->  
    Single View Application ->  
    Language: Objective-C (for now, we'll do Swift in a later post)

2. Save the project anywhere you like
3. Make sure to do your normal selection of Team, Provisioning Profile, etc. If you aren't a registered developer and only plan to try this on the simulator, I believe you won't need them.

**Step 3: Create some PureScript code to use in your project**

1. Create a directory anywhere you like. We'll refer to it as your PureScript working directory. In that directory, create a subdirectory called "`src`".

2. In `src`, create a PureScript source file, naming it "`PMath.purs`". Add the following code to `PMath.purs`:

<ul style="position: relative; top: -2em; margin-bottom: -2em">
{% codeblock lang:haskell %}
module PMath where

import Prelude

fib :: Int -> Int
fib 0 = 0
fib 1 = 1
fib n = fib (n - 2) + fib (n - 1)
{% endcodeblock %}
</ul>

<ol start="3">
<li>
In the Terminal, from your working directory (one level up from <code>src</code>), run the <code>pcc</code> command with just the <code>--xcode</code> option:

<span style="position: relative; top: 1em;">

{% codeblock lang:text %}
~/Projects/BlogPureScript/$ pcc --xcode

Generating psc-xcode.sh...
Done
Add this file to an Xcode 'Run Script' build phase.

Generating psc-package.json...
Done
Use the 'psc-package' utility to install or update packages.

Generating Makefile... pcc executable location /Users/andy/.local/bin/pcc
Done

Run 'make' or 'make release' to build an optimized release build.
Run 'make debug' to build an executable with assertions enabled and
suitable for source-level debugging.

The resulting binary executable will be located in output/bin (by default).

~/Projects/BlogPureScript/$ 
{% endcodeblock %}

</span>
</li>
</ol>

**Step 4: Add PureScript code to your project settings**

1. From Xcode's "Project navigator," select your project (topmost level). You should see various tabs in the main window. Select "Build Phases" and:
    a. Click the "+" -> "New Run Script Phase"
    b. Drag the new "Run Script" section to be *before* "Compile Sources"
    c. Expand the "Run Script" section (triangle), and in its editor add the following:

            cd path/to/your/working_dir
            sh psc-xcode.sh

      It should look similar to this screenshot:
      
      {% img /images/run_script.png %}

2. Now go to the "Build Settings" tab. Using the embedded search control (upper right), enter "header search". If you don't see any results, make sure you have the "All" button selected (instead of "Basic" or "Customized"). Select the "Header Search Paths" item and add the value "`path/to/your/working_dir/output`" to it, meaning the "`output`" subdirectory in your working directory (it doesn't exist yet, but it will be generated during the build process). Make sure to press "return" to save the value.

      {% img /images/search_paths.png %}

3. Again from "Build Settings," change your search term to "linker flags" to find "Other Linker Flags." Add the values<br>"`-lc++ path/to/your/working_dir/output/bin/purescript.o`", as shown below. This "`.o`" file is a native combined object file containing all of your compiled PureScript modules (sort of like a static lib), which will be generated during the build process. Again, remember to press "return".

      {% img /images/linker_flags.png %}

**Step 5: Add some Objective-C++ code that calls your PureScript code**

1. In your Xcode project, rename `ViewController.m` to `ViewController.mm` -- make sure you do this from Xcode, not the Terminal.

2. In `ViewController.mm` you should see implementations for methods like `viewDidLoad`. Add the implementation for `viewDidAppear` shown below, as well as adding `#import "PMath/PMath.hh"` and `using PMath::fib;` near the top. Ignore any of the "live" errors for now.

<ul style="position: relative; top: -2em;">

{% codeblock lang:objective-c %}
#import "ViewController.h"
#import "PMath/PMath.hh"

// This isn't necessary if we qualify our calls to the
// function as PMath::fib(x) -- but it looks nicer...
//
using PMath::fib;

@interface ViewController ()
@end

@implementation ViewController

- (void)viewDidLoad {
  [super viewDidLoad];
}

- (void)viewDidAppear:(BOOL) animated {
  [super viewDidAppear:animated];

  // Direct call to our PureScript function
  //
  int result = fib(10);

  NSString* resultString =
    [NSString stringWithFormat:@"Result of fib 10 is %d", result];

  UIAlertController * alert =
    [UIAlertController alertControllerWithTitle:@"Value from PureScript"
                                        message:resultString
                                 preferredStyle:UIAlertControllerStyleAlert];
  id okAction =
    [UIAlertAction actionWithTitle:@"OK"
                             style:UIAlertActionStyleDefault
                           handler:^(UIAlertAction * _Nonnull action) {
                           }];

  [alert addAction:okAction];
  [self presentViewController:alert animated:YES completion:nil];
}

@end
{% endcodeblock %}
</ul>

**Step 6: Build the project and run it**

1. For now, make sure your project is set to build a simulator target (it should be, by default). Build using Xcode's usual menu, Product -> Build (or shortcut Command-B). You should get a "build succeeded".

2. Run it! (menu Product -> Run or shortcut Command-R)

      {% img /images/ios_app.png 375 %}

3. With Debug builds, you can use Xcode's debugger as well (on the generated C++ code):

      {% img /images/debugger.png %}

**Next steps**

You can now proceed to make changes to any source files (including your PureScript ones) and simply rebuild/test/repeat using Xcode. As you're developing your app, if you just want to type check your PureScript changes (not recompile all the way to a binary target), use Terminal to run "`make codegen`" from your working directory.

If you take a peek at the `psc-xcode.sh` script, you'll see that your compiled PureScript C++ objects will respect your project settings for things like Debug/Release builds and target architecture -- and will rebuild appropriately if any of these things change. This also means you can do device hardware builds; as with any normal iOS project, simply change your target to one of the device variants and build.

