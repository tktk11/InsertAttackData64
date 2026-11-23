#include "../skse64/PluginAPI.h"
#include "../skse64_common/skse_version.h"
#include "../skse64_common/Relocation.h"
#include "../skse64_common/SafeWrite.h"
#include "../skse64/GameForms.h"
#include "../skse64/GameData.h"

#include <shlobj.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <map>
#include <filesystem>

#include "picojson\picojson.h"
#include "libskyrim\BGSAttackDataMap.h"

#ifdef _DEBUG
#include "DebugUtil.h"
#endif

// truncated name warning
#pragma warning(disable: 4503)

typedef std::vector<std::pair<std::string, picojson::value>> attackdata;
typedef std::map<std::string, attackdata> event_map;
typedef std::map<std::string, event_map> setting_map;

static setting_map s_setting;


inline static void Chomp(std::string & str, int c = '\r')
{
	std::size_t len = str.length();
	if (len > 0 && str[len - 1] == c)
	{
		str.resize(len - 1);
	}
}


inline static void ToLower(std::string & str)
{
	for (auto &n : str)
	{
		n = tolower(n);
	}
}


static std::string ToString(UInt32 id)
{
	DataHandler *dhnd = DataHandler::GetSingleton();
	UInt8 modIndex = id >> 24;
	if (modIndex >= dhnd->modList.loadedMods.count)
		return std::string("XXXXXX");

	std::stringstream ss;

	if (modIndex) {
		std::string modName = dhnd->modList.loadedMods[modIndex]->name;
		ToLower(modName);
		ss << modName << " ";
	}

	for (UInt32 i = 0; i < dhnd->modList.loadedMods.count; ++i) {
	}

	ss << std::hex << std::setfill('0') << std::setw(6) << (id & 0x00FFFFFF);
	return ss.str();
}


static bool ParseJson(std::string jsonFileName, picojson::value &val)
{
	std::stringstream ss;
	std::ifstream fileStream(jsonFileName);

	if (!fileStream)
	{
		_MESSAGE("**** error **** cannot read JSON file: %s", jsonFileName.c_str());
		return false;
	}

	ss << fileStream.rdbuf();

	std::string err = picojson::parse(val, ss);
	if (!err.empty())
	{
		_MESSAGE("**** error **** invalid JSON in %s\n  %s", jsonFileName, err.c_str());
		return false;
	}
	if (!val.is<picojson::object>())
	{
		_MESSAGE("**** error **** invalid top-level type in %s", jsonFileName);
		return false;
	}

	return true;
}


static void RemapJson(picojson::value &json, setting_map &setting)
{
	if (!json.is<picojson::object>())
		return;

	for (auto &kv : json.get<picojson::object>())
	{
		picojson::value &v = kv.second;
		if (!v.is<picojson::object>())
			continue;

		std::string key = kv.first;
		if (key.empty())
			continue;
		ToLower(key);

		picojson::object &events_in = v.get<picojson::object>();
		event_map &events_out = setting[key];

		for (auto &event : events_in)
		{
			picojson::value &value = event.second;
			if (!value.is<picojson::object>())
				continue;

			std::string eventName = event.first;
			if (eventName.empty())
				continue;

			picojson::object &attackdata_in = value.get<picojson::object>();
			attackdata &attackdata_out = events_out[eventName];
			attackdata_out.reserve(attackdata_in.size());

			for (auto &data : attackdata_in)
			{
				std::string name = data.first;
				if (name.empty())
					continue;
				ToLower(name);

				attackdata_out.push_back(std::make_pair(name, data.second));
			}
		}
	}
}


