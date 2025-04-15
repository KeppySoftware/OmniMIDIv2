/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#ifndef _EVBUF_T_H
#define _EVBUF_T_H

#pragma once

#define BEvBuf				BaseEvBuf_t
#define EvBuf				EvBuf_t
#define LEvBuf				LEvBuf_t
#define ShortEvent			uint32_t

#define DEF_EVBUF_SIZE		4096
#define MAX_EVBUF_SIZE		UINT_MAX / 8
#define MAX_LEVBUF_SIZE		64

#if !defined(_WIN64) && !defined(__x86_64__)
#define MAX_MIDIHDR_BUF		65536
#else
#define MAX_MIDIHDR_BUF		131072
#endif

#include "Common.hpp"

#include <atomic>
#ifdef _STATSDEV
#include <iostream>
#include <future>
#endif

namespace OmniMIDI {
	typedef struct AdvShortEvent {
		uint8_t status = 0;
		uint8_t param1 = 0;
		uint8_t param2 = 0;
	} AdvShortEv, *PAdvShortEv, ASE, *PASE;

	typedef struct LongEvent {
		uint8_t ev[MAX_MIDIHDR_BUF] = { 0 };
		size_t len = 0;
	} LongEv, *PLongEv, LE, *PLE;

	class BaseEvBuf_t {
	protected:
		size_t readHead = 0;
		size_t writeHead = 0;
		size_t size = 0;

	private:
		char longDummy = 'p';

	public:
		BaseEvBuf_t() { }
		BaseEvBuf_t(size_t size) { Allocate(size); }
		virtual ~BaseEvBuf_t() { }

		virtual bool Allocate(size_t ReqSize) { return true; }
		virtual bool Free() { return true; }

		// Short messages
		virtual void Write(uint32_t ev) { }
		virtual void Write(uint8_t status, uint8_t param1, uint8_t param2) { }

		virtual ShortEvent Read() { return 0; }
		virtual ShortEvent Peek() { return 0; }
		virtual ShortEvent* ReadPtr() { return nullptr; }
		virtual ShortEvent* PeekPtr() { return nullptr; }

		// Long messages
		virtual void Write(uint8_t* ev, size_t len) { }
		virtual void ReadLong(uint8_t* ev, size_t* len) { *ev = longDummy; *len = sizeof(longDummy); }
		virtual void PeekLong(uint8_t* ev, size_t* len) { *ev = longDummy; *len = sizeof(longDummy); }

		virtual bool NewEventsAvailable() { return false; }
		virtual void ResetHeads() { readHead = 0; writeHead = 0; }
		virtual size_t GetReadHeadPos() { return 0; }
		virtual size_t GetWriteHeadPos() { return 0; }
	};

	class LEvBuf_t : public BaseEvBuf_t {
	private:
		PLE buf = nullptr;

	public:
		LEvBuf_t() { }

		LEvBuf_t(size_t ReqSize) {
			Allocate(ReqSize);
		}

		~LEvBuf_t() {
			Free();
		}

		bool Allocate(size_t ReqSize) override {
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

		bool Free() override {
			if (!buf)
				return false;

			size = 0;
			readHead = 0;
			writeHead = 0;

			delete[] buf;
			buf = nullptr;

			return true;
		}

		void Write(uint8_t* ev, size_t len) override {
			auto tWriteHead = (writeHead + 1) % size;

			if (tWriteHead != readHead)
				writeHead = tWriteHead;

			memcpy(buf[writeHead].ev, ev, len);
			buf[writeHead].len = len;
		}

		void ReadLong(uint8_t* ev, size_t* len) override {
			if (readHead != writeHead)
			{
				if (++readHead >= size) readHead = 0;
				ev = buf[readHead].ev;
				*len = buf[readHead].len;
			}
		}

		void PeekLong(uint8_t* ev, size_t* len) override {
			auto tNextHead = (readHead + 1) % size;

			ev = buf[tNextHead].ev;
			*len = buf[tNextHead].len;
		}

		bool NewEventsAvailable() override {
			return (readHead != writeHead);
		}

		size_t GetReadHeadPos() override { return readHead; }
		size_t GetWriteHeadPos() override { return writeHead; }
	};

	class EvBuf_t : public BaseEvBuf_t {
	private:
		uint8_t runningStatus = 0;

#ifdef _STATSDEV
		FILE* dummy;
		size_t evSent = 0;
		size_t evSkipped = 0;
#endif

		ShortEvent* buf = nullptr;
		bool dontMiss = false;

		constexpr bool IsRunningStatus(uint32_t ev) { return ((ev & 0xFF) & 0x80) < 1; }
		constexpr uint32_t ApplyRunningStatus(uint32_t ev) {
			if (ev & 0x80) {
				runningStatus = ev & 0xFF;
				return ev;
			}

			return (ev << 8) | runningStatus;
		}
		constexpr uint8_t GetStatus(uint32_t ev) { return (ev & 0xFF) & 0x80; }
		constexpr uint8_t GetFirstParam(uint32_t ev) { return ((ev >> 8) & 0xFF); }
		constexpr uint8_t GetSecondParam(uint32_t ev) { return ((ev >> 16) & 0xFF); }

	public:
		EvBuf_t() { }

		EvBuf_t(size_t ReqSize) {
			Allocate(ReqSize);
		}

		~EvBuf_t() {
			Free();
		}

		void SetMissMode(bool val) {
			dontMiss = val;
		}

		bool Allocate(size_t ReqSize) override {
			if (buf)
				return false;
			
			size = ReqSize;

			if (size < 8)
				size = 8;

#if !defined(_WIN64) && !defined(__x86_64__)
			else if (size > MAX_EVBUF_SIZE)
				size = MAX_EVBUF_SIZE;
#endif

			buf = new ShortEvent[size]{};

			if (!buf)
				return false;

			return true;
		}

		bool Free() override {
			if (!buf)
				return false;

#ifdef _STATSDEV
			GetStats();
			evSent = 0;
			evSkipped = 0;
#endif
			readHead = 0;
			writeHead = 0;

			delete[] buf;
			buf = nullptr;

			size = 0;
			return true;
		}

		void Write(uint8_t status, uint8_t param1, uint8_t param2) override {
			Write(status | (param1 << 8) | (param2 << 16));
		}

		void Write(uint32_t ev) override {
			size_t curReadHead = readHead;
			size_t nextWriteHead = (writeHead + 1) % size;

			if (nextWriteHead != curReadHead) {
#ifdef _STATSDEV
				evSent++;
#endif				

				buf[nextWriteHead] = ev;
				writeHead = nextWriteHead;

				return;
			}

#ifdef _STATSDEV
			// Buffer full
			evSkipped++;
#endif
		}

		ShortEvent* ReadPtr() override {
			if (readHead == writeHead) 
				return nullptr;
				
			readHead = (readHead + 1) % size;
			return &buf[readHead];
		}

		ShortEvent Read() override {
			auto val = ReadPtr();

			if (val == nullptr)
				return 0;

			return *val;
		}

		ShortEvent* PeekPtr() override {
			auto tNextHead = (readHead + 1) % size;
			return &buf[tNextHead];
		}

		ShortEvent Peek() override {
			auto val = PeekPtr();

			if (val == nullptr)
				return 0;

			return *val;
		}

		bool NewEventsAvailable() override {
			return readHead != writeHead;
		}

		size_t GetReadHeadPos() override { return readHead; }
		size_t GetWriteHeadPos() override { return writeHead; }
	};
}

#endif