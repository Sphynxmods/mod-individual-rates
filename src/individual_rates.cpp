#include "ScriptMgr.h"
#include "Chat.h"
#include "Configuration/Config.h"
#include "DataMap.h"
#include "DBCStructure.h"
#include "DatabaseEnv.h"
#include "LootMgr.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellMgr.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Tokenize.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace Acore::ChatCommands;

enum IndividualRate : uint8
{
    IR_XP_KILL,
    IR_XP_BG_KILL_AV,
    IR_XP_BG_KILL_WSG,
    IR_XP_BG_KILL_AB,
    IR_XP_BG_KILL_EOTS,
    IR_XP_BG_KILL_SOTA,
    IR_XP_BG_KILL_IC,
    IR_XP_QUEST,
    IR_XP_QUEST_DF,
    IR_XP_EXPLORE,
    IR_XP_BATTLEGROUND_BONUS,
    IR_XP_LEVEL_1_9,
    IR_XP_LEVEL_10_19,
    IR_XP_LEVEL_20_29,
    IR_XP_LEVEL_30_39,
    IR_XP_LEVEL_40_49,
    IR_XP_LEVEL_50_59,
    IR_XP_LEVEL_60_69,
    IR_XP_LEVEL_70_79,
    IR_XP_LEVEL_80,
    IR_REPUTATION_GLOBAL,
    IR_REPUTATION_KILL,
    IR_REPUTATION_QUEST,
    IR_REPUTATION_OTHER,
    IR_HONOR,
    IR_ARENA_POINTS,
    IR_SKILL_CRAFTING,
    IR_SKILL_DEFENSE,
    IR_SKILL_GATHERING,
    IR_SKILL_PROFESSION_PRIMARY,
    IR_SKILL_PROFESSION_SECONDARY,
    IR_SKILL_GATHERING_HERBALISM,
    IR_SKILL_GATHERING_MINING,
    IR_SKILL_GATHERING_SKINNING,
    IR_SKILL_GATHERING_FISHING,
    IR_SKILL_WEAPON,
    IR_DROP_POOR,
    IR_DROP_NORMAL,
    IR_DROP_UNCOMMON,
    IR_DROP_RARE,
    IR_DROP_EPIC,
    IR_DROP_LEGENDARY,
    IR_DROP_ARTIFACT,
    IR_DROP_REFERENCED,
    IR_DROP_CHANCE,
    IR_DROP_QUEST_ITEM,
    IR_DROP_RECIPE,
    IR_DROP_PET,
    IR_DROP_MOUNT,
    IR_DROP_MONEY,
    IR_DROP_EMBLEM,
    IR_DUNGEON_RATE_LIMIT,
    IR_DUNGEON_RATE_DISABLED,
    IR_RAID_RATE_LIMIT,
    IR_RAID_RATE_DISABLED,
    IR_MAX
};

enum IndividualRateCategory : uint8
{
    IRC_XP,
    IRC_XP_LEVEL,
    IRC_COMBAT_SKILLS,
    IRC_PROFESSIONS,
    IRC_REPUTATION,
    IRC_PVP,
    IRC_DROPS,
    IRC_MONEY,
    IRC_EMBLEMS,
    IRC_LOOT_EXCEPTIONS,
    IRC_MAX
};

struct IndividualRateDefinition
{
    char const* CommandKey;
    char const* ConfigKey;
    IndividualRateCategory Category;
    float DefaultRate;
    float MaxRate;
};

struct IndividualRateCategoryDefinition
{
    char const* CommandKey;
    char const* ConfigKey;
    char const* DisplayName;
};

struct AccountUnlockStep
{
    float Rate = 1.0f;
    uint32 Level = 1;
};

static constexpr std::array<IndividualRateCategoryDefinition, IRC_MAX> CategoryDefinitions =
{{
    { "xp", "Category.XP", "Experience" },
    { "xplevel", "Category.XP.Level", "Experience Level Brackets" },
    { "combat", "Category.CombatSkills", "Weapons and Defense" },
    { "professions", "Category.Professions", "Professions" },
    { "reputation", "Category.Reputation", "Reputation" },
    { "pvp", "Category.PvP", "PvP Rewards" },
    { "drops", "Category.Drops", "Drop Rates" },
    { "money", "Category.Money", "Money" },
    { "emblems", "Category.Emblems", "Emblems" },
    { "lootexceptions", "Category.LootExceptions", "Loot Exceptions" }
}};

static constexpr std::array<IndividualRateDefinition, IR_MAX> RateDefinitions =
{{
    { "xp.kill", "Rate.XP.Kill", IRC_XP, 1.0f, 10.0f },
    { "xp.bg.av", "Rate.XP.BattlegroundKillAV", IRC_XP, 1.0f, 10.0f },
    { "xp.bg.wsg", "Rate.XP.BattlegroundKillWSG", IRC_XP, 1.0f, 10.0f },
    { "xp.bg.ab", "Rate.XP.BattlegroundKillAB", IRC_XP, 1.0f, 10.0f },
    { "xp.bg.eots", "Rate.XP.BattlegroundKillEOTS", IRC_XP, 1.0f, 10.0f },
    { "xp.bg.sota", "Rate.XP.BattlegroundKillSOTA", IRC_XP, 1.0f, 10.0f },
    { "xp.bg.ic", "Rate.XP.BattlegroundKillIC", IRC_XP, 1.0f, 10.0f },
    { "xp.quest", "Rate.XP.Quest", IRC_XP, 1.0f, 10.0f },
    { "xp.quest.df", "Rate.XP.Quest.DF", IRC_XP, 1.0f, 10.0f },
    { "xp.explore", "Rate.XP.Explore", IRC_XP, 1.0f, 10.0f },
    { "xp.battlegroundbonus", "Rate.XP.BattlegroundBonus", IRC_XP, 1.0f, 10.0f },
    { "xp.level.1-9", "Rate.XP.Level.1-9", IRC_XP_LEVEL, 1.0f, 10.0f },
    { "xp.level.10-19", "Rate.XP.Level.10-19", IRC_XP_LEVEL, 1.0f, 10.0f },
    { "xp.level.20-29", "Rate.XP.Level.20-29", IRC_XP_LEVEL, 1.0f, 10.0f },
    { "xp.level.30-39", "Rate.XP.Level.30-39", IRC_XP_LEVEL, 1.0f, 10.0f },
    { "xp.level.40-49", "Rate.XP.Level.40-49", IRC_XP_LEVEL, 1.0f, 10.0f },
    { "xp.level.50-59", "Rate.XP.Level.50-59", IRC_XP_LEVEL, 1.0f, 10.0f },
    { "xp.level.60-69", "Rate.XP.Level.60-69", IRC_XP_LEVEL, 1.0f, 10.0f },
    { "xp.level.70-79", "Rate.XP.Level.70-79", IRC_XP_LEVEL, 1.0f, 10.0f },
    { "xp.level.80", "Rate.XP.Level.80", IRC_XP_LEVEL, 1.0f, 10.0f },
    { "rep.global", "Rate.Reputation.Global", IRC_REPUTATION, 1.0f, 10.0f },
    { "rep.kill", "Rate.Reputation.Kill", IRC_REPUTATION, 1.0f, 10.0f },
    { "rep.quest", "Rate.Reputation.Quest", IRC_REPUTATION, 1.0f, 10.0f },
    { "rep.other", "Rate.Reputation.Other", IRC_REPUTATION, 1.0f, 10.0f },
    { "honor", "Rate.Honor", IRC_PVP, 1.0f, 10.0f },
    { "arenapoints", "Rate.ArenaPoints", IRC_PVP, 1.0f, 10.0f },
    { "skill.crafting", "SkillGain.Crafting", IRC_PROFESSIONS, 1.0f, 10.0f },
    { "skill.defense", "SkillGain.Defense", IRC_COMBAT_SKILLS, 1.0f, 10.0f },
    { "skill.gathering", "SkillGain.Gathering", IRC_PROFESSIONS, 1.0f, 10.0f },
    { "skill.prof.primary", "SkillGain.Profession.Primary", IRC_PROFESSIONS, 1.0f, 10.0f },
    { "skill.prof.secondary", "SkillGain.Profession.Secondary", IRC_PROFESSIONS, 1.0f, 10.0f },
    { "skill.gathering.herbalism", "SkillGain.Gathering.Herbalism", IRC_PROFESSIONS, 1.0f, 10.0f },
    { "skill.gathering.mining", "SkillGain.Gathering.Mining", IRC_PROFESSIONS, 1.0f, 10.0f },
    { "skill.gathering.skinning", "SkillGain.Gathering.Skinning", IRC_PROFESSIONS, 1.0f, 10.0f },
    { "skill.gathering.fishing", "SkillGain.Gathering.Fishing", IRC_PROFESSIONS, 1.0f, 10.0f },
    { "skill.weapon", "SkillGain.Weapon", IRC_COMBAT_SKILLS, 1.0f, 10.0f },
    { "drop.poor", "Rate.Drop.Item.Poor", IRC_DROPS, 7.0f, 20.0f },
    { "drop.normal", "Rate.Drop.Item.Normal", IRC_DROPS, 6.0f, 20.0f },
    { "drop.uncommon", "Rate.Drop.Item.Uncommon", IRC_DROPS, 5.0f, 20.0f },
    { "drop.rare", "Rate.Drop.Item.Rare", IRC_DROPS, 5.0f, 20.0f },
    { "drop.epic", "Rate.Drop.Item.Epic", IRC_DROPS, 4.0f, 20.0f },
    { "drop.legendary", "Rate.Drop.Item.Legendary", IRC_DROPS, 2.0f, 20.0f },
    { "drop.artifact", "Rate.Drop.Item.Artifact", IRC_DROPS, 1.0f, 20.0f },
    { "drop.referenced", "Rate.Drop.Item.Referenced", IRC_DROPS, 1.0f, 20.0f },
    { "drop.chance", "Rate.DropChance", IRC_DROPS, 1.0f, 20.0f },
    { "drop.questitem", "Rate.Drop.QuestItem", IRC_DROPS, 1.0f, 20.0f },
    { "drop.recipe", "Rate.Drop.Item.Recipe", IRC_DROPS, 1.0f, 20.0f },
    { "drop.pet", "Rate.Drop.Item.Pet", IRC_DROPS, 1.0f, 20.0f },
    { "drop.mount", "Rate.Drop.Item.Mount", IRC_DROPS, 1.0f, 20.0f },
    { "drop.money", "Rate.Drop.Money", IRC_MONEY, 5.0f, 20.0f },
    { "drop.emblem", "Rate.Drop.Emblem", IRC_EMBLEMS, 1.0f, 4.0f },
    { "dungeon.rate.limit", "Rate.Drop.Exception.Dungeon.Limit", IRC_LOOT_EXCEPTIONS, 0.0f, 10.0f },
    { "dungeon.rate.disabled", "Rate.Drop.Exception.Dungeon.Disabled", IRC_LOOT_EXCEPTIONS, 0.0f, 1.0f },
    { "raid.rate.limit", "Rate.Drop.Exception.Raid.Limit", IRC_LOOT_EXCEPTIONS, 0.0f, 10.0f },
    { "raid.rate.disabled", "Rate.Drop.Exception.Raid.Disabled", IRC_LOOT_EXCEPTIONS, 0.0f, 1.0f }
}};