static void SetAttackData(BGSAttackData* attackData, attackdata &data)
{
	for (auto &val : data)
	{
		std::string &valName = val.first;

		_MESSAGE("    %s = %s", valName.c_str(), val.second.serialize().c_str());

		if (valName == "damagemult") {
			attackData->damageMult = val.second.get<double>();
		}
		else if (valName == "attackchance") {
			attackData->attackChance = val.second.get<double>();
		}
		else if (valName == "attackangle") {
			attackData->attackAngle = val.second.get<double>();
		}
		else if (valName == "strikeangle") {
			attackData->strikeAngle = val.second.get<double>();
		}
		else if (valName == "stagger") {
			attackData->stagger = val.second.get<double>();
		}
		else if (valName == "knockdown") {
			attackData->knockdown = val.second.get<double>();
		}
		else if (valName == "recoverytime") {
			attackData->recoveryTime = val.second.get<double>();
		}
		else if (valName == "staminamult") {
			attackData->staminaMult = val.second.get<double>();
		}
		else if (valName == "ignoreweapon") {
			attackData->flags.ignoreWeapon = val.second.get<bool>();
		}
		else if (valName == "bashattack") {
			attackData->flags.bashAttack = val.second.get<bool>();
		}
		else if (valName == "powerattack") {
			attackData->flags.powerAttack = val.second.get<bool>();
		}
		else if (valName == "leftattack") {
			attackData->flags.leftAttack = val.second.get<bool>();
		}
		else if (valName == "rotatingattack") {
			attackData->flags.rotatingAttack = val.second.get<bool>();
		}
		else {
			_MESSAGE("**** warning **** unknown record name \" %s \"", valName);
		}
	}
}


static void InsertAttackDataRecords(TESRace* race, event_map &events)
{
	for (auto &ev : events)
	{
		BSFixedString eventName(ev.first.c_str());

		BGSAttackDataMap * adm = (BGSAttackDataMap *)race->attackData.unk08.ptr;

		BGSAttackData *attackData = adm->Get(eventName);

		if (!attackData)
		{
			_MESSAGE("(NEW) %s::%s", race->editorId.c_str(), ev.first.c_str());
			attackData = adm->Add(eventName);
		}
		else // being lazy here
		{
			_MESSAGE("%s::%s", race->editorId.c_str(), ev.first.c_str());
		}

		if (attackData)
		{
			SetAttackData(attackData, ev.second);
		}
	}
}

static void InitAttackDataJson()
{
	std::string baseDir("Meshes\\AttackData");

	// read json files (non-recursive, same as the std::experimental::filesystem version)
	namespace fs = std::filesystem;
	const fs::path dir(std::string("Data\\") + baseDir);

	std::error_code ec;
	fs::directory_iterator it(dir, ec);
	if (ec)
	{
		_MESSAGE("**** warning **** cannot open directory %s: %s", dir.string().c_str(), ec.message().c_str());
		return;
	}

	for (const fs::directory_entry& entry : it)
	{
		std::error_code fileEc;
		if (!entry.is_regular_file(fileEc))
			continue;

		const fs::path& p = entry.path();
		std::string ext = p.extension().string();
		if (_stricmp(ext.c_str(), ".json") != 0)
			continue;

		std::string fileName = "Data\\" + baseDir + "\\" + p.filename().string();

		_MESSAGE("found file %s", fileName.c_str());
		// read json
		picojson::value val;
		if (ParseJson(fileName, val))
		{
			RemapJson(val, s_setting);
		}
	}
}

// Per-runtime address table (RVAs, resolved with RelocAddr at use time), see libskyrim/GameAddresses.h.
// One DLL is built per SKSE SDK and only uses the table of its SDK's runtime (CURRENT_RELEASE_RUNTIME):
//   SKSE 2.0.20 -> 1.5.97    SKSE 2.2.6 -> 1.6.1170    SKSE 2.3.1 -> 1.7.104
//   1.5.73  : original SE addresses, migration source only
//   1.5.97 / 1.6.1170 / 1.7.104 : addresses + layouts verified (C:\dev\xse\re\verify.py)
//
// Address evidence (Steamless-unpacked exes, compared against 1.5.73):
//   TESRace::LoadForm / vtable[6]: RTTI COL of .?AVTESRace@@ at offset 0 -> primary vtable slot[6]
//                                  (same prologue in all versions; frame 0x400 SE / 0x460 AE)
//   BGSAttackData ctor           : the only code referencing ??_7BGSAttackData@@6B@; body is
//                                  instruction-identical to 1.5.73 (same field init 0x18..0x48),
//                                  callers allocate 0x50 bytes
//   CRC32_Size / _32 / _64       : table-driven CRC routines, same code shape as 1.5.73
// Layout evidence (1.5.97, 1.6.1170, 1.7.104):
//   TESRace base classes at the same offsets (TESFullName 0x20 ... BGSAttackDataForm 0x88),
//   editorId 0x2E0 (GetFormEditorID), behaviorGraph 0x1E8 (ctor), BGSAttackDataMap ctor sets the
//   same hash table fields (0x1C/0x24/0x28/0x38/0x40), DataHandler files array at 0xD70 / count
//   0xD80 (game's GetFile-by-compile-index is byte-identical).
using IAD::AddressTable;

