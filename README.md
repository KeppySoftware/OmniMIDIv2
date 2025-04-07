# OmniMIDI v2 (15.0+)
New codebase for OmniMIDI, completely rewritten from scratch using code laid down through various tests in the [Shakra branch](https://github.com/KaleidonKep99/ShakraDrv).

## Why a new branch?
Two reasons; the old code was too bloated, and I wanted to completely move the new release as far away from it as possible.

This code has been written completely in **C++**, instead of taping random code on top of the already unstable *C* code from OmniMIDI v1 *(pre 15.0)*. This will get rid of all the issues that have been plaguing the old branch so far, while also making the code more readable by normal human beings. ~~I became unable to read my own code in there...~~

## Why not Rust?
I feel like I should probably get to know C++ more, before moving to Rust. A Rust rewrite is not out of the question though! Maybe someday.

## What are the main differences between v1 and v2?
Here are the main differences:
- v2 is pure C++ code. *(Duh!)*
- Written with modularity in mind, to allow other developers to take full advantage of it *(Modules support!)* and... 
- ... support for more operating systems other than Windows, *and* more architectures other than i386 and AMD64, thanks to a built-in implementation of [TinySoundFont](https://github.com/schellingb/TinySoundFont)! *(Both a WinRT ARM32 and Linux AMD64 release have been tested internally so far.)*

## What's present in the driver so far?
Here's what available with OmniMIDI v2 so far.
- ✔️ Driver (Uh...)
    - ✔️ Support for Windows **and** Linux
    - ✔️ Modular synthesizer system, with built-in support for:
        - ✔️ [BASSMIDI](https://www.un4seen.com/bass.html) *(With support for WASAPI, ASIO and DirectSound)*
        - ✔️ [FluidSynth](https://github.com/FluidSynth/fluidsynth)
        - ✔️ [XSynth](https://github.com/arduano/xsynth)
    - ❌ Events parser overrides (change note length, ignore events etc...)
    - ✔️ Support for hot reload of settings or SoundFont lists
    - ❌ MIDI Feedback feature
    - ✔️* Blacklist system
    - ✔️ Audio limiter (anti-clipping)
    - ✔️ WinMMWRP support
- ❌ Configurator
- ❌ MIDI channel mixer
- ✔️ Debug window

## How to compile
### Windows

## Future of KDMAPI
[**Keppy's Direct MIDI API**](https://github.com/KeppySoftware/OmniMIDI/blob/master/DeveloperContent/KDMAPI.md), introduced in OmniMIDI *v1*, *will* continue to be supported in OmniMIDI *v2*.
<br />
I will make sure old applications will *still* be able to communicate with v2, but it will definitely be deprecated in favor of a better API implementation with a *better name*. To be continued...

## Will it work on my Pentium 4 PC with Windows XP...
Stop asking. No.