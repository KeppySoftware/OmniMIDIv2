/*

	OmniMIDI v15+ (Rewrite)

	This file contains the required code to run the driver under any OS.

*/

#ifndef _EVBUF_T_H
#define _EVBUF_T_H

#pragma once

#define BEvBuf				BaseEvBuf_t
#define EvBuf				EvBuf_t
#define LEvBuf				LEvBuf_t

#define MAX_EVBUF_SIZE		UINT_MAX / 2
#define MAX_LEVBUF_SIZE		64

#if !defined(_WIN64) && !defined(__x86_64__)
#define MAX_MIDIHDR_BUF		65536
#else
#define MAX_MIDIHDR_BUF		131072
#endif

#ifdef _STATSDEV
#include <iostream>
#include <future>
#endif

namespace OmniMIDI {
	class BaseEvBuf_t {
		unsigned int shortDummy = 0xFFFFFF;
		char longDummy = 'C';

	public:
		virtual ~BaseEvBuf_t() { }

		bool Allocate(size_t ReqSize) { return true; }
		bool Free() { return true; }

		// Short messages
		virtual void Push(unsigned int ev) { }
		virtual void Pop(unsigned int* ev) { *ev = shortDummy; }
		virtual void Peek(unsigned int* ev) { *ev = shortDummy; }

		// Long messages
		virtual void Push(char* ev, size_t len) { }
		virtual void Pop(char* ev, size_t* len) { *ev = longDummy; *len = 1; }
		virtual void Peek(char* ev, size_t* len) { *ev = longDummy; *len = 1; }

		virtual bool NewEventsAvailable() { return false; }
		virtual size_t GetReadHeadPos() { return 0; }
		virtual size_t GetWriteHeadPos() { return 0; }
	};

	class LEvBuf_t : public BaseEvBuf_t {
		typedef struct {
			char ev[MAX_MIDIHDR_BUF];
			int len;
		} LongEvent, LongEv, * PLongEv, LE, * PLE;

	private:
		std::atomic<size_t> readHead = 0;
		std::atomic<size_t> writeHead = 0;

		size_t size = 0;
		PLE buf = nullptr;

	public:
		LEvBuf_t() { }

		LEvBuf_t(size_t ReqSize) {
			Allocate(ReqSize);
		}

		~LEvBuf_t() {
			Free();
		}

		bool Allocate(size_t ReqSize) {
			if (buf)
				return false;

			size = ReqSize;

			if (size < 2)
				size = 2;
			else if (size > MAX_LEVBUF_SIZE)
				size = MAX_LEVBUF_SIZE;

			buf = new LE[size];
			return true;
		}

		bool Free() {
			if (!buf)
				return false;

			size = 0;
			readHead = 0;
			writeHead = 0;

			delete[] buf;
			buf = nullptr;

			return true;
		}

		void Push(char* ev, size_t len) {
			auto tWriteHead = writeHead + 1;
			if (tWriteHead >= size)
				tWriteHead = 0;

			if (tWriteHead != readHead)
				writeHead = tWriteHead;

			memcpy(buf[writeHead].ev, ev, len);
			buf[writeHead].len = len;
		}

		void Pop(char* ev, size_t* len) {
			if (readHead != writeHead)
			{
				if (++readHead >= size) readHead = 0;
				ev = buf[readHead].ev;
				*len = buf[readHead].len;
			}
		}

		void Peek(char* ev, size_t* len) {
			auto tNextHead = readHead + 1;

			if (tNextHead >= size)
				tNextHead = 0;

			ev = buf[readHead].ev;
			*len = buf[readHead].len;
		}

		bool NewEventsAvailable() {
			return (readHead != writeHead);
		}

		size_t GetReadHeadPos() { return readHead; }
		size_t GetWriteHeadPos() { return writeHead; }
	};

	class EvBuf_t : public BaseEvBuf_t {
	private:
		std::atomic<size_t> readHead = 0;
		std::atomic<size_t> writeHead = 0;

#ifdef _STATSDEV
		FILE* dummy;
		size_t evSent = 0;
		size_t evSkipped = 0;
#endif

		size_t size = 0;
		unsigned int* buf = nullptr;

	public:
		EvBuf_t() { }

		EvBuf_t(size_t ReqSize) {
			Allocate(ReqSize);
		}

		~EvBuf_t() {
			Free();
		}

		bool Allocate(size_t ReqSize) {		
			if (buf)
				return false;
			
			size = ReqSize;

			if (size < 8)
				size = 8;

#if !defined(_WIN64) && !defined(__x86_64__)
			else if (size > MAX_EVBUF_SIZE)
				size = MAX_EVBUF_SIZE;
#endif

			buf = new unsigned int[size];
			return true;
		}

		bool Free() {
			if (!buf)
				return false;

#ifdef _STATSDEV
			GetStats();
			evSent = 0;
			evSkipped = 0;
#endif

			size = 0;
			readHead = 0;
			writeHead = 0;

			delete[] buf;
			buf = nullptr;

			return true;
		}

		void Push(unsigned int ev) {
#ifndef EVBUF_OLD
			auto tWriteHead = writeHead + 1;
			if (tWriteHead >= size)
				tWriteHead = 0;

			if (tWriteHead != readHead)
				writeHead = tWriteHead;

			buf[writeHead] = ev;
#else
			auto tReadHead = readHead;
			auto tWriteHead = writeHead + 1;

			if (tWriteHead >= size)
				tWriteHead = 0;

#ifdef _STATSDEV
			evSent++;
#endif

			if (tWriteHead == tReadHead) {
#ifdef _STATSDEV
				evSkipped++;
#endif
				return;
			}

			buf[writeHead] = ev;
			writeHead = tWriteHead;
#endif
		}

		void Pop(unsigned int* ev) {
#ifndef EVBUF_OLD
			if (readHead != writeHead)
			{
				if (++readHead >= size) readHead = 0;
				*ev = buf[readHead];			
			}
#else
			auto tWriteHead = writeHead;

			if (readHead == tWriteHead)
				return;

			size_t tNextHead = readHead + 1;
			if (tNextHead >= size)
				tNextHead = 0;

			*ev = buf[readHead];
			readHead = tNextHead;
#endif
		}

		void Peek(unsigned int* ev) {
			auto tNextHead = readHead + 1;

			if (tNextHead >= size)
				tNextHead = 0;

			*ev = buf[tNextHead];
		}

		bool NewEventsAvailable() {
			return (readHead != writeHead);
		}

		size_t GetReadHeadPos() { return readHead; }
		size_t GetWriteHeadPos() { return writeHead; }
	};
}

#endif