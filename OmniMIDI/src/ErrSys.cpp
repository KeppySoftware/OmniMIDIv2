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

#include "ErrSys.hpp"
#include <iostream>

std::mutex ErrorSystem::Logger::logMutex;

void ErrorSystem::Logger::ShowError(const char *Message, const char *Title,
                                    bool IsSeriousError) {
#if defined(_WIN32) && !defined(_M_ARM)
    MsgBox(NULL, Message, Title,
           IsSeriousError ? MB_ICONERROR
                          : MB_ICONWARNING | MB_OK | MB_SYSTEMMODAL);
#else
    std::cout << Message << std::endl;
#endif
}

void ErrorSystem::Logger::Log(const char *Message, const char *File,
                              const char *Func, const uint32_t Line, ...) {
    va_list vl;
    va_start(vl, Line);

    char *cBuf = new char[BufSize];
    char *tBuf = new char[BufSize];

    vsnprintf(tBuf, SZBufSize, Message, vl);
    dbgprintf;

    logMutex.lock();
    std::cout << cBuf << std::endl;
    logMutex.unlock();

    delete[] cBuf;
    delete[] tBuf;

    va_end(vl);
}

void ErrorSystem::Logger::ThrowError(const char *Error, bool IsSeriousError,
                                     const char *File, const char *Func,
                                     const uint32_t Line, ...) {
    va_list vl;
    va_start(vl, Line);

    if (Error == nullptr) {
#if defined(_WIN32) && !defined(_M_ARM)
        int GLE = GetLastError();
        LPSTR GLEBuf = nullptr;

        size_t MsgBufSize = FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                FORMAT_MESSAGE_ALLOCATE_BUFFER,
            NULL, GLE, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&GLEBuf, 0, NULL);

        if (MsgBufSize != 0) {
            MsgBox(NULL, GLEBuf, "OmniMIDI - ERROR",
                   IsSeriousError ? MB_ICONERROR
                                  : MB_ICONWARNING | MB_OK | MB_SYSTEMMODAL);
            LocalFree(GLEBuf);
        }
#else
        return;
#endif
    } else {
        char *tBuf = new char[BufSize];

        vsnprintf(tBuf, SZBufSize, Error, vl);

#if defined(_DEBUG) || defined(VERBOSE_LOG) || !defined(_WIN32)
        char *cBuf = new char[BufSize];
        dbgprintf;

        logMutex.lock();
        std::cout << cBuf << std::endl;
        logMutex.unlock();

        delete[] cBuf;
#endif

        MsgBox(NULL, tBuf, "OmniMIDI - ERROR",
               IsSeriousError ? MB_ICONERROR
                              : MB_ICONWARNING | MB_OK | MB_SYSTEMMODAL);

        delete[] tBuf;
    }

    va_end(vl);
}

void ErrorSystem::Logger::ThrowFatalError(const char *Error, const char *File,
                                          const char *Func,
                                          const uint32_t Line) {
    char *Buf = new char[BufSize];

#if defined(_DEBUG) || defined(VERBOSE_LOG)
    snprintf(Buf, SZBufSize,
             "A fatal error has occured in the \"%s\" function, from which the "
             "driver can NOT recover! File: %s - Line: %u - Error:%s",
             Func, File, Line, Error);
#else
    snprintf(Buf, BufSize,
             "An fatal error has occured in the \"%s\" function, from which "
             "the driver can NOT recover! Error: %s",
             Func, Error);
#endif

    logMutex.lock();
    std::cout << Buf << std::endl;
    MsgBox(NULL, Buf, "OmniMIDI - FATAL ERROR",
           MB_ICONERROR | MB_OK | MB_SYSTEMMODAL);
    logMutex.unlock();

    delete[] Buf;

#ifdef _DEBUG
    throw std::runtime_error(Error);
#else
    exit(-1);
#endif
}