const AddressTable* IAD::g_addresses = nullptr;

static constexpr AddressTable k1_5_73 = {
	"1.5.73",
	0x00384BC0, 0x015A30E8,   // TESRace::LoadForm, vtable[6]
	0x003E7C20,               // BGSAttackData ctor
	0x00C06490, 0x00C064F0, 0x00C06570, // CRC32 Size / 32 / 64
};

// 1.5.97: same code addresses as 1.5.73, .rdata vtable moved by -0x10
static constexpr AddressTable k1_5_97 = {
	"1.5.97",
	0x00384BC0, 0x015A30D8,   // TESRace::LoadForm, vtable[6]
	0x003E7C20,               // BGSAttackData ctor
	0x00C06490, 0x00C064F0, 0x00C06570, // CRC32 Size / 32 / 64
};

static constexpr AddressTable k1_6_1170 = {
	"1.6.1170",
	0x003DCCD0, 0x017E7EA8,   // TESRace::LoadForm, vtable[6]
	0x004428E0,               // BGSAttackData ctor
	0x00CC8380, 0x00CC83E0, 0x00CC8460, // CRC32 Size / 32 / 64
};

static constexpr AddressTable k1_7_104 = {
	"1.7.104",
	0x003E3DA0, 0x01861D98,   // TESRace::LoadForm, vtable[6]
	0x00449D80,               // BGSAttackData ctor
	0x00CE2510, 0x00CE2570, 0x00CE25F0, // CRC32 Size / 32 / 64
};

// The table of the runtime this build's SKSE SDK targets (older SDKs lack the newer macros).
#if defined(RUNTIME_VERSION_1_7_104) && CURRENT_RELEASE_RUNTIME == RUNTIME_VERSION_1_7_104
static constexpr const AddressTable* kBuildTable = &k1_7_104;
#elif defined(RUNTIME_VERSION_1_6_1170) && CURRENT_RELEASE_RUNTIME == RUNTIME_VERSION_1_6_1170
static constexpr const AddressTable* kBuildTable = &k1_6_1170;
#elif defined(RUNTIME_VERSION_1_5_97) && CURRENT_RELEASE_RUNTIME == RUNTIME_VERSION_1_5_97
static constexpr const AddressTable* kBuildTable = &k1_5_97;
#else
static constexpr const AddressTable* kBuildTable = nullptr;
#endif

static const AddressTable* SelectAddressTable(UInt32 runtimeVersion)
{
	return runtimeVersion == CURRENT_RELEASE_RUNTIME ? kBuildTable : nullptr;
}

typedef bool(*_TESRaceLoadForm)(TESRace * race, int64_t unk1);
static _TESRaceLoadForm TESRaceOriginalLoadForm = nullptr;

bool HookedLoadForm(TESRace * race, int64_t unk1)
{
#ifdef _DEBUG
	_DMESSAGE("HookedLoadForm(0x%016" PRIXPTR ", %d) called", (uintptr_t) race, unk1);
#endif
	bool result = TESRaceOriginalLoadForm(race, unk1);

	if (result)
	{
		_MESSAGE("successfully loaded race with formid %08X and name %s", race->formID, race->fullName.GetName());
	}

	if (!race->attackData.unk08.ptr)
	{
		_MESSAGE("race has no attack data, returning");
		return result;
	}

	BGSAttackDataMap * adm = (BGSAttackDataMap *)race->attackData.unk08.ptr;

#ifdef _DEBUG
	PrintADMDebugInfo(adm);
#endif

	std::string keys[3];

	keys[0] = race->behaviorGraph[0].GetModelName();
	if (race->editorId)
	{
#ifdef _DEBUG
		_DMESSAGE("race has editor id %s", race->editorId);
#endif
		keys[1] = race->editorId;
	}
	keys[2] = ToString(race->formID);

	for (std::string &key : keys)
	{
		if (key.empty())
			continue;

		ToLower(key);

		auto it = s_setting.find(key);
		if (it != s_setting.end())
		{
			InsertAttackDataRecords(race, it->second);
		}
	}
	
#ifdef _DEBUG
	PrintADMDebugInfo(adm);
#endif

	return result;
}



