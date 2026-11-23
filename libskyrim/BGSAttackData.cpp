#include "BGSAttackData.h"
#include "GameAddresses.h"
#include "../skse64_common/Relocation.h"


BGSAttackData * BGSAttackData::Create()
{
	BGSAttackData *attackData = (BGSAttackData *) Heap_Allocate(sizeof(BGSAttackData));
	if (attackData)
	{
		// BGSAttackData::BGSAttackData(); `this` in rcx, same as a free function taking the pointer.
		typedef BGSAttackData * (*_Ctor)(BGSAttackData * self);
		RelocAddr<_Ctor> ctor(IAD::g_addresses->BGSAttackData_ctor);
		ctor(attackData);
	}

	return attackData;
}