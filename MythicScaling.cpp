#include "MythicScaling.hpp"
#include "MythicMgr.hpp"
#include "InstanceScript.h"
#include "MapReference.h"
#include "Player.h"

#include "Util.h"
#include "Containers.h"
#include "fmt/format.h"

MythicScaling::MythicScaling(Creature* _me, uint32 _mythicTemplateId)
    : me(_me)
    , m_mythicTemplateId(_mythicTemplateId)
    , m_keyLevel(1)
{
    ASSERT(me);
    m_additionalSpells.reserve(Mythic::MYTHIC_MAXIMUM_AMOUNT_OF_SPELLS);
    m_mythicType = Mythic::CREATURE_TYPE_MAX;
    m_affixCooldowns.clear();
    m_affixesAllowed        = true;
    m_additionalResistances = 0;
    m_canBeRespawned        = true;
}

void MythicScaling::Initialize()
{
    m_scalingData = sMythicMgr.GetMythicTemplateFor(m_mythicTemplateId);

    if (!m_scalingData)
        return;

    m_mythicType = m_scalingData->CreatureType;
    m_additionalSpells.clear(); // unusued but leaving as is
    //m_additionalSpells = sMythicMgr.GetMythicSpellsFor(GetMythicType());
    LoadAffixesForMyKeylevel();

    sLog->outDebug(DebugLogFilters::LOG_FILTER_TSCR, "Creature (%u) initializing mythic scaling.", me->GetEntry());

    if (!me->GetDBTableGUIDLow() || me->isWorldBoss() || me->IsSummon())
        m_affixesAllowed = false;
}

void MythicScaling::ScaleToKeyLevel()
{
    if (!m_scalingData)
        return;

    InstanceScript* instance = me->GetInstanceScript();
    //! Creature outside of instance can't be mythic
    if (!instance)
        return;

    //! order matters, m_keyLevel has to be set before everything else because we can
    //! change the keylevel once creatures have been loaded once
    m_keyLevel = instance->GetMythicKeyLevel();
    //! order matters, call it first or UpdateAllStats() will break health values
    CreatureTemplate const* cInfo = me->GetCreatureTemplate();
    if (cInfo)
    {
        //! Recalculate armor values, we've changed our level
        CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(me->getLevel(), cInfo->unit_class);
        float armor = (float)stats->GenerateArmor(cInfo);
        float const armorStep = 100 * m_keyLevel;
        armor += armorStep;
        me->SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, armor);

        m_additionalResistances = 45 + static_cast<float>(5 * m_keyLevel);
        me->SetModifierValue(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE,      m_additionalResistances);
        me->SetModifierValue(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE,      m_additionalResistances);
        me->SetModifierValue(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE,    m_additionalResistances);
        me->SetModifierValue(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE,     m_additionalResistances);
        me->SetModifierValue(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE,    m_additionalResistances);
        me->SetModifierValue(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE,    m_additionalResistances);

        uint32 _baseMainAttackSpeed = cInfo->baseattacktime;
        uint32 _baseRangedAttackSpeed = cInfo->rangeattacktime;
        uint32 _newMainSpeed = std::max(1250U, _baseMainAttackSpeed -
            static_cast<uint32>(_baseMainAttackSpeed * 0.2f));
        uint32 _newRangedSpeed = std::max(1350U, _baseRangedAttackSpeed -
            static_cast<uint32>(_baseRangedAttackSpeed * 0.2f));

        me->SetAttackTime(WeaponAttackType::BASE_ATTACK, _newMainSpeed);
        me->SetAttackTime(WeaponAttackType::OFF_ATTACK, _newMainSpeed);
        me->SetAttackTime(WeaponAttackType::RANGED_ATTACK, _newRangedSpeed);
        me->UpdateAllStats();
    }

    float statVariance = static_cast<float>(m_keyLevel * m_scalingData->KeyVarianceStats);
    float meleeVariance = static_cast<float>(m_keyLevel * m_scalingData->KeyVarianceMelee);

    me->SetLevel(m_scalingData->CreatureLevel);
    /** stats **/
    uint32 mythicHealth = Mythic::CalculateMythicValue(m_scalingData->BaseMythicHealth, statVariance);
    if (!mythicHealth)
        mythicHealth = me->GetMaxHealth();

    float const modifier = (me->IsDungeonBoss() || me->isWorldBoss()) ? 0.2f : 0.3f;
    uint32 _additionalHealthBoost = static_cast<uint32>(mythicHealth * modifier);
    me->SetMaxHealth(mythicHealth + _additionalHealthBoost);
    me->SetFullHealth();

    Powers powerType = me->getPowerType();
    if (powerType == POWER_MANA)
    {
        uint32 maxPower = Mythic::CalculateMythicValue(m_scalingData->BaseMythicPower, statVariance);
        //! if mana is not set in database then get base mana values
        //! otherwise we will set mana to zero and disable mana bar alltogether
        if (!maxPower)
            maxPower = me->GetMaxPower(POWER_MANA);
        me->SetMaxPower(me->getPowerType(), maxPower);
        me->SetPower(me->getPowerType(), maxPower);
    }

    /** melee **/
    uint32 minDamage = Mythic::CalculateMythicValue(m_scalingData->MinRawMeleeDamage, meleeVariance);
    uint32 maxDamage = Mythic::CalculateMythicValue(m_scalingData->MaxRawMeleeDamage, meleeVariance);
    me->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, minDamage);
    me->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, maxDamage);

    me->SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, minDamage);
    me->SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, maxDamage);

    me->SetBaseWeaponDamage(RANGED_ATTACK, MINDAMAGE, minDamage);
    me->SetBaseWeaponDamage(RANGED_ATTACK, MAXDAMAGE, maxDamage);

    me->UpdateDamagePhysical(BASE_ATTACK);
    me->UpdateDamagePhysical(OFF_ATTACK);

    me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_TAUNT, true);
    me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_ATTACK_ME, true);
    me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_CHARM, true);
    me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_AOE_CHARM, true);
    me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_POSSESS, true);
    me->ApplySpellImmune(0, IMMUNITY_ID, 58994, true); // Mojo pool

    if (!me->GetDBTableGUIDLow() && me->GetSummoner())
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_GRIP, true);

    if (me->isWorldBoss() || me->IsDungeonBoss() || me->IsTrigger())
    {
        me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
        me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK_DEST, true);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_GRIP, true);
    }
}