// SKSE 2.0.x (SE) uses SKSEPlugin_Query + SKSEPlugin_Load, SKSE 2.1+ (AE) SKSEPlugin_Version + SKSEPlugin_Load.
#if SKSE_VERSION_INTEGER > 2 || (SKSE_VERSION_INTEGER == 2 && SKSE_VERSION_INTEGER_MINOR >= 1)
#define IAD_AE_SDK 1
#else
#define IAD_AE_SDK 0
#endif

// Log and compatibility checks shared by the SE (Query) and AE (Load) entry points.
static bool CheckRuntime(const SKSEInterface * skse)
{
	gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim Special Edition\\SKSE\\InsertAttackData64.log");
	gLog.SetPrintLevel(IDebugLog::kLevel_Error);
	gLog.SetLogLevel(IDebugLog::kLevel_DebugMessage);

	_MESSAGE("InsertAttackData64 Plugin");
	_MESSAGE("runtime %08X, SKSE %08X, built for runtime %08X", skse->runtimeVersion, skse->skseVersion, CURRENT_RELEASE_RUNTIME);

	if (skse->isEditor)
	{
		_MESSAGE("loaded in editor, marking as incompatible");
		return false;
	}

#if !IAD_AE_SDK
	// SE SKSE has no version data; reject other runtimes here (AE SKSE does it from SKSEPlugin_Version).
	if (skse->runtimeVersion != CURRENT_RELEASE_RUNTIME)
	{
		_MESSAGE("unsupported runtime version %08X", skse->runtimeVersion);
		return false;
	}
#endif
	return true;
}

// Installs the TESRace::LoadForm hook and reads the JSON files (run from SKSEPlugin_Load, as before).
static void Install(const SKSEInterface * skse)
{
	const AddressTable* addresses = SelectAddressTable(skse->runtimeVersion);
	IAD::g_addresses = addresses;
	if (addresses)
	{
		_MESSAGE("Address table: %s", addresses->runtimeName);
		TESRaceOriginalLoadForm = RelocAddr<_TESRaceLoadForm>(addresses->TESRace_LoadForm);

		_MESSAGE("Hooking TESRace::LoadForm");
		SafeWrite64(RelocAddr<uintptr_t>(addresses->TESRace_vtbl_LoadForm).GetUIntPtr(), uintptr_t(HookedLoadForm));
		_MESSAGE("Patched");
	}
	else
	{
		// Never touch game memory on a runtime this build was not made for.
		_MESSAGE("Address table: none for runtime %08X (not this build's runtime), TESRace::LoadForm hook disabled", skse->runtimeVersion);
	}

	_MESSAGE("Reading attack data json");
	InitAttackDataJson();
	_MESSAGE("Done");
}

extern "C" {
#if IAD_AE_SDK
	// AE SKSE reads this data without running any plugin code to decide whether to load the DLL.
	__declspec(dllexport) SKSEPluginVersionData SKSEPlugin_Version =
	{
		SKSEPluginVersionData::kVersion,

		1,
		"InsertAttackData64 plugin",

		"",
		"",

		0,	// not version independent (extended field)
		0,	// not version independent: hardcoded addresses and game structure layouts
		{ CURRENT_RELEASE_RUNTIME, 0 },	// the runtime of the SKSE SDK this DLL is built with

		0,	// works with any version of the script extender
	};

	__declspec(dllexport) bool SKSEPlugin_Load(const SKSEInterface * skse) {
		if (!CheckRuntime(skse))
			return false;
		Install(skse);
		return true;
	}
#else
	__declspec(dllexport) bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info)
	{
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "InsertAttackData64 plugin";
		info->version = 1;

		return CheckRuntime(skse);
	}

	__declspec(dllexport) bool SKSEPlugin_Load(const SKSEInterface * skse) {
		Install(skse);
		return true;
	}
#endif
};