struct IndividualRatesConfig
{
    bool Enabled = true;
    bool Announce = true;
    bool AnnounceRatesOnLogin = true;
    bool MigrateIndividualXp = true;
    bool CharacterRates = false;
    bool AccountRates = false;
    bool SetCommandUsesAccount = true;
    bool AccountUnlocks = false;
    std::array<bool, IRC_MAX> CategoryEnabled = {};
    std::array<bool, IRC_MAX> CategoryUnlockEnabled = {};
    std::array<uint32, IRC_MAX> CategoryUnlockLevel = {};
    std::array<std::vector<AccountUnlockStep>, IRC_MAX> CategoryUnlockSteps = {};
    std::array<bool, IRC_MAX> CategoryMaxLevelEnabled = {};
    std::array<uint32, IRC_MAX> CategoryMaxLevel = {};
    std::array<float, IRC_MAX> CategoryMaxLevelMaxRate = {};
    std::array<bool, IR_MAX> RateEnabled = {};
    std::array<bool, IR_MAX> UnlockEnabled = {};
    std::array<uint32, IR_MAX> UnlockLevel = {};
    std::array<std::vector<AccountUnlockStep>, IR_MAX> UnlockSteps = {};
    std::array<bool, IR_MAX> MaxLevelEnabled = {};
    std::array<uint32, IR_MAX> MaxLevel = {};
    std::array<float, IR_MAX> MaxLevelMaxRate = {};
    std::array<float, IR_MAX> DefaultRate = {};
    std::array<float, IR_MAX> MaxRate = {};
};

static IndividualRatesConfig individualRates;
static Optional<bool> oldIndividualXpTableExists;

class PlayerIndividualRates : public DataMap::Base
{
public:
    PlayerIndividualRates()
    {
        for (uint8 i = 0; i < IR_MAX; ++i)
        {
            Enabled[i] = individualRates.RateEnabled[i];
            Rate[i] = individualRates.DefaultRate[i];
            AccountEnabled[i] = individualRates.RateEnabled[i];
            AccountRate[i] = individualRates.DefaultRate[i];
        }
    }

    std::array<bool, IR_MAX> Enabled = {};
    std::array<float, IR_MAX> Rate = {};
    std::array<bool, IR_MAX> CharacterHasRate = {};
    std::array<bool, IR_MAX> AccountEnabled = {};
    std::array<float, IR_MAX> AccountRate = {};
    std::array<bool, IR_MAX> AccountHasRate = {};
    bool XpLocked = false;
    uint32 AccountHighestLevel = 1;
};

static PlayerIndividualRates* GetRates(Player const* player)
{
    if (!player)
        return nullptr;

    return const_cast<Player*>(player)->CustomData.GetDefault<PlayerIndividualRates>("IndividualRates");
}

static bool IsUsingAccountRate(PlayerIndividualRates const* data, IndividualRate rate)
{
    return individualRates.AccountRates && data && data->AccountHasRate[rate] &&
        !(individualRates.CharacterRates && data->CharacterHasRate[rate]);
}

static bool IsUsingCharacterRate(PlayerIndividualRates const* data, IndividualRate rate)
{
    return individualRates.CharacterRates && data && data->CharacterHasRate[rate];
}

static bool IsMaxLevelActive(Player const* player, IndividualRate rate)
{
    if (!player)
        return false;

    IndividualRateCategory category = RateDefinitions[rate].Category;
    bool useRateMaxLevel = individualRates.MaxLevelEnabled[rate];
    bool useCategoryMaxLevel = individualRates.CategoryMaxLevelEnabled[category];
    if (!useRateMaxLevel && !useCategoryMaxLevel)
        return false;

    uint32 maxLevel = useRateMaxLevel ? individualRates.MaxLevel[rate] : individualRates.CategoryMaxLevel[category];
    return maxLevel > 0 && player->GetLevel() >= maxLevel;
}

static bool IsRateAvailable(IndividualRate rate)
{
    return individualRates.Enabled &&
        individualRates.CategoryEnabled[RateDefinitions[rate].Category] &&
        individualRates.RateEnabled[rate];
}

static float GetMaxAllowedRate(Player const* player, IndividualRate rate)
{
    float configuredMax = individualRates.MaxRate[rate];
    IndividualRateCategory category = RateDefinitions[rate].Category;
    bool useRateUnlock = individualRates.UnlockEnabled[rate];
    bool useCategoryUnlock = individualRates.CategoryUnlockEnabled[category];

    if (IsMaxLevelActive(player, rate))
    {
        configuredMax = std::min(configuredMax, individualRates.MaxLevelEnabled[rate]
            ? individualRates.MaxLevelMaxRate[rate]
            : individualRates.CategoryMaxLevelMaxRate[category]);
    }

    PlayerIndividualRates* data = GetRates(player);
    if (!data)
        return individualRates.DefaultRate[rate];

    if (!individualRates.AccountUnlocks || (!useRateUnlock && !useCategoryUnlock))
        return configuredMax;

    std::vector<AccountUnlockStep> const& steps = useRateUnlock
        ? individualRates.UnlockSteps[rate]
        : individualRates.CategoryUnlockSteps[category];
    if (!steps.empty())
    {
        float allowedRate = individualRates.DefaultRate[rate];
        for (AccountUnlockStep const& step : steps)
        {
            if (data->AccountHighestLevel >= step.Level)
                allowedRate = std::max(allowedRate, step.Rate);
        }

        return std::min(allowedRate, configuredMax);
    }

    uint32 unlockLevel = useRateUnlock
        ? individualRates.UnlockLevel[rate]
        : individualRates.CategoryUnlockLevel[category];
    if (data->AccountHighestLevel < unlockLevel)
        return individualRates.DefaultRate[rate];

    return configuredMax;
}

