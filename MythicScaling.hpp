#pragma once

#include "MythicMgr.hpp"
#include <ctime>

class Creature;
class InstanceScript;

namespace Mythic
{
    //! wymyslic lepsza nazwe :/
    constexpr size_t MYTHIC_MAXIMUM_AMOUNT_OF_SPELLS{ 10 };
    using AffixCooldownMap = std::unordered_map<uint32/*id*/, std::time_t/*cooldown*/>;
};

static std::string MythicCreatureDataString =
"Base health: {}\n"
"Base mythic power: {}\n"
"Base minimum melee damage: {}\n"
"Base maximum melee damage: {}\n"
"Key variance melee: {}\n"
"Key variance stats: {}\n"
"Creature level: {}\n"
"Mythic template id: {}";

enum SpellBounceSpells
{
    SPELL_MYTHIC_BOUNCE_SHADOW  = 33335,
    SPELL_MYTHIC_BOUNCE_FIRE    = 30943,
    SPELL_MYTHIC_BOUNCE_NATURE  = 20698,
    SPELL_MYTHIC_BOUNCE_FROST   = 11,
    SPELL_MYTHIC_BOUNCE_ARCANE  = 38265
};

static std::vector<uint32> _spellBounceSpells
{
    SPELL_MYTHIC_BOUNCE_SHADOW,
    SPELL_MYTHIC_BOUNCE_FIRE,
    SPELL_MYTHIC_BOUNCE_NATURE,
    SPELL_MYTHIC_BOUNCE_FROST,
    SPELL_MYTHIC_BOUNCE_ARCANE
};

class MythicScaling
{
public:
    MythicScaling(Creature* _me, uint32 _mythicTemplateId);

    void Initialize();
    Mythic::MythicCreatureType const GetMythicType() const;
    void ScaleToKeyLevel();
    bool LoadCreatureAddon();
    void LoadAffixesForMyKeylevel();
    std::vector<Mythic::MythicSpells> const& GetAdditionalSpells() { return m_additionalSpells; }
    std::vector<Mythic::MythicAffixData> m_affixes;

    /** Mythic affixes procs **/
    void HandleAffixProc(uint32 procFlag, SpellInfo const* info = nullptr, Unit* victim = nullptr);
    void AffixProcEvent(uint32 affixId, SpellInfo const* info = nullptr, Unit* victim = nullptr);
    bool const IsSpellBounceSpell(uint32 id) const;
    bool const CanProcSpellBounce(SpellInfo const* /*info*/) const;

    void AddAffixCooldown(uint32 id, uint32 cooldown);
    bool HasAffixCooldown(uint32 id) const;
    void ResetCooldowns()
    {
        m_affixCooldowns.clear();
    }

    bool m_affixesAllowed;
    bool m_canBeRespawned;
    uint32 m_mythicTemplateId;
    uint32 m_additionalResistances;
    std::string ToString();
private:
    MythicScaling();

    Creature* me;
    Mythic::MythicCreatureType m_mythicType;

    std::vector<Mythic::MythicSpells> m_additionalSpells;
    Optional<Mythic::MythicCreatureData> m_scalingData;
    Mythic::AffixCooldownMap m_affixCooldowns;
    uint32 m_keyLevel;

    void HandleSpellBounce(SpellInfo const* info, Unit* victim);

protected:
    MythicScaling(MythicScaling const&) = delete;
    MythicScaling(MythicScaling&&) = delete;
    MythicScaling& operator=(MythicScaling const&) = delete;
    MythicScaling& operator=(MythicScaling&&) = delete;
};