Mythic::MythicCreatureType const MythicScaling::GetMythicType() const
{
    return m_mythicType;
}

bool MythicScaling::LoadCreatureAddon()
{
    return false;
}

void MythicScaling::LoadAffixesForMyKeylevel()
{
    sMythicMgr.GetMythicAffixesForCreature(m_affixes);
    //! erase affixes that do not meet requirements
    Trinity::Containers::Erase_if(m_affixes, [this](Mythic::MythicAffixData& aff)
    {
        return aff.AffixKeyRequirement > m_keyLevel;
    });

    if (m_mythicType == Mythic::CREATURE_TYPE_CASTER ||
        m_mythicType == Mythic::CREATURE_TYPE_HEALER)
    {
        Optional<Mythic::MythicAffixData const> data = sMythicMgr.GetSingleAffix(Mythic::AFFIX_SPELL_BOUNCE);
        if (data)
            m_affixes.push_back(data.get());
    }
}

void MythicScaling::HandleAffixProc(uint32 procFlag, SpellInfo const* info, Unit* victim)
{
    for (Mythic::MythicAffixData const& data : m_affixes)
    {
        if ((data.AffixFlags & procFlag) != 0)
            AffixProcEvent(data.AffixId, info, victim);
    }
}

constexpr uint32 SPELL_MYTHIC_AFFIX_MOJO_POOL{ 58994 };
void MythicScaling::AffixProcEvent(uint32 affixId, SpellInfo const* info , Unit* victim)
{
    if (!m_affixesAllowed)
        return;

    auto const& affix = std::find_if(m_affixes.begin(), m_affixes.end(),
        [&affixId](Mythic::MythicAffixData const& elem)
    {
        return affixId == elem.AffixId;
    });

    if (affix == m_affixes.end())
        return;

    bool _successfulProc = roll_chance_i(affix->AffixChance);
    if (!_successfulProc || HasAffixCooldown(affixId))
        return;

    switch (affixId)
    {
        case Mythic::AFFIX_BREAK_CC_IN_COMBAT:
        {
            if (!me->HasAura(90000))
                me->AddAura(90000, me);
            break;
        }
        case Mythic::AFFIX_BUFF_NEARBY_ON_DEATH:
        {
            if (me->GetDBTableGUIDLow())
                me->CastSpell(me, Mythic::SPELL_BUFF_NEARBY_CREATURES_MYTHIC, true);
            break;
        }
        case Mythic::AFFIX_BELOW_20_PCT_HEALTH_ENRAGE:
        {
            if (!me->HasAura(45111) || !me->HasAura(42006))
            {
                me->RemoveAurasDueToSpell(42006);
                me->RemoveAurasDueToSpell(56769);

                CustomSpellValues val;
                val.AddSpellMod(SPELLVALUE_BASE_POINT0, 100);
                val.AddSpellMod(SPELLVALUE_BASE_POINT1, 50);
                val.AddSpellMod(SPELLVALUE_BASE_POINT2, 100);
                me->CastCustomSpell(42006, val, me, TRIGGERED_FULL_MASK);

                val.AddSpellMod(SPELLVALUE_BASE_POINT0, 0);
                val.AddSpellMod(SPELLVALUE_BASE_POINT1, 0);
                val.AddSpellMod(SPELLVALUE_BASE_POINT2, 0);
                me->CastCustomSpell(45111, val, me, TRIGGERED_FULL_MASK);
            }
            break;
        }
        case Mythic::AFFIX_MOJO_POOL_ON_DEATH:
        {
            Creature* trig = me->SummonCreature(WORLD_TRIGGER, me->GetPosition());
            if (trig)
            {
                trig->setFaction(me->getFaction());
                trig->SetPassive();

                Mythic::MythicSpells data;
                ASSERT(sMythicMgr.GetSpellValuesFor(SPELL_MYTHIC_AFFIX_MOJO_POOL, data));
                float variance = float(m_keyLevel) * data.SpellValueVariance;

                CustomSpellValues val;
                val.AddSpellMod(SPELLVALUE_AURA_DURATION, data.AuraDuration);
                val.AddSpellMod(SPELLVALUE_RADIUS_MOD, 4000);
                val.AddSpellMod(SPELLVALUE_BASE_POINT0, Mythic::CalculateMythicValue(data.SpellEffectValue0, variance));
                me->CastCustomSpell(SPELL_MYTHIC_AFFIX_MOJO_POOL, val, (Unit*)nullptr, TRIGGERED_FULL_MASK);
                trig->SetObjectScale(1.5f);
                trig->DespawnOrUnsummon(13s);
            }

            break;
        }
        case Mythic::AFFIX_MORTAL_WOUND_ON_MELEE:
        {
            Unit* victim = me->GetVictim();
            if (!victim)
                break;

            CustomSpellValues val;
            val.AddSpellMod(SPELLVALUE_AURA_DURATION, 4500);
            me->CastCustomSpell(25646, val, victim, TRIGGERED_FULL_MASK);
            break;
        }
        case Mythic::AFFIX_SPELL_BOUNCE:
        {
            if (victim && info && victim != me)
            {
                if (IsSpellBounceSpell(info->Id) || !CanProcSpellBounce(info))
                    return;

                HandleSpellBounce(info, victim);
            }
            break;
        }
        default:
            sLog->outCrash("UNSUPPORTED MYTHIC AFFIX ID: %u", affixId);
            ASSERT(false);
            break;
    }

    AddAffixCooldown(affixId, affix->AffixCooldown);
}