static float GetPlayerRate(Player const* player, IndividualRate rate)
{
    PlayerIndividualRates* data = GetRates(player);
    if (!individualRates.Enabled || !data || !individualRates.CategoryEnabled[RateDefinitions[rate].Category] ||
        !individualRates.RateEnabled[rate])
    {
        return 1.0f;
    }

    float rateValue = individualRates.DefaultRate[rate];
    if (individualRates.CharacterRates && data->CharacterHasRate[rate])
        rateValue = data->Enabled[rate] ? data->Rate[rate] : 1.0f;
    else if (individualRates.AccountRates && data->AccountHasRate[rate])
        rateValue = data->AccountEnabled[rate] ? data->AccountRate[rate] : 1.0f;

    return std::min(rateValue, GetMaxAllowedRate(player, rate));
}

static std::set<uint32> emblemItemIds;

static bool IsEmblemItem(uint32 itemId)
{
    return emblemItemIds.find(itemId) != emblemItemIds.end();
}

static bool IsWholeNumberRate(IndividualRate rate)
{
    switch (rate)
    {
        case IR_DROP_EMBLEM:
        case IR_DUNGEON_RATE_LIMIT:
        case IR_DUNGEON_RATE_DISABLED:
        case IR_RAID_RATE_LIMIT:
        case IR_RAID_RATE_DISABLED:
            return true;
        default:
            return false;
    }
}

static float GetMinimumRate(IndividualRate rate)
{
    return rate == IR_DROP_EMBLEM ? 1.0f : 0.0f;
}

static float GetExceptionRate(Player const* player, IndividualRate rate)
{
    PlayerIndividualRates* data = GetRates(player);
    if (!individualRates.Enabled || !data ||
        !individualRates.CategoryEnabled[RateDefinitions[rate].Category] || !individualRates.RateEnabled[rate])
    {
        return 0.0f;
    }

    float rateValue = individualRates.DefaultRate[rate];
    if (individualRates.CharacterRates && data->CharacterHasRate[rate])
        rateValue = data->Enabled[rate] ? data->Rate[rate] : individualRates.DefaultRate[rate];
    else if (individualRates.AccountRates && data->AccountHasRate[rate])
        rateValue = data->AccountEnabled[rate] ? data->AccountRate[rate] : individualRates.DefaultRate[rate];

    return std::min(rateValue, GetMaxAllowedRate(player, rate));
}

static void ApplyLootContextException(Player const* player, float& rate)
{
    if (!player || !player->GetMap())
        return;

    Map const* map = player->GetMap();
    if (map->IsRaid())
    {
        if (GetExceptionRate(player, IR_RAID_RATE_DISABLED) > 0.0f)
        {
            rate = 1.0f;
            return;
        }

        float limit = GetExceptionRate(player, IR_RAID_RATE_LIMIT);
        if (limit > 0.0f)
            rate = std::min(rate, limit);
        return;
    }

    if (map->IsDungeon())
    {
        if (GetExceptionRate(player, IR_DUNGEON_RATE_DISABLED) > 0.0f)
        {
            rate = 1.0f;
            return;
        }

        float limit = GetExceptionRate(player, IR_DUNGEON_RATE_LIMIT);
        if (limit > 0.0f)
            rate = std::min(rate, limit);
    }
}

static uint32 ScaleUInt32(uint32 amount, float rate)
{
    return static_cast<uint32>(std::round(static_cast<float>(amount) * rate));
}

static void ScaleUInt32Ref(uint32& amount, float rate)
{
    amount = ScaleUInt32(amount, rate);
}

static void ScaleFloatRef(float& amount, float rate)
{
    amount *= rate;
}

static Optional<IndividualRate> FindRate(std::string_view key)
{
    for (uint8 i = 0; i < IR_MAX; ++i)
        if (key == RateDefinitions[i].CommandKey)
            return IndividualRate(i);

    return {};
}

static Optional<IndividualRateCategory> FindCategory(std::string_view key)
{
    for (uint8 i = 0; i < IRC_MAX; ++i)
        if (key == CategoryDefinitions[i].CommandKey)
            return IndividualRateCategory(i);

    return {};
}

static std::string_view Trim(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.remove_prefix(1);

    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.remove_suffix(1);

    return value;
}

static std::vector<AccountUnlockStep> ParseUnlockSteps(std::string const& value, float maxRate)
{
    std::vector<AccountUnlockStep> steps;
    for (std::string_view token : Acore::Tokenize(value, ',', false))
    {
        token = Trim(token);
        if (token.empty())
            continue;

        std::vector<std::string_view> parts = Acore::Tokenize(token, ':', false);
        if (parts.size() != 2)
            continue;

        Optional<float> rate = Acore::StringTo<float>(Trim(parts[0]));
        Optional<uint32> level = Acore::StringTo<uint32>(Trim(parts[1]));
        if (!rate || !level || *rate < 0.0f || *level == 0)
            continue;

        steps.push_back({ std::min(*rate, maxRate), *level });
    }

    std::sort(steps.begin(), steps.end(), [](AccountUnlockStep const& left, AccountUnlockStep const& right)
    {
        if (left.Level == right.Level)
            return left.Rate < right.Rate;

        return left.Level < right.Level;
    });

    return steps;
}

static uint32 GetAccountHighestLevel(uint32 accountId)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT MAX(`level`) FROM `characters` WHERE `account` = {}",
        accountId);
    if (!result)
        return 1;

    return std::max<uint32>(1, result->Fetch()[0].Get<uint32>());
}

static bool OldIndividualXpTableExists()
{
    if (!oldIndividualXpTableExists)
    {
        QueryResult result = CharacterDatabase.Query("SHOW TABLES LIKE 'individualxp'");
        oldIndividualXpTableExists = bool(result);
    }

    return *oldIndividualXpTableExists;
}

static void ApplyMigratedXpRate(PlayerIndividualRates* data, float rate)
{
    static constexpr std::array<IndividualRate, 11> xpRates =
    {{
        IR_XP_KILL,
        IR_XP_BG_KILL_AV,
        IR_XP_BG_KILL_WSG,
        IR_XP_BG_KILL_AB,
        IR_XP_BG_KILL_EOTS,
        IR_XP_BG_KILL_SOTA,
        IR_XP_BG_KILL_IC,
        IR_XP_QUEST,
        IR_XP_QUEST_DF,
        IR_XP_EXPLORE,
        IR_XP_BATTLEGROUND_BONUS
    }};

    for (IndividualRate xpRate : xpRates)
    {
        uint8 index = xpRate;
        if (!individualRates.RateEnabled[index])
            continue;

        data->Enabled[index] = true;
        data->Rate[index] = std::min(rate, individualRates.MaxRate[index]);
    }
}

static bool IsWeaponSkill(uint32 skillId)
{
    switch (skillId)
    {
        case SKILL_SWORDS:
        case SKILL_AXES:
        case SKILL_BOWS:
        case SKILL_GUNS:
        case SKILL_MACES:
        case SKILL_2H_SWORDS:
        case SKILL_2H_AXES:
        case SKILL_2H_MACES:
        case SKILL_POLEARMS:
        case SKILL_STAVES:
        case SKILL_UNARMED:
        case SKILL_DAGGERS:
        case SKILL_THROWN:
        case SKILL_CROSSBOWS:
        case SKILL_WANDS:
        case SKILL_FIST_WEAPONS:
            return true;
        default:
            return false;
    }
}

static bool IsSecondaryProfessionSkill(uint32 skillId)
{
    switch (skillId)
    {
        case SKILL_COOKING:
        case SKILL_FIRST_AID:
        case SKILL_FISHING:
            return true;
        default:
            return false;
    }
}

static float GetProfessionRate(Player const* player, uint32 skillId)
{
    float rate = 1.0f;
    if (IsPrimaryProfessionSkill(skillId))
        rate *= GetPlayerRate(player, IR_SKILL_PROFESSION_PRIMARY);
    else if (IsSecondaryProfessionSkill(skillId))
        rate *= GetPlayerRate(player, IR_SKILL_PROFESSION_SECONDARY);

    return rate;
}

static float GetGatheringSpecificRate(Player const* player, uint32 skillId)
{
    switch (skillId)
    {
        case SKILL_HERBALISM:
            return GetPlayerRate(player, IR_SKILL_GATHERING_HERBALISM);
        case SKILL_MINING:
            return GetPlayerRate(player, IR_SKILL_GATHERING_MINING);
        case SKILL_SKINNING:
            return GetPlayerRate(player, IR_SKILL_GATHERING_SKINNING);
        case SKILL_FISHING:
            return GetPlayerRate(player, IR_SKILL_GATHERING_FISHING);
        default:
            return 1.0f;
    }
}

