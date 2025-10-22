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
- ... support for more operating systems other than Windows, *and* more architectures other than i386 and AMD64.

## What's present in the driver so far?
Here's what available with OmniMIDI v2 so far.
- ✔️ Runtime
   - 🔽 OS support
      - ✔️ Windows
      - ✔️ Linux
      - ⚠️ BSD
      - ❌ macOS
   - 🔽 Synth modules
      - ✔️ [BASSMIDI*](https://www.un4seen.com/bass.html) *(Requires extra config in xmake)*
      - ✔️ [FluidSynth](https://github.com/FluidSynth/fluidsynth)
      - ✔️ [XSynth](https://github.com/arduano/xsynth)
      - ✔️ External plugin system
   - 🔽 Features
      - ✔️ Audio limiter (anti-clipping)
      - ✔️ Hot reload of settings or SoundFont lists
      - ❌ Events overrides (change note length, ignore events etc...)
      - ❌ Events loopback
      - ❓ App blacklist system
   - ✔️ [WinMMWRP](https://github.com/KeppySoftware/WinMMWRP) support
- ⚠️ Configurator
- ⚠️ Debug window
- ❌ MIDI channel mixer

✔️ Implemented<br>
⚠️ Roughly implemented<br>
❓ Untested<br>
❌ Not implemented<br>

## How to compile
This guide will help you set up your environment and compile OmniMIDI v2 from source.

### Prerequisites

- A computer
- [Xmake](https://xmake.io/#/home)
- A GCC C++ compiler (Mingw or Clang)
- Git

### Steps

1. **Install Xmake**

   You can install Xmake by following its online guide: https://xmake.io/guide/quick-start.html#installation

2. **Download the compiler**

   **For Windows**: Download either **Mingw** or **Clang**, then extract and place it somewhere accessible (e.g., `F:\Compilers\mingw64`), then add it to the `PATH`.<br>
   Make sure to get both **i686** and **AMD64** versions of the compiler, and **ARM64** too if you're planning on compiling for *non-x86* CPUs.<br>
   OmniMIDIv2's Win32 compile process is tested against [w64devkit by skeeto](https://github.com/skeeto/w64devkit).<br><br>
   **For Linux**: Install **GCC** or **Clang** using the package manager for your distro.

3. **Clone the repo**

   Clone the repo somewhere on your device:

   ```sh
   git clone https://github.com/KeppySoftware/OmniMIDIv2.git
   ```

4. **Prepare the project**

   Open a terminal on the newly cloned repo, and navigate to the project's directory:

   ```sh
   cd OmniMIDI
   ```

   Then prepare the project for the compile process, and compile it.
   
   Set `--plat=<plat>` to `mingw` for Windows, otherwise `linux` for Linux.<br>
   Set `--arch=<arch>` to one of the following supported platforms: `i386`, `x86_64`, `arm64`<br>
   To compile with BASS support, append `--nonfree=y` at the end:

   ```sh
   xmake f -m release --plat=<plat> --arch=<arch>
   ```

   This should make xmake download all the required dependencies. Press `Y` when prompted.<br>

5. **Compile the project**

   After xmake is done downloading all the dependencies, you can begin compiling:

   ```sh
   xmake -v
   ```

   The compiled library will be in the `./build` folder.

6. **__(EXTRA)__ Winelib for Wine support**

   If you want to use OmniMIDIv2 Linux on a Win32 app, you'll need to compile the KDMAPI Winelib shim too.

   Go to the winelib folder:
   ```sh
   cd winelib
   ```

   Then run the compile process:
   ```sh
   ./compile.sh
   ```

   You'll get a file called **OmniMIDI.dll**.<br>
   Put this file next to the Win32 app you want to run through Wine, together with libOmniMIDI.so and the required synth libraries.

7. **Test the build**

   Test the build against something that can use it (WinMM under Win32, ALSAMIDI/winelib under Linux, KDMAPI for both).

   If you're under Linux, and you want to test the newly created build against a KDMAPI test app, remember to export the build's directory as a valid library loading path, by calling `export LD_LIBRARY_PATH=$PWD` in the terminal.

## Future of KDMAPI
[**Keppy's Direct MIDI API**](https://github.com/KeppySoftware/OmniMIDI/blob/master/DeveloperContent/KDMAPI.md), introduced in OmniMIDI *v1*, *will* continue to be supported in OmniMIDI *v2*.
<br />
I will make sure old applications will *still* be able to communicate with v2, but it will definitely be deprecated in favor of a better API implementation with a *better name*. To be continued...