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
	typedef struct {
		unsigned char status;
		unsigned char param1;
		unsigned char param2;
	} ShortEvent, ShortEv, * PShortEv, SE, * PSE;

	typedef struct {
		char ev[MAX_MIDIHDR_BUF];
		int len;
	} LongEvent, LongEv, * PLongEv, LE, * PLE;

	class BaseEvBuf_t {
	protected:
		std::atomic<size_t> readHead = 0;
		std::atomic<size_t> writeHead = 0;

		SE shortDummyStruct = { .status = 0xFF, .param1 = 0xFF, .param2 = 0xFF };
		unsigned int shortDummy = 0xFFFFFF;
		char longDummy = 'C';

	public:
		virtual ~BaseEvBuf_t() { }

		bool Allocate(size_t ReqSize) { return true; }
		bool Free() { return true; }
		constexpr unsigned int CreateDword(PSE shortEv) { return shortEv->status | shortEv->param1 << 8 | shortEv->param2 << 16; }

		// Short messages
		virtual void Push(unsigned int ev) { }
		virtual void Push(unsigned char status, unsigned char param1, unsigned char param2) { }
		virtual unsigned int Pop() { return shortDummy; }
		virtual unsigned int Peek() { return shortDummy; }

		virtual PSE PopItem() { return &shortDummyStruct; }
		virtual PSE PeekItem() { return &shortDummyStruct; }

		// Long messages
		virtual void Push(char* ev, size_t len) { }
		virtual void Pop(char* ev, size_t* len) { *ev = longDummy; *len = sizeof(longDummy); }
		virtual void Peek(char* ev, size_t* len) { *ev = longDummy; *len = sizeof(longDummy); }

		virtual bool NewEventsAvailable() { return false; }
		virtual size_t GetReadHeadPos() { return 0; }
		virtual size_t GetWriteHeadPos() { return 0; }
	};

	class LEvBuf_t : public BaseEvBuf_t {
	private:
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
		unsigned char lastRunningStatus = 0;

#ifdef _STATSDEV
		FILE* dummy;
		size_t evSent = 0;
		size_t evSkipped = 0;
#endif

		size_t size = 0;
		PSE* buf = nullptr;

		constexpr unsigned char ApplyLastRunningStatus(unsigned char status) {
			unsigned char tStatus = status;

			if ((tStatus & 0x80) != 0) lastRunningStatus = tStatus;
			else return tStatus;

			return status;
		}
		constexpr unsigned int GetStatus(unsigned int ev) { return (ev & 0xFF); }
		constexpr unsigned int GetFirstParam(unsigned int ev) { return ((ev >> 8) & 0xFF); }
		constexpr unsigned int GetSecondParam(unsigned int ev) { return ((ev >> 16) & 0xFF); }

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

			buf = new PSE[ReqSize]();
			for (size_t i = 0; i < size; i++)
				buf[i] = new SE();

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
			readHead = 0;
			writeHead = 0;

			for (size_t i = 0; i < size; i++)
				delete buf[i];

			delete[] buf;
			buf = nullptr;

			size = 0;
			return true;
		}

		void Push(unsigned char status, unsigned char param1, unsigned char param2) {
			auto tReadHead = (size_t)readHead;
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

			buf[tWriteHead]->status = ApplyLastRunningStatus(status);
			buf[tWriteHead]->param1 = param1;
			buf[tWriteHead]->param2 = param2;
			writeHead = tWriteHead;
		}

		void Push(unsigned int ev) {
			Push(GetStatus(ev), GetFirstParam(ev), GetSecondParam(ev));
		}

		PSE PopItem() {
			auto tWriteHead = (size_t)writeHead;

			if (readHead == tWriteHead)
				return nullptr;

			auto tNextHead = readHead + 1;
			if (tNextHead >= size)
				tNextHead = 0;

			readHead = tNextHead;
			return buf[tNextHead];
		}

		unsigned int Pop() {
			PSE t = PopItem();
			return t ? CreateDword(t) : 0;
		}

		PSE PeekItem() {
			auto tNextHead = readHead + 1;

			if (tNextHead >= size)
				tNextHead = 0;

			return buf[tNextHead];
		}

		unsigned int Peek() {
			PSE t = PeekItem();
			return t ? CreateDword(t) : 0;
		}

		bool NewEventsAvailable() {
			return (readHead != writeHead);
		}

		size_t GetReadHeadPos() { return readHead; }
		size_t GetWriteHeadPos() { return writeHead; }
	};
}

#endif