static IndividualRate KillXpRateForPlayer(Player const* player)
{
    if (!player || !player->InBattleground())
        return IR_XP_KILL;

    switch (player->GetMapId())
    {
        case MAP_ALTERAC_VALLEY:
            return IR_XP_BG_KILL_AV;
        case MAP_WARSONG_GULCH:
            return IR_XP_BG_KILL_WSG;
        case MAP_ARATHI_BASIN:
            return IR_XP_BG_KILL_AB;
        case MAP_EYE_OF_THE_STORM:
            return IR_XP_BG_KILL_EOTS;
        case MAP_STRAND_OF_THE_ANCIENTS:
            return IR_XP_BG_KILL_SOTA;
        case MAP_ISLE_OF_CONQUEST:
            return IR_XP_BG_KILL_IC;
        default:
            return IR_XP_KILL;
    }
}

static IndividualRate LevelXpRateForPlayer(Player const* player)
{
    if (!player)
        return IR_XP_LEVEL_1_9;

    uint8 level = player->GetLevel();
    if (level <= 9)
        return IR_XP_LEVEL_1_9;
    if (level <= 19)
        return IR_XP_LEVEL_10_19;
    if (level <= 29)
        return IR_XP_LEVEL_20_29;
    if (level <= 39)
        return IR_XP_LEVEL_30_39;
    if (level <= 49)
        return IR_XP_LEVEL_40_49;
    if (level <= 59)
        return IR_XP_LEVEL_50_59;
    if (level <= 69)
        return IR_XP_LEVEL_60_69;
    if (level <= 79)
        return IR_XP_LEVEL_70_79;

    return IR_XP_LEVEL_80;
}

static float GetXpLevelRate(Player const* player)
{
    return GetPlayerRate(player, LevelXpRateForPlayer(player));
}

static IndividualRate DropRateForLootItem(LootStoreItem const* lootStoreItem)
{
    if (lootStoreItem->reference)
        return IR_DROP_REFERENCED;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(lootStoreItem->itemid);
    if (!proto || proto->Quality >= ITEM_QUALITY_HEIRLOOM)
        return IR_DROP_NORMAL;

    switch (proto->Quality)
    {
        case ITEM_QUALITY_POOR:
            return IR_DROP_POOR;
        case ITEM_QUALITY_NORMAL:
            return IR_DROP_NORMAL;
        case ITEM_QUALITY_UNCOMMON:
            return IR_DROP_UNCOMMON;
        case ITEM_QUALITY_RARE:
            return IR_DROP_RARE;
        case ITEM_QUALITY_EPIC:
            return IR_DROP_EPIC;
        case ITEM_QUALITY_LEGENDARY:
            return IR_DROP_LEGENDARY;
        case ITEM_QUALITY_ARTIFACT:
            return IR_DROP_ARTIFACT;
        default:
            return IR_DROP_NORMAL;
    }
}

static IndividualRate SpecificDropRateForLootItem(LootStoreItem const* lootStoreItem)
{
    if (lootStoreItem->needs_quest)
        return IR_DROP_QUEST_ITEM;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(lootStoreItem->itemid);
    if (!proto)
        return IR_DROP_CHANCE;

    if (proto->Class == ITEM_CLASS_RECIPE)
        return IR_DROP_RECIPE;

    if (proto->Class == ITEM_CLASS_MISC)
    {
        if (proto->SubClass == ITEM_SUBCLASS_JUNK_PET)
            return IR_DROP_PET;

        if (proto->SubClass == ITEM_SUBCLASS_JUNK_MOUNT)
            return IR_DROP_MOUNT;
    }

    return IR_DROP_CHANCE;
}

class IndividualRatesWorldScript : public WorldScript
{
public:
    IndividualRatesWorldScript() : WorldScript("IndividualRatesWorldScript") { }

    void OnBeforeConfigLoad(bool /*reload*/) override
    {
        individualRates.Enabled = sConfigMgr->GetOption<bool>("IndividualRates.Enabled", true);
        individualRates.Announce = sConfigMgr->GetOption<bool>("IndividualRates.Announce", true);
        individualRates.AnnounceRatesOnLogin =
            sConfigMgr->GetOption<bool>("IndividualRates.AnnounceRatesOnLogin", true);
        individualRates.MigrateIndividualXp =
            sConfigMgr->GetOption<bool>("IndividualRates.MigrateIndividualXp.Enabled", true);
        individualRates.CharacterRates = sConfigMgr->GetOption<bool>("IndividualRates.CharacterRates.Enabled", false);
        individualRates.AccountRates = sConfigMgr->GetOption<bool>("IndividualRates.AccountRates.Enabled", true);
        individualRates.SetCommandUsesAccount =
            sConfigMgr->GetOption<bool>("IndividualRates.SetCommand.AccountDefault", true);
        individualRates.AccountUnlocks = sConfigMgr->GetOption<bool>("IndividualRates.AccountUnlocks.Enabled", false);

        for (uint8 i = 0; i < IRC_MAX; ++i)
        {
            std::string const enabledKey =
                Acore::StringFormat("IndividualRates.{}.Enabled", CategoryDefinitions[i].ConfigKey);
            std::string const unlockKey =
                Acore::StringFormat("IndividualRates.{}.Unlock.Enabled", CategoryDefinitions[i].ConfigKey);
            std::string const unlockLevelKey =
                Acore::StringFormat("IndividualRates.{}.Unlock.Level", CategoryDefinitions[i].ConfigKey);
            std::string const unlockStepsKey =
                Acore::StringFormat("IndividualRates.{}.Unlock.Steps", CategoryDefinitions[i].ConfigKey);
            std::string const maxLevelKey =
                Acore::StringFormat("IndividualRates.{}.MaxLevel.Enabled", CategoryDefinitions[i].ConfigKey);
            std::string const maxLevelLevelKey =
                Acore::StringFormat("IndividualRates.{}.MaxLevel.Level", CategoryDefinitions[i].ConfigKey);
            std::string const maxLevelMaxRateKey =
                Acore::StringFormat("IndividualRates.{}.MaxLevel.MaxRate", CategoryDefinitions[i].ConfigKey);

            individualRates.CategoryEnabled[i] = sConfigMgr->GetOption<bool>(enabledKey, true);
            individualRates.CategoryUnlockEnabled[i] = sConfigMgr->GetOption<bool>(unlockKey, false);
            individualRates.CategoryUnlockLevel[i] = sConfigMgr->GetOption<uint32>(unlockLevelKey, 80);
            individualRates.CategoryUnlockSteps[i] =
                ParseUnlockSteps(sConfigMgr->GetOption<std::string>(unlockStepsKey, ""), 100000.0f);
            individualRates.CategoryMaxLevelEnabled[i] = sConfigMgr->GetOption<bool>(maxLevelKey, false);
            individualRates.CategoryMaxLevel[i] = sConfigMgr->GetOption<uint32>(maxLevelLevelKey, 70);
            individualRates.CategoryMaxLevelMaxRate[i] = sConfigMgr->GetOption<float>(maxLevelMaxRateKey, 3.0f);
        }

        for (uint8 i = 0; i < IR_MAX; ++i)
        {
            std::string const enabledKey =
                Acore::StringFormat("IndividualRates.{}.Enabled", RateDefinitions[i].ConfigKey);
            std::string const defaultKey =
                Acore::StringFormat("IndividualRates.{}.Default", RateDefinitions[i].ConfigKey);
            std::string const maxKey = Acore::StringFormat("IndividualRates.{}.Max", RateDefinitions[i].ConfigKey);
            std::string const unlockKey =
                Acore::StringFormat("IndividualRates.{}.Unlock.Enabled", RateDefinitions[i].ConfigKey);
            std::string const unlockLevelKey =
                Acore::StringFormat("IndividualRates.{}.Unlock.Level", RateDefinitions[i].ConfigKey);
            std::string const unlockStepsKey =
                Acore::StringFormat("IndividualRates.{}.Unlock.Steps", RateDefinitions[i].ConfigKey);
            std::string const maxLevelKey =
                Acore::StringFormat("IndividualRates.{}.MaxLevel.Enabled", RateDefinitions[i].ConfigKey);
            std::string const maxLevelLevelKey =
                Acore::StringFormat("IndividualRates.{}.MaxLevel.Level", RateDefinitions[i].ConfigKey);
            std::string const maxLevelMaxRateKey =
                Acore::StringFormat("IndividualRates.{}.MaxLevel.MaxRate", RateDefinitions[i].ConfigKey);

            individualRates.RateEnabled[i] = sConfigMgr->GetOption<bool>(enabledKey, true);
            individualRates.DefaultRate[i] = sConfigMgr->GetOption<float>(defaultKey, RateDefinitions[i].DefaultRate);
            individualRates.MaxRate[i] = sConfigMgr->GetOption<float>(maxKey, RateDefinitions[i].MaxRate);
            individualRates.UnlockEnabled[i] = sConfigMgr->GetOption<bool>(unlockKey, false);
            individualRates.UnlockLevel[i] = sConfigMgr->GetOption<uint32>(unlockLevelKey, 80);
            individualRates.UnlockSteps[i] =
                ParseUnlockSteps(sConfigMgr->GetOption<std::string>(unlockStepsKey, ""), individualRates.MaxRate[i]);
            individualRates.MaxLevelEnabled[i] = sConfigMgr->GetOption<bool>(maxLevelKey, false);
            individualRates.MaxLevel[i] = sConfigMgr->GetOption<uint32>(maxLevelLevelKey, 70);
            individualRates.MaxLevelMaxRate[i] = sConfigMgr->GetOption<float>(maxLevelMaxRateKey, 3.0f);
        }

        emblemItemIds.clear();
        for (std::string_view token : Acore::Tokenize(
            sConfigMgr->GetOption<std::string>("IndividualRates.Drop.Emblem.ItemIds",
                "29434,40752,40753,45624,47241,49426"), ',', false))
        {
            token = Trim(token);
            if (Optional<uint32> itemId = Acore::StringTo<uint32>(token))
                emblemItemIds.insert(*itemId);
        }
    }
};

