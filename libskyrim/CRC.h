#pragma once

#include "../skse64_common/Relocation.h"
#include "GameAddresses.h"

// The CRC32 helpers live in the game executable; their addresses come from the runtime table
// (IAD::g_addresses). Only called from the TESRace::LoadForm hook, which is installed only when
// a table is selected.

template <class Ty>
class CRC32Calculator
{
public:
	inline CRC32Calculator() {}
	inline CRC32Calculator(const Ty &val) {
		operator=(val);
	}

	inline operator UInt32() {
		return m_checksum;
	}

protected:
	template <std::size_t SIZE = sizeof(Ty)>
	inline CRC32Calculator & operator=(const Ty &val) {
		typedef void(*Fn)(UInt32 *, const void *, UInt32);
		RelocAddr<Fn> fn(IAD::g_addresses->CRC32_Size); // CalculateCRC32_Size

		fn(&m_checksum, &val, SIZE); 
		return *this;
	}

	template <>
	inline CRC32Calculator & operator=<4>(const Ty &val) {
		typedef void(*Fn)(UInt32 *, Ty);
		RelocAddr<Fn> fn(IAD::g_addresses->CRC32_32); // CalculateCRC32_32

		fn(&m_checksum, val);
		return *this;
	}

	template <>
	inline CRC32Calculator & operator=<8>(const Ty &val) {
		typedef void(*Fn)(UInt32 *, Ty);
		RelocAddr<Fn> fn(IAD::g_addresses->CRC32_64); // CalculateCRC32_64

		fn(&m_checksum, val);
		return *this;
	}

	UInt32	m_checksum;
};

template <class Ty>
inline UInt32 CalcCRC32(const Ty &val) {
	CRC32Calculator<Ty> crc(val);
	return crc;
}
