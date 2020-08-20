hotwax
======

Coverage-guided binary fuzzing powered by Frida Stalker

A good introduction to the concept of coverage-guided fuzzing can be found on the [AFL repo](https://github.com/google/AFL#1-challenges-of-guided-fuzzing). Details on the Frida Stalker can be found [here](https://frida.re/docs/stalker/) (note that these examples are in JavaScript whereas this uses the C API which has little documentation.)

## How does this work?

The main hangup with fuzzing binaries is the lack of instrumentation. In traditional coverage-guided fuzzing scenarios, the target binary is instrumented ahead-of-time (AOT) using, for example, a compiler plugin which inserts function calls on basic block edges to report binary coverage. This provides a feedback mechanism to a generative algorithm which decides whether a mutated input was _useful_ or not. _Usefulness_ is based on a number of factors, with the primary one being how many _basic blocks_ a specific input reaches. The goal of this stochastic process is to maximize basic block _coverage_. The more code you reach, the more bugs you are likely to hit.

However, in our closed-box example, we do not have the ability to AOT instrument binaries because we do not have perfect control flow to operate off of. Here's where Frida comes in: we observe program execution, looking for basic block _terminators_ (jumps, branches, etc.) and dynamically insert machine instructions on these basic block edges. (This kind of dynamic instrumentation can be particularily complex at times, and is described in further detail in the [Frida Stalker](https://frida.re/docs/stalker/) article.) These machine instructions then call out into a fuzzer and provide it with coverage information. Now we can use AFL's well-tuned fuzzing algorithms by providing it with basic block coverage!

## How can I use it?

To get a list of build targets, run `make`:
```
Build all of the components for the given platform
    linux-x86
    linux-x86_64
    macos-x86_64
    android-x86_64

Delete all built artefacts
    clean

Build the AFL examples for comparison
    afl

Build the FRIDA GUM devkit
    devkit-linux-x86
    devkit-linux-x86_64
    devkit-macos-x86_64
    devkit-android-x86_64

Build stalker AFL fork mode example
    target-fork-linux-x86
    target-fork-linux-x86_64
    target-fork-macos-x86_64
    target-fork-android-x86_64

Build stalker AFL persistent mode example
    target-persistent-linux-x86
    target-persistent-linux-x86_64
    target-persistent-macos-x86_64
    target-persistent-android-x86_64

Build afl-clang stalker fork mode example
    fork-instr

Build afl-clang persistent mode example
    persistent-instr

Run stalker AFL fork mode example
    run-target-fork-linux-x86
    run-target-fork-linux-x86_64
    run-target-fork-macos-x86_64
    run-target-fork-android-x86_64

Run stalker AFL persistent mode example
    run-target-persistent-linux-x86
    run-target-persistent-linux-x86_64
    run-target-persistent-macos-x86_64
    run-target-persistent-android-x86_64

Run afl-clang stalker fork mode example
    run-fork-instr

Run afl-clang persistent mode example
    run-persistent-instr
```

The build of hotwax is dependent on the operating system and architecture on which it is built, the following platforms are supported:
* linux x86
* linux x86_64
* macos x86_64
* android x86_64

The make file will take care of downloading and building the appropriate FRIDA GUM devkit and AFL.

For example, in order to compare `afl-clang` instrumentation to that of `hotwax`'s stalker instrumentation in AFL's fork mode run:
* `make run-fork-instr`
* `make run-target-fork-linux-x86_64`

To perform the same comparison in AFL's persistent mode run:
* `make run-persistent-instr`
* `make run-target-persistent-linux-x86_64`

To fuzz custom software, replace the code in `target_X.c` and add test case(s) as appropriate. External libraries will be automatically instrumented (you can prevent this by adding exclusion calls in the respective target).

---

<img src="https://i.imgur.com/7FCrC97.png"></img>

## Trophies

If you find bugs with this tool, please make a PR to add to the trophy case.

## License

This software uses code from AFL which is licensed under the Apache 2.0 license and is copyright of Google Inc. Further, some code is lifted from the Frida project which is licensed under the wxWindows Library Licence, Version 3.1. See respective file headers for more details. All code in this repository of `@meme`'s copyright is under the Unlicense.