void MythicScaling::AddAffixCooldown(uint32 affixId, uint32 time)
{
    std::time_t _cooldown = std::time(nullptr) + (time / 1000);
    m_affixCooldowns[affixId] = _cooldown;
}

bool MythicScaling::HasAffixCooldown(uint32 id) const
{
    auto const& it = m_affixCooldowns.find(id);
    if (it == m_affixCooldowns.end())
        return false;

    std::time_t _now = std::time(nullptr);
    std::time_t _cooldown = it->second;
    return it->second > _now;
}

bool const MythicScaling::IsSpellBounceSpell(uint32 spellId) const
{
    return std::any_of(_spellBounceSpells.begin(), _spellBounceSpells.end(), [spellId](uint32 id)
    {
        return id == spellId;
    });
}

bool const MythicScaling::CanProcSpellBounce(SpellInfo const* info) const
{
    if (!info)
        return false;

    return !info->IsPassive() && !info->HasAura(SPELL_AURA_PERIODIC_DAMAGE) && !info->IsTargetingArea();
}

void MythicScaling::HandleSpellBounce(SpellInfo const* info, Unit* victim)
{
    uint32 spellId = 0;
    uint32 mask = info->SchoolMask;

    switch (mask)
    {
        case SPELL_SCHOOL_MASK_SHADOW:
            spellId = SPELL_MYTHIC_BOUNCE_SHADOW;
            break;
        case SPELL_SCHOOL_MASK_FIRE:
            spellId = SPELL_MYTHIC_BOUNCE_FIRE;
            break;
        case SPELL_SCHOOL_MASK_NATURE:
            spellId = SPELL_MYTHIC_BOUNCE_NATURE;
            break;
        case SPELL_SCHOOL_MASK_FROST:
            spellId = SPELL_MYTHIC_BOUNCE_FROST;
            break;
        case SPELL_SCHOOL_MASK_ARCANE:
            spellId = SPELL_MYTHIC_BOUNCE_ARCANE;
            break;
        default:
            //me->MonsterYell("Unsupported school mask spell bounce", 0, nullptr);
            break;
    }

    auto const& refMap = me->GetMap()->GetPlayers();
    for (auto const& ref : refMap)
    {
        Player* player = ref.GetSource();
        if (!player || player->IsGameMaster() || player->GetDistance2d(victim) > 30.f || victim->GetGUID() == player->GetGUID())
            continue;

        CustomSpellValues val;
        val.AddSpellMod(SPELLVALUE_BASE_POINT0, 10000);
        val.AddSpellMod(SPELLVALUE_BASE_POINT1, 0);
        val.AddSpellMod(SPELLVALUE_BASE_POINT2, 0);
        val.AddSpellMod(SPELLVALUE_AURA_DURATION, 0);
        val.AddSpellMod(SPELLVALUE_SPELL_RANGE, 40.f);
        victim->CastCustomSpell(spellId, val, player, TRIGGERED_FULL_MASK);
    }
}

std::string MythicScaling::ToString()
{
    if (!m_scalingData)
        return "";

    return fmt::format(MythicCreatureDataString, m_scalingData->BaseMythicHealth, m_scalingData->BaseMythicPower,
        m_scalingData->MinRawMeleeDamage, m_scalingData->MaxRawMeleeDamage, m_scalingData->KeyVarianceMelee,
        m_scalingData->KeyVarianceStats, m_scalingData->CreatureLevel, m_mythicTemplateId);
}
