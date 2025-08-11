/*
 * SPDX-License-Identifier: MIT
 *
 * OmniMIDI
 *
 * Copyright (c) 2024 Keppy's Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the MIT License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * MIT License for more details.
 *
 * You should have received a copy of the MIT License along with this
 * program.  If not, see <https://opensource.org/license/mit/>.
 */

#pragma once

#ifndef _UTILS_H
#define _UTILS_H

#include "ErrSys.hpp"
#include <cassert>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>

#include <cassert>
#include <guiddef.h>
#include <strsafe.h>
#include <shlobj.h>

#define TYPE %s
#define TYPENO TYPE
#define LIBSUFF ".dll"
#else
#include <dlfcn.h>
#include <sys/time.h>
#include <unistd.h>

#define TYPE lib%s.so
#define TYPENO lib%s
#define LIBSUFF ".so"
#endif

#define STREXP(X) #X
#define LIBEXP(X) STREXP(X)

namespace OMShared {
enum FIDs {
    CurrentDirectory,
    UserFolder,
    PluginFolder,
    LibGeneric,
    LibLocal,
    Libi386,
    LibAMD64,
    LibAArch64
};

struct LibVersion {
    unsigned char Build = 0;
    unsigned char Major = 0;
    unsigned char Minor = 0;
    unsigned char Rev = 0;

    LibVersion(unsigned int raw) {
        Build = (raw >> 24) & 0xFF;
        Major = (raw >> 16) & 0xFF;
        Minor = (raw >> 8) & 0xFF;
        Rev = raw & 0xFF;
    }

    unsigned int GetHiWord() { return (Build << 8 | Major) & 0xFFFF; }
    unsigned int GetLoWord() { return (Minor << 8 | Rev) & 0xFFFF; }
    unsigned int GetRaw() { return GetHiWord() << 16 | GetLoWord(); }
};

class LibFuncs {
  public:
    static void *Load(const char *path);
    static bool Free(void *libPtr);
    static void *GetLibraryAddress(const char *name);
    static void *GetFuncAddress(void *ptr, const char *funcName);
};

class LibImport {
  protected:
    void **funcptr = nullptr;
    const char *funcname = nullptr;

  public:
    LibImport(void **pptr, const char *pfuncname) {
        funcptr = pptr;
        *(funcptr) = nullptr;
        funcname = pfuncname;
    }

    ~LibImport() { *(funcptr) = nullptr; }

    const char *GetName() { return funcname; }
    bool LoadFailed() {
        return funcptr == nullptr ||
               (funcptr != nullptr && *(funcptr) == nullptr);
    }

    void *GetPtr() { return *(funcptr); }
    bool SetPtr(void *lib = nullptr, const char *ptrname = nullptr);
};

class Lib {
  private:
    const char *Name = nullptr;
    const char *Suffix = nullptr;
    void *Library = nullptr;
    bool Initialized = false;
    bool LoadFailed = false;
    bool AppSelfHosted = false;
    ErrorSystem::Logger *ErrLog = nullptr;

    LibImport *Funcs = nullptr;
    size_t FuncsCount = 0;

    bool IteratePath(char *outPath, OMShared::FIDs fid);

  public:
    const char *GetName() { return Name; }
    void *GetPtr() { return Library; }
    bool IsOnline() {
        return (Library != nullptr && Initialized && !LoadFailed);
    }

    Lib(const char *pName, const char *Suffix, ErrorSystem::Logger *PErr,
        LibImport **pFuncs = nullptr, size_t pFuncsCount = 0);
    ~Lib();

    bool GetLibPath(char *outPath = nullptr);
    bool LoadLib(char *CustomPath = nullptr);
    bool UnloadLib();
    bool IsSupported(uint32_t loaded, uint32_t minimum);
};

class Funcs {
#ifdef _WIN32
  private:
    void *ntdll = nullptr;
    bool LL = false;
    uint32_t(WINAPI *pNtDelayExecution)(uint8_t, int64_t *) = nullptr;
    uint32_t(WINAPI *pNtQuerySystemTime)(int64_t *) = nullptr;
#endif

  public:
    Funcs();
    ~Funcs();

    void MicroSleep(int64_t v);
    uint32_t QuerySystemTime(int64_t *v);

#ifdef _WIN32
    wchar_t *GetUTF16(char *utf8);
#endif

    bool GetFolderPath(const FIDs fID, char *folderPath, size_t szFolderPath);
    bool CreateFolder(char *folderPath, size_t szFolderPath);
    bool DoesFileExist(std::string filePath);
    bool OpenFile(std::string filePath);
};

class MIDIUtils {
  public:
    static constexpr uint8_t GetStatus(uint32_t ev) { return (ev & 0xFF); }
    static constexpr uint8_t GetCommand(uint8_t status) {
        return (status & 0xF0);
    }
    static constexpr uint8_t GetChannel(uint8_t status) {
        return (status & 0xF);
    }

    static constexpr uint8_t GetFirstParam(uint32_t ev) {
        return ((ev >> 8) & 0xFF);
    }
    static constexpr uint8_t GetSecondParam(uint32_t ev) {
        return ((ev >> 16) & 0xFF);
    }
    static constexpr uint16_t MakeFullParam(uint8_t p1, uint8_t p2,
                                            uint8_t bit) {
        return (p2 << bit) | p1;
    }
};
} // namespace OMShared

#endif