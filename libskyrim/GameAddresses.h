#pragma once

#include <cstdint>

// Runtime-selected game addresses (RVAs) used by InsertAttackData64 and its bundled libskyrim.
// Resolved with RelocAddr at call time. g_addresses is set in SKSEPlugin_Load and stays nullptr on
// runtimes that are not enabled; nothing that uses it runs in that case (the LoadForm hook is not
// installed).
namespace IAD
{
	struct AddressTable
	{
		const char* runtimeName;

		std::uintptr_t TESRace_LoadForm;       // TESRace::LoadForm
		std::uintptr_t TESRace_vtbl_LoadForm;  // ??_7TESRace@@6B@ vtable[6]

		std::uintptr_t BGSAttackData_ctor;     // BGSAttackData::BGSAttackData()
		std::uintptr_t CRC32_Size;             // CalculateCRC32_Size(UInt32*, const void*, UInt32)
		std::uintptr_t CRC32_32;               // CalculateCRC32_32(UInt32*, UInt32)
		std::uintptr_t CRC32_64;               // CalculateCRC32_64(UInt32*, UInt64)
	};

	extern const AddressTable* g_addresses;
}
