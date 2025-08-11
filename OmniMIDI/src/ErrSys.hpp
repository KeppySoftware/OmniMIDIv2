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

#ifndef _ERRSYS_H
#define _ERRSYS_H

#include <mutex>

#if !defined(_MSC_VER)
#define __func__ __FUNCTION__
#endif

#if defined(_WIN32)
#include <windows.h>

#include <psapi.h>
#include <tchar.h>
#else
#include <stdarg.h>
#endif

#if defined(_WIN32)
#define MsgBox MessageBoxA
#else
#define MsgBox(...)
#define MB_ICONWARNING 0
#define MB_ICONERROR 0
#define MB_SYSTEMMODAL 0
#define MB_OK 0
#endif

#if defined(_DEBUG) || defined(VERBOSE_LOG)
#include <stdexcept>
#define __SRCFILE __FILE__
#define __FUNCNAME __func__
#define __LINENUMBER __LINE__
#define dbgprintf                                                              \
    snprintf(cBuf, SZBufSize, "[%s -> %s, L%u] >> %s", Func, File, Line, tBuf);
#else
#define __SRCFILE nullptr
#define __FUNCNAME nullptr
#define __LINENUMBER 0
#define dbgprintf snprintf(cBuf, SZBufSize, "[] >> %s", tBuf);
#endif

#define Error(text, fatal, ...)                                                \
    if (ErrLog)                                                                \
    ErrLog->ThrowError(text, fatal, __SRCFILE, __FUNCNAME, __LINENUMBER,       \
                       ##__VA_ARGS__)

#define Fatal(text)                                                            \
    if (ErrLog)                                                                \
    ErrLog->ThrowFatalError(text, __SRCFILE, __FUNCNAME, __LINENUMBER)

#define Message(text, ...)                                                     \
    if (ErrLog)                                                                \
    ErrLog->Log(text, __SRCFILE, __FUNCNAME, __LINENUMBER, ##__VA_ARGS__)

namespace ErrorSystem {
class Logger {
  private:
    static const int32_t BufSize = 2048;
    static const int32_t SZBufSize = sizeof(char) * BufSize;
    static std::mutex logMutex;

  public:
    void ShowError(const char *Message, const char *Title, bool IsSeriousError);
    void Log(const char *Message, const char *File, const char *Func,
             const uint32_t Line, ...);
    void ThrowError(const char *Error, bool IsSeriousError, const char *File,
                    const char *Func, const uint32_t Line, ...);
    void ThrowFatalError(const char *Error, const char *File, const char *Func,
                         const uint32_t Line);
};
} // namespace ErrorSystem

#endif