class IndividualRatesPlayerScript : public PlayerScript
{
public:
    IndividualRatesPlayerScript() : PlayerScript("IndividualRatesPlayerScript") { }

    void OnPlayerLogin(Player* player) override
    {
        PlayerIndividualRates* data = player->CustomData.GetDefault<PlayerIndividualRates>("IndividualRates");
        data->AccountHighestLevel = GetAccountHighestLevel(player->GetSession()->GetAccountId());

        bool migratedIndividualXp = false;
        if (individualRates.CharacterRates)
        {
            QueryResult result = CharacterDatabase.Query(
                "SELECT `RateKey`, `RateValue`, `Enabled` FROM `individual_rates` WHERE `CharacterGUID` = {}",
                player->GetGUID().GetCounter());
            if (result)
            {
                do
                {
                    Field* fields = result->Fetch();
                    if (Optional<IndividualRate> rate = FindRate(fields[0].Get<std::string>()))
                    {
                        uint8 index = *rate;
                        data->Rate[index] = fields[1].Get<float>();
                        data->Enabled[index] = fields[2].Get<bool>();
                        data->CharacterHasRate[index] = true;
                    }
                } while (result->NextRow());
            }
            else if (individualRates.MigrateIndividualXp && OldIndividualXpTableExists())
            {
                QueryResult xpResult = CharacterDatabase.Query(
                    "SELECT `XPRate` FROM `individualxp` WHERE `CharacterGUID` = {}",
                    player->GetGUID().GetCounter());
                if (xpResult)
                {
                    ApplyMigratedXpRate(data, xpResult->Fetch()[0].Get<float>());
                    std::array<IndividualRate, 11> migratedRates =
                    {
                        IR_XP_KILL,
                        IR_XP_QUEST,
                        IR_XP_QUEST_DF,
                        IR_XP_EXPLORE,
                        IR_XP_BATTLEGROUND_BONUS,
                        IR_XP_BG_KILL_AV,
                        IR_XP_BG_KILL_WSG,
                        IR_XP_BG_KILL_AB,
                        IR_XP_BG_KILL_EOTS,
                        IR_XP_BG_KILL_SOTA,
                        IR_XP_BG_KILL_IC
                    };

                    for (IndividualRate rate : migratedRates)
                        data->CharacterHasRate[rate] = true;

                    migratedIndividualXp = true;
                }
            }
        }

        if (individualRates.AccountRates)
        {
            QueryResult accountResult = CharacterDatabase.Query(
                "SELECT `RateKey`, `RateValue`, `Enabled` FROM `account_individual_rates` WHERE `AccountID` = {}",
                player->GetSession()->GetAccountId());
            if (accountResult)
            {
                do
                {
                    Field* fields = accountResult->Fetch();
                    if (Optional<IndividualRate> rate = FindRate(fields[0].Get<std::string>()))
                    {
                        uint8 index = *rate;
                        data->AccountRate[index] = fields[1].Get<float>();
                        data->AccountEnabled[index] = fields[2].Get<bool>();
                        data->AccountHasRate[index] = true;
                    }
                } while (accountResult->NextRow());
            }
        }

        QueryResult settingsResult = CharacterDatabase.Query(
            "SELECT `Enabled` FROM `individual_rate_settings` WHERE `CharacterGUID` = {} "
            "AND `SettingKey` = 'xp.locked'",
            player->GetGUID().GetCounter());
        if (settingsResult)
            data->XpLocked = settingsResult->Fetch()[0].Get<bool>();

        if (individualRates.AccountRates)
        {
            QueryResult accountSettingsResult = CharacterDatabase.Query(
                "SELECT `Enabled` FROM `account_individual_rate_settings` WHERE `AccountID` = {} "
                "AND `SettingKey` = 'xp.locked'",
                player->GetSession()->GetAccountId());
            if (accountSettingsResult)
                data->XpLocked = accountSettingsResult->Fetch()[0].Get<bool>();
        }

        if (individualRates.Enabled && individualRates.Announce)
            ChatHandler(player->GetSession()).SendSysMessage(
                "This server is running the Individual Rates module. Use .rate view to see your rates.");

        if (migratedIndividualXp)
        {
            ChatHandler(player->GetSession()).SendSysMessage(
                "[Rates] Individual XP data is handled by the new Individual Rates module.");
        }

        if (individualRates.Enabled && individualRates.AnnounceRatesOnLogin)
        {
            ChatHandler handler(player->GetSession());
            SendRateList(&handler, player, true);
        }
    }

