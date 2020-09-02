hotwax [![Bors enabled](https://bors.tech/images/badge_small.svg)](https://app.bors.tech/repositories/27272)
======

Coverage-guided binary fuzzing powered by Frida Stalker

A good introduction to the concept of coverage-guided fuzzing can be found on the [AFL repo](https://github.com/google/AFL#1-challenges-of-guided-fuzzing). Details on the Frida Stalker can be found [here](https://frida.re/docs/stalker/) (note that these examples are in JavaScript whereas this uses the C API which has little documentation.)

## How does this work?

The main hangup with fuzzing binaries is the lack of instrumentation. In traditional coverage-guided fuzzing scenarios, the target binary is instrumented ahead-of-time (AOT) using, for example, a compiler plugin which inserts function calls on basic block edges to report binary coverage. This provides a feedback mechanism to a generative algorithm which decides whether a mutated input was _useful_ or not. _Usefulness_ is based on a number of factors, with the primary one being how many _basic blocks_ a specific input reaches. The goal of this stochastic process is to maximize basic block _coverage_. The more code you reach, the more bugs you are likely to hit.

However, in our closed-box example, we do not have the ability to AOT instrument binaries because we do not have perfect control flow to operate off of. Here's where Frida comes in: we observe program execution, looking for basic block _terminators_ (jumps, branches, etc.) and dynamically insert machine instructions on these basic block edges. (This kind of dynamic instrumentation can be particularily complex at times, and is described in further detail in the [Frida Stalker](https://frida.re/docs/stalker/) article.) These machine instructions then call out into a fuzzer and provide it with coverage information. Now we can use AFL's well-tuned fuzzing algorithms by providing it with basic block coverage!

## How can I use it?

You need the `frida-gum-devkit`, which can be found here: https://github.com/frida/frida/releases (make sure you pick the correct OS and architecture)

You'll also need to have AFL built, enter `AFL` and `make` in the root.

To build `hotwax`, you need `meson`. Consult your package manager or the meson documentation for details. Use `meson build && cd build && ninja` to build.

To begin fuzzing the example targets, create the `testcase_dir` and place a seed in it: `mkdir -p testcase_dir && echo "AAA" > testcase_dir/sample.txt`. Then, use your AFL built to begin fuzzing the appropriate target, e.g.:

```
$ afl-fuzz -m 128 -i testcase_dir -o findings_dir ./build/target/target_persistent
```

To fuzz custom software, replace the code in `target_X.c` and add test case(s) as appropriate. External libraries will be automatically instrumented (you can prevent this by adding exclusion calls in the respective target).

To build the baseline, which is now deprecated:

1) Enter `AFL/llvm_mode` and run `make`
2) Build the baseline targets:
    ```
    $ ./AFL/afl-clang target.c baseline/fork_instr.c -o baseline/fork_instr
    $ ./AFL/afl-clang-fast target.c baseline/persistent_instr.c -o baseline/persistent_instr
    ```

---

<img src="https://i.imgur.com/7FCrC97.png"></img>

## Trophies

If you find bugs with this tool, please make a PR to add to the trophy case.

## License

This software uses code from AFL which is licensed under the Apache 2.0 license and is copyright of Google Inc. Further, some code is lifted from the Frida project which is licensed under the wxWindows Library Licence, Version 3.1. See respective file headers for more details. All code in this repository of `@meme`'s copyright is under the Unlicense.