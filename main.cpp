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

// Per-runtime address table (RVAs, resolved with RelocAddr when the hook is installed).
//   1.5.73  : original SE addresses, migration source only (AE SKSE never loads this plugin on 1.5.73)
//   1.6.1170: not migrated yet (phase 2)
//   1.7.104 : not migrated yet (phase 2)
// Only add a runtime to SelectAddressTable() after phase 2 (addresses) AND phase 3 (TESRace /
// BGSAttackDataMap layout) are verified for it: the hook writes into game structures.
struct AddressTable
{
	const char* runtimeName;
	uintptr_t TESRace_LoadForm;       // TESRace::LoadForm
	uintptr_t TESRace_vtbl_LoadForm;  // ??_7TESRace@@6B@ vtable[6]
};

static constexpr AddressTable k1_5_73 = { "1.5.73", 0x00384BC0, 0x015A30E8 };

static const AddressTable* SelectAddressTable(UInt32 runtimeVersion)
{
	switch (runtimeVersion)
	{
	// case RUNTIME_VERSION_1_7_104:  return &k1_7_104;   // phase 2/3
	// case RUNTIME_VERSION_1_6_1170: return &k1_6_1170;  // phase 2/3
	default:
		return nullptr;
	}
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



extern "C" {
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
		{ RUNTIME_VERSION_1_7_104, RUNTIME_VERSION_1_6_1170, 0 },

		0,	// works with any version of the script extender
	};

	bool SKSEPlugin_Load(const SKSEInterface * skse) {
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim Special Edition\\SKSE\\InsertAttackData64.log");
		gLog.SetPrintLevel(IDebugLog::kLevel_Error);
		gLog.SetLogLevel(IDebugLog::kLevel_DebugMessage);

		_MESSAGE("InsertAttackData64 Plugin");
		_MESSAGE("runtime %08X, SKSE %08X", skse->runtimeVersion, skse->skseVersion);

		if (skse->isEditor)
		{
			_MESSAGE("loaded in editor, marking as incompatible");
			return false;
		}

		const AddressTable* addresses = SelectAddressTable(skse->runtimeVersion);
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
			// Phase 1 of the AE migration: load, but never touch game memory until this runtime is verified.
			_MESSAGE("Address table: none for runtime %08X (not migrated yet), TESRace::LoadForm hook disabled", skse->runtimeVersion);
		}

		_MESSAGE("Reading attack data json");
		InitAttackDataJson();
		_MESSAGE("Done");
		return true;
	}
};