    void OnPlayerLogout(Player* player) override
    {
        PlayerIndividualRates* data = player->CustomData.Get<PlayerIndividualRates>("IndividualRates");
        if (!data)
            return;

        if (individualRates.CharacterRates)
        {
            for (uint8 i = 0; i < IR_MAX; ++i)
            {
                if (!data->CharacterHasRate[i])
                    continue;

                CharacterDatabase.DirectExecute(
                    "REPLACE INTO `individual_rates` (`CharacterGUID`, `RateKey`, `RateValue`, `Enabled`) "
                    "VALUES ({}, '{}', {}, {})",
                    player->GetGUID().GetCounter(), RateDefinitions[i].CommandKey, data->Rate[i],
                    data->Enabled[i] ? 1 : 0);
            }
        }

        CharacterDatabase.DirectExecute(
            "REPLACE INTO `individual_rate_settings` (`CharacterGUID`, `SettingKey`, `Enabled`) "
            "VALUES ({}, 'xp.locked', {})",
            player->GetGUID().GetCounter(), data->XpLocked ? 1 : 0);
    }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* /*victim*/, uint8 xpSource) override
    {
        if (PlayerIndividualRates* data = GetRates(player))
        {
            if (data->XpLocked)
            {
                amount = 0;
                return;
            }
        }

        switch (xpSource)
        {
            case XPSOURCE_KILL:
                ScaleUInt32Ref(amount, GetPlayerRate(player, KillXpRateForPlayer(player)) * GetXpLevelRate(player));
                break;
            case XPSOURCE_EXPLORE:
                ScaleUInt32Ref(amount, GetPlayerRate(player, IR_XP_EXPLORE) * GetXpLevelRate(player));
                break;
            case XPSOURCE_BATTLEGROUND:
                ScaleUInt32Ref(amount, GetPlayerRate(player, IR_XP_BATTLEGROUND_BONUS) * GetXpLevelRate(player));
                break;
            default:
                break;
        }
    }

    void OnPlayerQuestComputeXP(Player* player, Quest const* quest, uint32& xpValue) override
    {
        if (!quest)
            return;

        if (PlayerIndividualRates* data = GetRates(player))
        {
            if (data->XpLocked)
            {
                xpValue = 0;
                return;
            }
        }

        ScaleUInt32Ref(xpValue,
            GetPlayerRate(player, quest->IsDFQuest() ? IR_XP_QUEST_DF : IR_XP_QUEST) * GetXpLevelRate(player));
    }

    void OnPlayerGiveReputation(Player* player, int32 /*factionID*/, float& amount, ReputationSource repSource) override
    {
        IndividualRate sourceRate = IR_REPUTATION_OTHER;
        switch (repSource)
        {
            case REPUTATION_SOURCE_KILL:
                sourceRate = IR_REPUTATION_KILL;
                break;
            case REPUTATION_SOURCE_QUEST:
            case REPUTATION_SOURCE_DAILY_QUEST:
            case REPUTATION_SOURCE_WEEKLY_QUEST:
            case REPUTATION_SOURCE_MONTHLY_QUEST:
            case REPUTATION_SOURCE_REPEATABLE_QUEST:
                sourceRate = IR_REPUTATION_QUEST;
                break;
            default:
                break;
        }

        ScaleFloatRef(amount, GetPlayerRate(player, IR_REPUTATION_GLOBAL) * GetPlayerRate(player, sourceRate));
    }

    void OnPlayerVictimRewardAfter(Player* player, Player* /*victim*/, uint32& /*killer_title*/,
        int32& /*victim_rank*/, float& honor) override
    {
        ScaleFloatRef(honor, GetPlayerRate(player, IR_HONOR));
    }

    void OnPlayerUpdateGatheringSkill(Player* player, uint32 skillId, uint32 /*current*/, uint32 /*gray*/,
        uint32 /*green*/, uint32 /*yellow*/, uint32& gain) override
    {
        ScaleUInt32Ref(gain,
            GetPlayerRate(player, IR_SKILL_GATHERING) *
            GetProfessionRate(player, skillId) *
            GetGatheringSpecificRate(player, skillId));
    }

    void OnPlayerUpdateCraftingSkill(Player* player, SkillLineAbilityEntry const* skill,
        uint32 /*currentLevel*/, uint32& gain) override
    {
        uint32 skillId = skill ? skill->SkillLine : 0;
        ScaleUInt32Ref(gain, GetPlayerRate(player, IR_SKILL_CRAFTING) * GetProfessionRate(player, skillId));
    }

    bool OnPlayerCanUpdateSkill(Player* player, uint32 skillId) override
    {
        if (skillId == SKILL_DEFENSE)
            return GetPlayerRate(player, IR_SKILL_DEFENSE) > 0.0f;

        if (IsWeaponSkill(skillId))
            return GetPlayerRate(player, IR_SKILL_WEAPON) > 0.0f;

        return true;
    }

    void OnPlayerUpdateSkill(Player* player, uint32 skillId, uint32 value, uint32 max, uint32 step,
        uint32 newValue) override
    {
        float rate = 1.0f;
        if (skillId == SKILL_DEFENSE)
            rate = GetPlayerRate(player, IR_SKILL_DEFENSE);
        else if (IsWeaponSkill(skillId))
            rate = GetPlayerRate(player, IR_SKILL_WEAPON);

        if (rate <= 1.0f || newValue >= max)
            return;

        uint32 scaledStep = ScaleUInt32(step, rate);
        if (scaledStep <= step)
            return;

        uint32 scaledValue = std::min(value + scaledStep, max);
        if (scaledValue > newValue)
            player->SetSkill(skillId, player->GetSkillStep(skillId), scaledValue, max);
    }

    static void SendRateList(ChatHandler* handler, Player* player, bool compact)
    {
        PlayerIndividualRates* data = GetRates(player);
        if (!data)
            return;

        handler->PSendSysMessage("[Rates] Your individual rates:");
        if (data->XpLocked)
            handler->SendSysMessage("  xp is locked");

        for (uint8 i = 0; i < IR_MAX; ++i)
        {
            if (compact && !IsRateAvailable(IndividualRate(i)))
                continue;

            if (IsMaxLevelActive(player, IndividualRate(i)))
            {
                handler->PSendSysMessage("  {} = {} (max-level cap, default: {})",
                    RateDefinitions[i].CommandKey, GetPlayerRate(player, IndividualRate(i)),
                    individualRates.DefaultRate[i]);
                continue;
            }

            if (IsUsingCharacterRate(data, IndividualRate(i)))
            {
                handler->PSendSysMessage("  {} = {} (character {}, default: {})",
                    RateDefinitions[i].CommandKey, data->Rate[i], data->Enabled[i] ? "enabled" : "disabled",
                    individualRates.DefaultRate[i]);
                continue;
            }

            if (IsUsingAccountRate(data, IndividualRate(i)))
            {
                handler->PSendSysMessage("  {} = {} (account {}, default: {})",
                    RateDefinitions[i].CommandKey, data->AccountRate[i],
                    data->AccountEnabled[i] ? "enabled" : "disabled", individualRates.DefaultRate[i]);
                continue;
            }

            handler->PSendSysMessage("  {} = {} (server default)",
                RateDefinitions[i].CommandKey, individualRates.DefaultRate[i]);
        }
    }
};

class IndividualRatesGlobalScript : public GlobalScript
{
public:
    IndividualRatesGlobalScript() : GlobalScript("IndividualRatesGlobalScript") { }

    bool OnItemRoll(Player const* player, LootStoreItem const* lootStoreItem, float& chance,
        Loot& /*loot*/, LootStore const& /*store*/) override
    {
        float rate = GetPlayerRate(player, IR_DROP_CHANCE) * GetPlayerRate(player, DropRateForLootItem(lootStoreItem));
        IndividualRate specificRate = SpecificDropRateForLootItem(lootStoreItem);
        if (specificRate != IR_DROP_CHANCE)
            rate *= GetPlayerRate(player, specificRate);

        ApplyLootContextException(player, rate);
        ScaleFloatRef(chance, rate);
        return true;
    }

    void OnBeforeDropAddItem(Player const* player, Loot& loot, bool /*canRate*/, uint16 /*lootMode*/,
        LootStoreItem* lootStoreItem, LootStore const& /*store*/) override
    {
        if (!individualRates.Enabled || !lootStoreItem || !IsEmblemItem(lootStoreItem->itemid))
            return;

        float emblemRate = GetPlayerRate(player, IR_DROP_EMBLEM);
        ApplyLootContextException(player, emblemRate);

        uint32 copies = std::clamp<uint32>(static_cast<uint32>(std::round(emblemRate)), 1u, 4u);
        for (uint32 i = 1; i < copies; ++i)
            loot.AddItem(*lootStoreItem);
    }

    void OnBeforeUpdateArenaPoints(ArenaTeam* /*team*/, std::map<ObjectGuid, uint32>& points) override
    {
        for (auto& pointPair : points)
        {
            if (Player* player = ObjectAccessor::FindPlayer(pointPair.first))
                ScaleUInt32Ref(pointPair.second, GetPlayerRate(player, IR_ARENA_POINTS));
        }
    }
};

class IndividualRatesLootMoneyScript : public PlayerScript
{
public:
    IndividualRatesLootMoneyScript() : PlayerScript("IndividualRatesLootMoneyScript") { }

    void OnPlayerBeforeSendLoot(Player* player, ObjectGuid lootGuid, Loot* loot) override
    {
        if (!loot || !loot->gold || _scaledLootGuids.count(lootGuid.GetRawValue()))
            return;

        float moneyRate = GetPlayerRate(player, IR_DROP_MONEY);
        ApplyLootContextException(player, moneyRate);
        ScaleUInt32Ref(loot->gold, moneyRate);
        _scaledLootGuids.insert(lootGuid.GetRawValue());
    }

private:
    std::set<uint64> _scaledLootGuids;
};

class IndividualRatesCommandScript : public CommandScript
{
public:
    IndividualRatesCommandScript() : CommandScript("IndividualRatesCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable rateCommandTable =
        {
            { "help", HandleHelpCommand, SEC_PLAYER, Console::No },
            { "list", HandleListCommand, SEC_PLAYER, Console::No },
            { "view", HandleViewCommand, SEC_PLAYER, Console::No },
            { "set", HandleSetCommand, SEC_PLAYER, Console::No },
            { "setall", HandleSetCommand, SEC_PLAYER, Console::No },
            { "account set", HandleAccountSetCommand, SEC_PLAYER, Console::No },
            { "character set", HandleCharacterSetCommand, SEC_PLAYER, Console::No },
            { "character reset", HandleCharacterResetCommand, SEC_PLAYER, Console::No },
            { "character default", HandleCharacterDefaultCommand, SEC_PLAYER, Console::No },
            { "character enable", HandleCharacterEnableCommand, SEC_PLAYER, Console::No },
            { "character on", HandleCharacterEnableCommand, SEC_PLAYER, Console::No },
            { "character disable", HandleCharacterDisableCommand, SEC_PLAYER, Console::No },
            { "character off", HandleCharacterDisableCommand, SEC_PLAYER, Console::No },
            { "reset", HandleResetCommand, SEC_PLAYER, Console::No },
            { "default", HandleDefaultCommand, SEC_PLAYER, Console::No },
            { "enable", HandleEnableCommand, SEC_PLAYER, Console::No },
            { "on", HandleEnableCommand, SEC_PLAYER, Console::No },
            { "disable", HandleDisableCommand, SEC_PLAYER, Console::No },
            { "off", HandleDisableCommand, SEC_PLAYER, Console::No },
            { "lock", HandleLockCommand, SEC_PLAYER, Console::No },
            { "unlock", HandleUnlockCommand, SEC_PLAYER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "rate", rateCommandTable }
        };

        return commandTable;
    }

    static bool HandleHelpCommand(ChatHandler* handler)
    {
        handler->SendSysMessage("[Rates] Commands:");
        handler->SendSysMessage("  .rate view - show changeable rates only");
        handler->SendSysMessage("  .rate list - show changeable rate keys and category keys");
        handler->SendSysMessage(
            "  .rate set <key|category> <value> - set one rate or category using the server default target");
        handler->SendSysMessage("  .rate setall <category> <value> - set all rates in a category");
        handler->SendSysMessage("  .rate account set <key|category> <value> - set account rates");
        handler->SendSysMessage("  .rate character set <key|category> <value> - set a character override");
        handler->SendSysMessage("  .rate character reset <key|category> - remove character override");
        handler->SendSysMessage("  .rate reset <key|category> - remove account and character overrides");
        handler->SendSysMessage("  .rate default <key> - restore one rate for the server default target");
        handler->SendSysMessage("  .rate character default <key> - remove one character override");
        handler->SendSysMessage("  .rate enable|disable <key> - toggle one rate for the server default target");
        handler->SendSysMessage("  .rate character enable|disable <key> - toggle one character override");
        handler->SendSysMessage("  .rate on|off <key> - alias for enable/disable");
        handler->SendSysMessage("  .rate lock / .rate unlock - toggle XP gain");
        handler->SendSysMessage("  .rate set drop.emblem <1-4> - emblem count per loot (whole numbers)");
        handler->SendSysMessage("  .rate set dungeon.rate.limit <0-10> - cap your loot multiplier in dungeons");
        handler->SendSysMessage("  .rate set dungeon.rate.disabled <0|1> - force loot x1 in dungeons");
        handler->SendSysMessage("  .rate set raid.rate.limit <0-10> - cap your loot multiplier in raids");
        handler->SendSysMessage("  .rate set raid.rate.disabled <0|1> - force loot x1 in raids");
        return true;
    }

    static bool HandleListCommand(ChatHandler* handler)
    {
        if (!CheckModuleEnabled(handler))
            return false;

        handler->SendSysMessage("[Rates] Categories:");
        for (uint8 i = 0; i < IRC_MAX; ++i)
        {
            if (!individualRates.CategoryEnabled[i])
                continue;

            handler->PSendSysMessage("  {} - {}",
                CategoryDefinitions[i].CommandKey, CategoryDefinitions[i].DisplayName);
        }

        IndividualRatesPlayerScript::SendRateList(handler, handler->GetSession()->GetPlayer(), true);
        return true;
    }

    static bool HandleViewCommand(ChatHandler* handler)
    {
        if (!CheckModuleEnabled(handler))
            return false;

        IndividualRatesPlayerScript::SendRateList(handler, handler->GetSession()->GetPlayer(), true);
        return true;
    }

    static bool HandleSetCommand(ChatHandler* handler, Tail args)
    {
        return SetRateOrCategory(handler, args, individualRates.SetCommandUsesAccount);
    }

    static bool HandleAccountSetCommand(ChatHandler* handler, Tail args)
    {
        return SetRateOrCategory(handler, args, true);
    }

    static bool HandleCharacterSetCommand(ChatHandler* handler, Tail args)
    {
        return SetRateOrCategory(handler, args, false);
    }

    static bool HandleDefaultCommand(ChatHandler* handler, Tail args)
    {
        return SetRateDefault(handler, args, individualRates.SetCommandUsesAccount);
    }

    static bool HandleCharacterDefaultCommand(ChatHandler* handler, Tail args)
    {
        return SetRateDefault(handler, args, false);
    }

    static bool HandleCharacterResetCommand(ChatHandler* handler, Tail args)
    {
        return ResetRateOrCategory(handler, args, false);
    }

    static bool HandleResetCommand(ChatHandler* handler, Tail args)
    {
        return ResetRateOrCategory(handler, args, true);
    }

    static bool HandleEnableCommand(ChatHandler* handler, Tail args)
    {
        return SetRateEnabled(handler, args, true, individualRates.SetCommandUsesAccount);
    }

    static bool HandleCharacterEnableCommand(ChatHandler* handler, Tail args)
    {
        return SetRateEnabled(handler, args, true, false);
    }

    static bool HandleDisableCommand(ChatHandler* handler, Tail args)
    {
        return SetRateEnabled(handler, args, false, individualRates.SetCommandUsesAccount);
    }

    static bool HandleCharacterDisableCommand(ChatHandler* handler, Tail args)
    {
        return SetRateEnabled(handler, args, false, false);
    }

    static bool SetRateDefault(ChatHandler* handler, Tail args, bool account)
    {
        if (!CheckModuleEnabled(handler))
            return false;

        if (!CheckStorageModeEnabled(handler, account))
            return false;

        std::vector<std::string_view> tokens = Acore::Tokenize(args, ' ', false);
        if (tokens.size() != 1)
        {
            handler->PSendSysMessage("Usage: .rate {}default <key>", account ? "" : "character ");
            return false;
        }

        Optional<IndividualRate> rate = FindRate(tokens[0]);
        if (!rate)
            return SendUnknownRate(handler);

        uint8 index = *rate;
        if (!CanUseRate(handler, index))
            return false;

        PlayerIndividualRates* data = GetRates(handler->GetSession()->GetPlayer());
        if (account)
        {
            data->AccountRate[index] = individualRates.DefaultRate[index];
            data->AccountEnabled[index] = true;
            data->AccountHasRate[index] = false;
            DeleteAccountRate(handler->GetSession()->GetPlayer(), index);
            handler->PSendSysMessage("[Rates] {} account rate restored to {}.",
                RateDefinitions[index].CommandKey, individualRates.DefaultRate[index]);
        }
        else
        {
            data->Rate[index] = individualRates.DefaultRate[index];
            data->Enabled[index] = true;
            data->CharacterHasRate[index] = false;
            DeleteCharacterRate(handler->GetSession()->GetPlayer(), index);
            handler->PSendSysMessage("[Rates] {} character override removed.", RateDefinitions[index].CommandKey);
        }

        return true;
    }

    static bool HandleLockCommand(ChatHandler* handler)
    {
        if (!CheckModuleEnabled(handler))
            return false;

        PlayerIndividualRates* data = GetRates(handler->GetSession()->GetPlayer());
        data->XpLocked = true;
        handler->SendSysMessage("[Rates] XP gain locked.");
        return true;
    }

    static bool HandleUnlockCommand(ChatHandler* handler)
    {
        if (!CheckModuleEnabled(handler))
            return false;

        PlayerIndividualRates* data = GetRates(handler->GetSession()->GetPlayer());
        data->XpLocked = false;
        handler->SendSysMessage("[Rates] XP gain unlocked.");
        return true;
    }

private:
    static bool CheckModuleEnabled(ChatHandler* handler)
    {
        if (individualRates.Enabled)
            return true;

        handler->SendSysMessage("[Rates] The Individual Rates module is disabled.");
        return false;
    }

    static bool CanUseRate(ChatHandler* handler, uint8 index)
    {
        if (!IsRateAvailable(IndividualRate(index)))
        {
            handler->PSendSysMessage("[Rates] {} is disabled by server config.", RateDefinitions[index].CommandKey);
            return false;
        }

        return true;
    }

    static bool SendUnknownRate(ChatHandler* handler)
    {
        handler->SendSysMessage("Unknown rate key or category. Use .rate list to see valid keys.");
        return false;
    }

    static bool SetRateOrCategory(ChatHandler* handler, Tail args, bool account)
    {
        if (!CheckModuleEnabled(handler))
            return false;

        if (!CheckStorageModeEnabled(handler, account))
            return false;

        std::vector<std::string_view> tokens = Acore::Tokenize(args, ' ', false);
        if (tokens.size() != 2)
        {
            handler->PSendSysMessage("Usage: .rate {}<key|category> <value>",
                account ? "account set " : "character set ");
            return false;
        }

        Optional<float> value = Acore::StringTo<float>(tokens[1]);
        if (!value)
        {
            handler->SendSysMessage("Rate value must be a number.");
            return false;
        }

        if (Optional<IndividualRate> rate = FindRate(tokens[0]))
            return SetSingleRate(handler, *rate, *value, account);

        if (Optional<IndividualRateCategory> category = FindCategory(tokens[0]))
            return SetCategoryRates(handler, *category, *value, account);

        return SendUnknownRate(handler);
    }

    static bool ResetRateOrCategory(ChatHandler* handler, Tail args, bool includeAccount)
    {
        if (!CheckModuleEnabled(handler))
            return false;

        std::vector<std::string_view> tokens = Acore::Tokenize(args, ' ', false);
        if (tokens.size() != 1)
        {
            handler->PSendSysMessage("Usage: .rate {}reset <key|category>", includeAccount ? "" : "character ");
            return false;
        }

        if (!includeAccount && !individualRates.CharacterRates)
        {
            handler->SendSysMessage("[Rates] Character-specific rates are disabled by server config.");
            return false;
        }

        if (Optional<IndividualRate> rate = FindRate(tokens[0]))
            return ResetSingleRate(handler, *rate, includeAccount);

        if (Optional<IndividualRateCategory> category = FindCategory(tokens[0]))
            return ResetCategoryRates(handler, *category, includeAccount);

        return SendUnknownRate(handler);
    }

    static bool SetSingleRate(ChatHandler* handler, IndividualRate rate, float value, bool account)
    {
        uint8 index = rate;
        if (!CanUseRate(handler, index))
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        float maxRate = GetMaxAllowedRate(player, rate);
        if (IsMaxLevelActive(player, rate) && value > maxRate)
        {
            handler->PSendSysMessage("[Rates] This rate is capped at {} by max-level config.", maxRate);
            return false;
        }

        float minRate = GetMinimumRate(rate);
        if (IsWholeNumberRate(rate) && std::floor(value) != value)
        {
            handler->PSendSysMessage("[Rates] {} only accepts whole numbers.", RateDefinitions[index].CommandKey);
            return false;
        }

        if (value < minRate || value > maxRate)
        {
            handler->PSendSysMessage("Rate must be between {} and {}.", minRate, maxRate);
            return false;
        }

        PlayerIndividualRates* data = GetRates(player);
        if (account)
        {
            data->AccountRate[index] = value;
            data->AccountEnabled[index] = true;
            data->AccountHasRate[index] = true;
            SaveAccountRate(player, index);
        }
        else
        {
            data->Rate[index] = value;
            data->Enabled[index] = true;
            data->CharacterHasRate[index] = true;
        }

        handler->PSendSysMessage("[Rates] {} set to {}{}.",
            RateDefinitions[index].CommandKey, value, account ? " for your account" : " for this character");
        return true;
    }

    static bool SetCategoryRates(ChatHandler* handler, IndividualRateCategory category, float value, bool account)
    {
        if (!individualRates.CategoryEnabled[category])
        {
            handler->PSendSysMessage("[Rates] {} is disabled by server config.",
                CategoryDefinitions[category].CommandKey);
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();
        PlayerIndividualRates* data = GetRates(player);
        uint8 changed = 0;

        for (uint8 i = 0; i < IR_MAX; ++i)
        {
            if (RateDefinitions[i].Category != category || !IsRateAvailable(IndividualRate(i)))
                continue;

            float maxRate = GetMaxAllowedRate(player, IndividualRate(i));
            if (IsMaxLevelActive(player, IndividualRate(i)) && value > maxRate)
                continue;

            if (IsWholeNumberRate(IndividualRate(i)) && std::floor(value) != value)
                continue;

            float minRate = GetMinimumRate(IndividualRate(i));
            if (value < minRate || value > maxRate)
                continue;

            if (account)
            {
                data->AccountRate[i] = value;
                data->AccountEnabled[i] = true;
                data->AccountHasRate[i] = true;
                SaveAccountRate(player, i);
            }
            else
            {
                data->Rate[i] = value;
                data->Enabled[i] = true;
                data->CharacterHasRate[i] = true;
            }
            ++changed;
        }

        if (!changed)
        {
            handler->SendSysMessage("[Rates] No rates in that category can use that value.");
            return false;
        }

        handler->PSendSysMessage("[Rates] {} rates set to {}{}.",
            CategoryDefinitions[category].CommandKey, value, account ? " for your account" : " for this character");
        return true;
    }

    static bool ResetSingleRate(ChatHandler* handler, IndividualRate rate, bool includeAccount)
    {
        uint8 index = rate;
        if (!CanUseRate(handler, index))
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        PlayerIndividualRates* data = GetRates(player);
        ResetCharacterRate(player, index);

        if (includeAccount)
            ResetAccountRate(player, index);

        handler->PSendSysMessage("[Rates] {} reset to {}.",
            RateDefinitions[index].CommandKey,
            includeAccount || !individualRates.AccountRates || !data->AccountHasRate[index] ?
                "server default" : "account rate");
        return true;
    }

    static bool ResetCategoryRates(ChatHandler* handler, IndividualRateCategory category, bool includeAccount)
    {
        if (!individualRates.CategoryEnabled[category])
        {
            handler->PSendSysMessage("[Rates] {} is disabled by server config.",
                CategoryDefinitions[category].CommandKey);
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();
        uint8 changed = 0;
        for (uint8 i = 0; i < IR_MAX; ++i)
        {
            if (RateDefinitions[i].Category != category || !IsRateAvailable(IndividualRate(i)))
                continue;

            ResetCharacterRate(player, i);
            if (includeAccount)
                ResetAccountRate(player, i);

            ++changed;
        }

        if (!changed)
        {
            handler->SendSysMessage("[Rates] No enabled rates found in that category.");
            return false;
        }

        handler->PSendSysMessage("[Rates] {} rates reset to {}.",
            CategoryDefinitions[category].CommandKey, includeAccount ? "server defaults" : "account rates/defaults");
        return true;
    }

    static void SaveAccountRate(Player* player, uint8 index)
    {
        PlayerIndividualRates* data = GetRates(player);
        CharacterDatabase.DirectExecute(
            "REPLACE INTO `account_individual_rates` (`AccountID`, `RateKey`, `RateValue`, `Enabled`) "
            "VALUES ({}, '{}', {}, {})",
            player->GetSession()->GetAccountId(), RateDefinitions[index].CommandKey, data->AccountRate[index],
            data->AccountEnabled[index] ? 1 : 0);
    }

    static void ResetAccountRate(Player* player, uint8 index)
    {
        PlayerIndividualRates* data = GetRates(player);
        data->AccountRate[index] = individualRates.DefaultRate[index];
        data->AccountEnabled[index] = true;
        data->AccountHasRate[index] = false;
        DeleteAccountRate(player, index);
    }

    static void ResetCharacterRate(Player* player, uint8 index)
    {
        PlayerIndividualRates* data = GetRates(player);
        data->Rate[index] = individualRates.DefaultRate[index];
        data->Enabled[index] = true;
        data->CharacterHasRate[index] = false;
        DeleteCharacterRate(player, index);
    }

    static void DeleteAccountRate(Player* player, uint8 index)
    {
        CharacterDatabase.DirectExecute(
            "DELETE FROM `account_individual_rates` WHERE `AccountID` = {} AND `RateKey` = '{}'",
            player->GetSession()->GetAccountId(), RateDefinitions[index].CommandKey);
    }

    static void DeleteCharacterRate(Player* player, uint8 index)
    {
        CharacterDatabase.DirectExecute(
            "DELETE FROM `individual_rates` WHERE `CharacterGUID` = {} AND `RateKey` = '{}'",
            player->GetGUID().GetCounter(), RateDefinitions[index].CommandKey);
    }

    static bool SetRateEnabled(ChatHandler* handler, Tail args, bool enabled, bool account)
    {
        if (!CheckModuleEnabled(handler))
            return false;

        if (!CheckStorageModeEnabled(handler, account))
            return false;

        std::vector<std::string_view> tokens = Acore::Tokenize(args, ' ', false);
        if (tokens.size() != 1)
        {
            handler->PSendSysMessage("Usage: .rate {}{} <key>",
                account ? "" : "character ", enabled ? "enable" : "disable");
            return false;
        }

        Optional<IndividualRate> rate = FindRate(tokens[0]);
        if (!rate)
            return SendUnknownRate(handler);

        uint8 index = *rate;
        if (!CanUseRate(handler, index))
            return false;

        PlayerIndividualRates* data = GetRates(handler->GetSession()->GetPlayer());
        if (account)
        {
            data->AccountEnabled[index] = enabled;
            data->AccountHasRate[index] = true;
            SaveAccountRate(handler->GetSession()->GetPlayer(), index);
        }
        else
        {
            data->Enabled[index] = enabled;
            data->CharacterHasRate[index] = true;
        }

        handler->PSendSysMessage("[Rates] {} {}{}.",
            RateDefinitions[index].CommandKey, enabled ? "enabled" : "disabled",
            account ? " for your account" : " for this character");
        return true;
    }

    static bool CheckStorageModeEnabled(ChatHandler* handler, bool account)
    {
        if (account)
        {
            if (individualRates.AccountRates)
                return true;

            handler->SendSysMessage("[Rates] Account-wide rates are disabled by server config.");
            return false;
        }

        if (individualRates.CharacterRates)
            return true;

        handler->SendSysMessage("[Rates] Character-specific rates are disabled by server config.");
        return false;
    }
};

void AddIndividualRatesScripts()
{
    new IndividualRatesWorldScript();
    new IndividualRatesPlayerScript();
    new IndividualRatesGlobalScript();
    new IndividualRatesLootMoneyScript();
    new IndividualRatesCommandScript();
}
