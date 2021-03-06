//
// This file is part of the WoW++ project.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
//

#include "pch.h"
#include "single_cast_state.h"
#include "game_character.h"
#include "game_creature.h"
#include "game_item.h"
#include "inventory.h"
#include "spell_cast.h"
#include "world_instance.h"
#include "each_tile_in_sight.h"
#include "universe.h"
#include "unit_mover.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	static UInt32 getLockId(const proto::ObjectEntry &entry)
	{
		UInt32 lockId = 0;

		switch (entry.type())
		{
			case 0:
			case 1:
				lockId = entry.data(1);
				break;
			case 2:
			case 3:
			case 6:
			case 10:
			case 12:
			case 13:
			case 24:
			case 26:
				lockId = entry.data(0);
				break;
			case 25:
				lockId = entry.data(4);
				break;
		}

		return lockId;
	}

	void SingleCastState::spellEffectAddComboPoints(const proto::SpellEffect &effect)
	{
		GameCharacter *character = nullptr;
		if (isPlayerGUID(m_cast.getExecuter().getGuid()))
		{
			character = dynamic_cast<GameCharacter *>(&m_cast.getExecuter());
		}

		if (!character)
		{
			ELOG("Invalid character");
			return;
		}

		m_affectedTargets.insert(character->shared_from_this());

		UInt64 comboTarget = m_target.getUnitTarget();
		character->addComboPoints(comboTarget, UInt8(calculateEffectBasePoints(effect)));
	}

	void SingleCastState::spellEffectDuel(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		if (!caster.isGameCharacter())
		{
			// This spell effect is only usable by player characters right now
			ELOG("Caster is not a game character!");
			return;
		}

		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		// Did we find at least one valid target?
		if (targets.empty())
		{
			WLOG("No targets found");
			return;
		}

		// Get the first target
		GameUnit *targetUnit = targets.front();
		ASSERT(targetUnit);

		// Check if target is a character
		if (!targetUnit->isGameCharacter())
		{
			// Skip this target
			WLOG("Target is not a character");
			return;
		}

		// Check if target is already dueling
		if (targetUnit->getUInt64Value(character_fields::DuelArbiter))
		{
			// Target is already dueling
			WLOG("Target is already dueling");
			return;
		}

		// Target is dead
		if (!targetUnit->isAlive())
		{
			WLOG("Target is dead");
			return;
		}

		// We affect this target
		m_affectedTargets.insert(targetUnit->shared_from_this());

		// Find flag object entry
		auto &project = targetUnit->getProject();
		const auto *entry = project.objects.getById(effect.miscvaluea());
		if (!entry)
		{
			ELOG("Could not find duel arbiter object: " << effect.miscvaluea());
			return;
		}

		// Spawn new duel arbiter flag
		if (auto *world = caster.getWorldInstance())
		{
			auto flagObject = world->spawnWorldObject(
				*entry,
				caster.getLocation(),
				0.0f,
				0.0f
			);
			flagObject->setUInt32Value(world_object_fields::AnimProgress, 0);
			flagObject->setUInt32Value(world_object_fields::Level, caster.getLevel());
			flagObject->setUInt32Value(world_object_fields::Faction, caster.getFactionTemplate().id());
			world->addGameObject(*flagObject);

			// Save this object to prevent it from being deleted immediatly
			caster.addWorldObject(flagObject);

			// Save them
			caster.setUInt64Value(character_fields::DuelArbiter, flagObject->getGuid());
			targetUnit->setUInt64Value(character_fields::DuelArbiter, flagObject->getGuid());
			DLOG("Duel arbiter spawned: " << flagObject->getGuid());
		}
	}

	void SingleCastState::spellEffectWeaponDamageNoSchool(const proto::SpellEffect &effect)
	{
		//TODO: Implement
		meleeSpecialAttack(effect, false);
	}

	void SingleCastState::spellEffectCreateItem(const proto::SpellEffect &effect)
	{
		// Get item entry
		const auto *item = m_cast.getExecuter().getProject().items.getById(effect.itemtype());
		if (!item)
		{
			ELOG("SPELL_EFFECT_CREATE_ITEM: Could not find item by id " << effect.itemtype());
			return;
		}

		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		bool wasCreated = false;

		m_attackTable.checkPositiveSpellNoCrit(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);
		const auto itemCount = calculateEffectBasePoints(effect);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			if (targetUnit->isGameCharacter())
			{
				auto *charUnit = reinterpret_cast<GameCharacter *>(targetUnit);
				wasCreated = createItems(reinterpret_cast<GameCharacter&>(*targetUnit), item->id(), itemCount);
			}
		}

		// Increase crafting skill eventually
		if (wasCreated && caster.isGameCharacter())
		{
			GameCharacter &casterChar = reinterpret_cast<GameCharacter&>(caster);
			if (m_spell.skill() != 0)
			{
				// Increase skill point if possible
				UInt16 current = 0, max = 0;
				if (casterChar.getSkillValue(m_spell.skill(), current, max))
				{
					const UInt32 yellowLevel = m_spell.trivialskilllow();
					const UInt32 greenLevel = m_spell.trivialskilllow() + (m_spell.trivialskillhigh() - m_spell.trivialskilllow()) / 2;
					const UInt32 grayLevel = m_spell.trivialskillhigh();

					// Determine current rank
					UInt32 successChance = 0;
					if (current < yellowLevel)
					{
						// Orange
						successChance = 100;
					}
					else if (current < greenLevel)
					{
						// Yellow
						successChance = 75;
					}
					else if (current < grayLevel)
					{
						// Green
						successChance = 25;
					}

					// Do we need to roll?
					if (successChance > 0 && successChance != 100)
					{
						// Roll
						std::uniform_int_distribution<UInt32> roll(0, 100);
						UInt32 val = roll(randomGenerator);
						if (val >= successChance)
						{
							// No luck - exit here
							return;
						}
					}
					else if (successChance == 0)
					{
						return;
					}

					// Increase spell value
					current = std::min<UInt16>(max, current + 1);
					casterChar.setSkillValue(m_spell.skill(), current, max);
				}
			}
		}
	}

	void SingleCastState::spellEffectWeaponDamage(const proto::SpellEffect &effect)
	{
		//TODO: Implement
		meleeSpecialAttack(effect, false);
	}

	void SingleCastState::spellEffectApplyAura(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		bool isPositive = (m_spell.positive() != 0);
		UInt8 school = m_spell.schoolmask();

		if (isPositive)
		{
			m_attackTable.checkPositiveSpellNoCrit(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);	//Buff
		}
		else
		{
			m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);	//Debuff
		}

		UInt32 aura = effect.aura();
		bool modifiedByBonus;
		switch (aura)
		{
			case game::aura_type::PeriodicDamage:
			case game::aura_type::PeriodicHeal:
			case game::aura_type::PeriodicLeech:
				modifiedByBonus = true;
			default:
				modifiedByBonus = false;
		}

		// World was already checked. If world would be nullptr, unitTarget would be null as well
		auto *world = m_cast.getExecuter().getWorldInstance();
		auto &universe = world->getUniverse();
		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			UInt32 totalPoints = 0;
			game::SpellMissInfo missInfo = game::spell_miss_info::None;
			bool spellFailed = false;

			if (hitInfos[i] == game::hit_info::Miss)
			{
				missInfo = game::spell_miss_info::Miss;
			}
			else if (victimStates[i] == game::victim_state::Evades)
			{
				missInfo = game::spell_miss_info::Evade;
			}
			else if (victimStates[i] == game::victim_state::IsImmune)
			{
				missInfo = game::spell_miss_info::Immune;
			}
			else if (victimStates[i] == game::victim_state::Normal)
			{
				if (resists[i] == 100.0f)
				{
					missInfo = game::spell_miss_info::Resist;
				}
				else
				{
					if (modifiedByBonus)
					{
						UInt32 spellPower = caster.getBonus(school);
						UInt32 spellBonusPct = caster.getBonusPct(school);
						totalPoints = getSpellPointsTotal(effect, spellPower, spellBonusPct);
						totalPoints -= totalPoints * (resists[i] / 100.0f);
					}
					else
					{
						totalPoints = calculateEffectBasePoints(effect);
					}

					if (effect.aura() == game::aura_type::PeriodicDamage &&
						m_spell.attributes(4) & game::spell_attributes_ex_d::StackDotModifier)
					{
						targetUnit->getAuras().forEachAuraOfType(game::aura_type::PeriodicDamage, [&totalPoints, this](AuraEffect &aura) -> bool
						{
							if (aura.getSlot().getSpell().id() == m_spell.id())
							{
								Int32 remainingTicks = aura.getMaxTickCount() - aura.getTickCount();
								Int32 remainingDamage = aura.getBasePoints() * remainingTicks;

								totalPoints += remainingDamage / aura.getMaxTickCount();
							}

							return true;
						});
					}
				}
			}

			if (missInfo != game::spell_miss_info::None)
			{
				m_completedEffectsExecution[targetUnit->getGuid()] = completedEffects.connect([this, &caster, targetUnit, school, missInfo]()
				{
					std::map<UInt64, game::SpellMissInfo> missedTargets;
					missedTargets[targetUnit->getGuid()] = missInfo;

					sendPacketFromCaster(caster,
						std::bind(game::server_write::spellLogMiss, std::placeholders::_1,
							m_spell.id(),
							caster.getGuid(),
							0,
							std::cref(missedTargets)));
				});	// End connect
			}
			else if (targetUnit->isAlive())
			{
				// Create a new slot for this unit if it didn't happen already
				if (m_auraSlots.find(targetUnit->getGuid()) == m_auraSlots.end())
				{
					m_auraSlots[targetUnit->getGuid()] = std::make_shared<AuraSpellSlot>(targetUnit->getTimers(), m_spell, m_itemGuid);
					m_auraSlots[targetUnit->getGuid()]->setOwner(std::static_pointer_cast<GameUnit>(targetUnit->shared_from_this()));
					m_auraSlots[targetUnit->getGuid()]->setCaster(std::static_pointer_cast<GameUnit>(m_cast.getExecuter().shared_from_this()));
				}

				// Get slot
				auto slot = m_auraSlots[targetUnit->getGuid()];
				ASSERT(slot);

				// Now, create an aura effect
				std::shared_ptr<AuraEffect> auraEffect = std::make_shared<AuraEffect>(*slot, effect, totalPoints, &caster, *targetUnit, m_target, false);

				const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
				if (!noThreat)
				{
					targetUnit->threaten(caster, 0.0f);
				}

				// TODO: Add aura to unit target
				if (isChanneled())
				{
					m_onChannelAuraRemoved = auraEffect->misapplied.connect([this]() {
						stopCast(game::spell_interrupt_flags::None);
					});
				}

				// Add to slot
				slot->addAuraEffect(auraEffect);

				// We need to be sitting for this aura to work
				if (m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::NotSeated)
				{
					// Sit down
					caster.setStandState(unit_stand_state::Sit);
				}
			}

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i]);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i]);
			}
		}

		// If auras should be removed on immunity, do so!
		if (aura == game::aura_type::MechanicImmunity &&
			(m_spell.attributes(1) & game::spell_attributes_ex_a::DispelAurasOnImmunity) != 0)
		{
			if (!m_removeAurasOnImmunity.connected())
			{
				m_removeAurasOnImmunity = completedEffects.connect([this] {
					UInt32 immunityMask = 0;
					for (Int32 i = 0; i < m_spell.effects_size(); ++i)
					{
						if (m_spell.effects(i).type() == game::spell_effects::ApplyAura &&
							m_spell.effects(i).aura() == game::aura_type::MechanicImmunity)
						{
							immunityMask |= (1 << m_spell.effects(i).miscvaluea());
						}
					}

					for (auto &target : m_affectedTargets)
					{
						auto strong = target.lock();
						if (strong)
						{
							auto unit = std::static_pointer_cast<GameUnit>(strong);
							unit->getAuras().removeAllAurasDueToMechanic(immunityMask);
						}
					}
				});
			}
		}
	}

	void SingleCastState::spellEffectPersistentAreaAura(const proto::SpellEffect & effect)
	{
		// Check targets
		GameUnit &caster = m_cast.getExecuter();
		if (!m_target.hasDestTarget())
		{
			WLOG("SPELL_EFFECT_APPLY_AREA_AURA: No dest target info found!");
			return;
		}

		math::Vector3 dstLoc;
		m_target.getDestLocation(dstLoc.x, dstLoc.y, dstLoc.z);

		static UInt64 lowGuid = 1;

		// Create a new dynamic object
		auto dynObj = std::make_shared<DynObject>(
			caster.getProject(),
			caster.getTimers(),
			caster,
			m_spell,
			effect
			);
		// TODO: Add lower guid counter
		auto guid = createEntryGUID(lowGuid++, m_spell.id(), guid_type::Player);
		dynObj->setGuid(guid);
		dynObj->relocate(dstLoc, 0.0f, false);
		dynObj->initialize();

		// Remember to destroy this object on end of channeling
		if (isChanneled())
		{
			m_dynObjectsToDespawn.push_back(guid);
		}
		else
		{
			// Timed despawn
			dynObj->triggerDespawnTimer(m_spell.duration());
		}

		if (effect.amplitude())
		{
			dynObj->startUnitWatcher();
			spellEffectApplyAura(effect);
		}

		// Add this object to the unit (this will also sawn it)
		caster.addDynamicObject(dynObj);

		if (m_hitResults.find(caster.getGuid()) == m_hitResults.end())
		{
			HitResult procInfo(m_attackerProc, m_victimProc, game::hit_info::NoAction, game::victim_state::Normal);
			m_hitResults.emplace(caster.getGuid(), procInfo);
		}
		else
		{
			HitResult &procInfo = m_hitResults.at(caster.getGuid());
			procInfo.add(game::hit_info::NoAction, game::victim_state::Normal);
		}
	}

	void SingleCastState::spellEffectHeal(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);		//Buff, HoT

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			UInt8 school = m_spell.schoolmask();
			UInt32 totalPoints;
			bool crit = false;
			if (victimStates[i] == game::victim_state::IsImmune)
			{
				totalPoints = 0;
			}
			else
			{
				UInt32 spellPower = caster.getBonus(school);
				UInt32 spellBonusPct = caster.getBonusPct(school);
				totalPoints = getSpellPointsTotal(effect, spellPower, spellBonusPct);

				// Only apply healing done bonus if not caused by an item
				if (m_itemGuid == 0)
				{
					caster.applyHealingDoneBonus(m_spell.spelllevel(), caster.getLevel(), 1, totalPoints);
				}

				// Always apply healing taken bonus
				targetUnit->applyHealingTakenBonus(1, totalPoints);

				if (hitInfos[i] == game::hit_info::CriticalHit)
				{
					crit = true;
					totalPoints *= 2.0f;
				}
			}

			// Update health value
			const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
			if (targetUnit->heal(totalPoints, &caster, noThreat))
			{
				// Send spell heal packet
				sendPacketFromCaster(caster,
					std::bind(game::server_write::spellHealLog, std::placeholders::_1,
						targetUnit->getGuid(),
						caster.getGuid(),
						m_spell.id(),
						totalPoints,
						crit));
			}

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i], totalPoints);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i], totalPoints);
			}
		}
	}

	void SingleCastState::spellEffectBind(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;

		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			if (targetUnit->isGameCharacter())
			{
				GameCharacter *character = dynamic_cast<GameCharacter *>(targetUnit);
				character->setHome(caster.getMapId(), caster.getLocation(), caster.getOrientation());
			}
		}
	}

	void SingleCastState::spellEffectQuestComplete(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;

		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			if (targetUnit->isGameCharacter())
			{
				reinterpret_cast<GameCharacter*>(targetUnit)->completeQuest(effect.miscvaluea());
			}
		}
	}

	void SingleCastState::spellEffectTriggerSpell(const proto::SpellEffect &effect)
	{
		if (!effect.triggerspell())
		{
			WLOG("Spell " << m_spell.id() << ": No spell to trigger found! Trigger effect will be ignored.");
			return;
		}

		GameUnit &caster = m_cast.getExecuter();
		caster.castSpell(m_target, effect.triggerspell(), { 0, 0, 0 }, 0, true);
	}

	void SingleCastState::spellEffectEnergize(const proto::SpellEffect &effect)
	{
		Int32 powerType = effect.miscvaluea();
		if (powerType < 0 || powerType > 5) {
			return;
		}

		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);		//Buff, HoT

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			UInt32 power = calculateEffectBasePoints(effect);
			if (victimStates[i] == game::victim_state::IsImmune)
			{
				power = 0;
			}

			UInt32 curPower = targetUnit->getUInt32Value(unit_fields::Power1 + powerType);
			UInt32 maxPower = targetUnit->getUInt32Value(unit_fields::MaxPower1 + powerType);
			if (curPower + power > maxPower)
			{
				curPower = maxPower;
			}
			else
			{
				curPower += power;
			}
			targetUnit->setUInt32Value(unit_fields::Power1 + powerType, curPower);
			sendPacketFromCaster(m_cast.getExecuter(),
				std::bind(game::server_write::spellEnergizeLog, std::placeholders::_1,
					m_cast.getExecuter().getGuid(), targetUnit->getGuid(), m_spell.id(), static_cast<UInt8>(powerType), power));

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i]);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i]);
			}
		}
	}

	void SingleCastState::spellEffectPowerBurn(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			UInt8 school = m_spell.schoolmask();
			Int32 powerType = effect.miscvaluea();
			UInt32 burn;
			UInt32 damage = 0;
			UInt32 resisted = 0;
			UInt32 absorbed = 0;
			if (victimStates[i] == game::victim_state::IsImmune)
			{
				burn = 0;
			}
			if (hitInfos[i] == game::hit_info::Miss)
			{
				burn = 0;
			}
			else
			{
				burn = calculateEffectBasePoints(effect);
				resisted = burn * (resists[i] / 100.0f);
				burn -= resisted;
				burn = 0 - targetUnit->addPower(game::PowerType(powerType), 0 - burn);
				damage = burn * effect.multiplevalue();
				absorbed = targetUnit->consumeAbsorb(damage, school);
			}

			// Update health value
			const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
			float threat = noThreat ? 0.0f : damage - absorbed;
			if (!noThreat && m_cast.getExecuter().isGameCharacter())
			{
				reinterpret_cast<GameCharacter&>(m_cast.getExecuter()).applySpellMod(spell_mod_op::Threat, m_spell.id(), threat);
			}
			if (targetUnit->dealDamage(damage - absorbed, school, game::DamageType::Direct, &caster, threat))
			{
				// Send spell damage packet
				sendPacketFromCaster(caster,
					std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
						targetUnit->getGuid(),
						caster.getGuid(),
						m_spell.id(),
						damage,
						school,
						absorbed,
						resisted,
						false,
						0,
						false));	//crit
			}

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i], damage, absorbed, damage ? true : false);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i], damage, absorbed, damage ? true : false);
			}
		}
	}


	void SingleCastState::spellEffectWeaponPercentDamage(const proto::SpellEffect &effect)
	{
		meleeSpecialAttack(effect, true);
	}

	void SingleCastState::spellEffectOpenLock(const proto::SpellEffect &effect)
	{
		// Try to get the target
		WorldObject *obj = nullptr;
		if (!m_target.hasGOTarget())
		{
			DLOG("TODO: SPELL_EFFECT_OPEN_LOCK without GO target");
			return;
		}

		auto *world = m_cast.getExecuter().getWorldInstance();
		if (!world)
		{
			return;
		}

		GameObject *raw = world->findObjectByGUID(m_target.getGOTarget());
		if (!raw)
		{
			WLOG("SPELL_EFFECT_OPEN_LOCK: Could not find target object");
			return;
		}

		if (!raw->isWorldObject())
		{
			WLOG("SPELL_EFFECT_OPEN_LOCK: Target object is not a world object");
			return;
		}

		obj = reinterpret_cast<WorldObject*>(raw);
		m_affectedTargets.insert(obj->shared_from_this());

		UInt32 currentState = obj->getUInt32Value(world_object_fields::State);

		const auto &entry = obj->getEntry();
		UInt32 lockId = getLockId(entry);
		DLOG("Lock id: " << lockId);

		// TODO: Get lock info

		// We need to consume eventual cast items NOW before we send the loot packet to the client
		// If we remove the item afterwards, the client will reject the loot dialog and request a 
		// close immediatly. Don't worry, consumeItem may be called multiple times: It will only 
		// remove the item once
		if (!consumeItem(false))
		{
			return;
		}

		// If it is a door, try to open it
		switch (entry.type())
		{
			case world_object_type::Door:
			case world_object_type::Button:
				obj->setUInt32Value(world_object_fields::State, (currentState == 1 ? 0 : 1));
				break;
			case world_object_type::Chest:
			{
				obj->setUInt32Value(world_object_fields::State, (currentState == 1 ? 0 : 1));

				// Open chest loot window
				auto loot = obj->getObjectLoot();
				if (loot && !loot->isEmpty())
				{
					// Start inspecting the loot
					if (m_cast.getExecuter().isGameCharacter())
					{
						GameCharacter *character = reinterpret_cast<GameCharacter *>(&m_cast.getExecuter());
						character->lootinspect(loot);
					}
				}
				break;
			}
			default:	// Make the compiler happy
				break;
		}

		if (m_cast.getExecuter().isGameCharacter())
		{
			GameCharacter *character = reinterpret_cast<GameCharacter *>(&m_cast.getExecuter());
			character->onQuestObjectCredit(m_spell.id(), *obj);
			character->objectInteraction(*obj);
		}

		// Raise interaction triggers
		obj->raiseTrigger(trigger_event::OnInteraction);
	}

	void SingleCastState::spellEffectApplyAreaAuraParty(const proto::SpellEffect &effect)
	{

	}

	void SingleCastState::spellEffectDispel(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		UInt8 school = m_spell.schoolmask();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			UInt32 totalPoints = 0;
			bool spellFailed = false;

			if (hitInfos[i] == game::hit_info::Miss)
			{
				spellFailed = true;
			}
			else if (victimStates[i] == game::victim_state::IsImmune)
			{
				spellFailed = true;
			}
			else if (victimStates[i] == game::victim_state::Normal)
			{
				if (resists[i] == 100.0f)
				{
					spellFailed = true;
				}
				else
				{
					totalPoints = calculateEffectBasePoints(effect);
				}
			}

			if (spellFailed)
			{
				// TODO send fail packet
				sendPacketFromCaster(caster,
					std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
						targetUnit->getGuid(),
						caster.getGuid(),
						m_spell.id(),
						1,
						school,
						0,
						1,	//resisted
						false,
						0,
						false));
			}
			else if (targetUnit->isAlive())
			{
				UInt32 auraDispelType = effect.miscvaluea();
				targetUnit->getAuras().removeAurasDueToDispel(auraDispelType, totalPoints);
			}

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i]);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i]);
			}
		}
	}

	void SingleCastState::spellEffectSummon(const proto::SpellEffect &effect)
	{
		const auto *entry = m_cast.getExecuter().getProject().units.getById(effect.summonunit());
		if (!entry)
		{
			WLOG("Can't summon anything - missing entry");
			return;
		}

		GameUnit &executer = m_cast.getExecuter();
		auto *world = executer.getWorldInstance();
		if (!world)
		{
			WLOG("Could not find world instance!");
			return;
		}

		float o = executer.getOrientation();
		math::Vector3 location(executer.getLocation());

		auto spawned = world->spawnSummonedCreature(*entry, location, o);
		if (!spawned)
		{
			ELOG("Could not spawn creature!");
			return;
		}

		spawned->setUInt64Value(unit_fields::SummonedBy, executer.getGuid());
		world->addGameObject(*spawned);

		if (executer.getVictim())
		{
			spawned->threaten(*executer.getVictim(), 0.0001f);
		}
	}

	void SingleCastState::spellEffectSummonPet(const proto::SpellEffect & effect)
	{
		GameUnit &executer = m_cast.getExecuter();

		// Check if caster already have a pet
		UInt64 petGUID = executer.getUInt64Value(unit_fields::Summon);
		if (petGUID != 0)
		{
			// Already have a pet!
			return;
		}

		// Get the pet unit entry
		const auto *entry = executer.getProject().units.getById(effect.miscvaluea());
		if (!entry)
		{
			WLOG("Can't summon pet - missing entry");
			return;
		}

		auto *world = executer.getWorldInstance();
		if (!world)
		{
			return;
		}

		float o = executer.getOrientation();
		math::Vector3 location(executer.getLocation());

		auto spawned = world->spawnSummonedCreature(*entry, location, o);
		if (!spawned)
		{
			ELOG("Could not spawn creature!");
			return;
		}

		spawned->setUInt64Value(unit_fields::SummonedBy, executer.getGuid());
		spawned->setFactionTemplate(executer.getFactionTemplate());
		spawned->setLevel(executer.getLevel());		// TODO
		executer.setUInt64Value(unit_fields::Summon, spawned->getGuid());
		spawned->setUInt32Value(unit_fields::CreatedBySpell, m_spell.id());
		spawned->setUInt64Value(unit_fields::CreatedBy, executer.getGuid());
		spawned->setUInt32Value(unit_fields::NpcFlags, 0);
		spawned->setUInt32Value(unit_fields::Bytes1, 0);
		spawned->setUInt32Value(unit_fields::PetNumber, guidLowerPart(spawned->getGuid()));
		world->addGameObject(*spawned);

		if (m_hitResults.find(spawned->getGuid()) == m_hitResults.end())
		{
			HitResult procInfo(m_attackerProc, m_victimProc, game::hit_info::NoAction, game::victim_state::Normal);
			m_hitResults.emplace(spawned->getGuid(), procInfo);
		}
		else
		{
			HitResult &procInfo = m_hitResults.at(spawned->getGuid());
			procInfo.add(game::hit_info::NoAction, game::victim_state::Normal);
		}
	}

	void SingleCastState::spellEffectCharge(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		if (!targets.empty())
		{
			GameUnit &firstTarget = *targets[0];
			m_affectedTargets.insert(firstTarget.shared_from_this());

			// TODO: Error checks and limit max path length
			auto &mover = caster.getMover();

			const float orientation = firstTarget.getAngle(caster);
			const math::Vector3 target =
				firstTarget.getMover().getCurrentLocation().getRelativePosition(orientation, 
					firstTarget.getMeleeReach() + caster.getMeleeReach());
			mover.moveTo(target, 35.0f);

			if (m_hitResults.find(firstTarget.getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[0], victimStates[0], resists[0]);
				m_hitResults.emplace(firstTarget.getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(firstTarget.getGuid());
				procInfo.add(hitInfos[0], victimStates[0], resists[0]);
			}
		}
	}

	void SingleCastState::spellEffectAttackMe(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			GameUnit *topThreatener = targetUnit->getTopThreatener().value();
			if (topThreatener)
			{
				float addThread = targetUnit->getThreat(*topThreatener).value();
				addThread -= targetUnit->getThreat(caster).value();
				if (addThread > 0.0f) {
					targetUnit->threaten(caster, addThread);
				}
			}

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i]);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i]);
			}
		}
	}

	void SingleCastState::spellEffectScript(const proto::SpellEffect &effect)
	{
		switch (m_spell.id())
		{
			case 20271:	// Judgment
						// aura = get active seal from aura_container (Dummy-effect)
						// m_cast.getExecuter().castSpell(target, aura.getBasePoints(), -1, 0, false);
				break;
			default:
				break;
		}
	}


	void SingleCastState::spellEffectDispelMechanic(const proto::SpellEffect & effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			targetUnit->getAuras().removeAllAurasDueToMechanic(1 << effect.miscvaluea());

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i]);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i]);
			}
		}
	}

	void SingleCastState::spellEffectResurrect(const proto::SpellEffect & effect)
	{
		if (!isPlayerGUID(m_target.getUnitTarget()))
		{
			return;
		}

		auto *world = m_cast.getExecuter().getWorldInstance();

		if (!world)
		{
			return;
		}

		GameUnit &caster = m_cast.getExecuter();
		GameUnit *targetUnit = nullptr;
		m_target.resolvePointers(*world, &targetUnit, nullptr, nullptr, nullptr);

		m_affectedTargets.insert(targetUnit->shared_from_this());

		if (!targetUnit || targetUnit->isAlive())
		{
			return;
		}

		auto *target = reinterpret_cast<GameCharacter *>(targetUnit);

		if (target->isResurrectRequested())
		{
			return;
		}

		UInt32 health = target->getUInt32Value(unit_fields::MaxHealth) * calculateEffectBasePoints(effect) / 100;
		UInt32 mana = target->getUInt32Value(unit_fields::Power1) * calculateEffectBasePoints(effect) / 100;

		target->setResurrectRequestData(caster.getGuid(), caster.getMapId(), caster.getLocation(), health, mana);
		target->resurrectRequested(caster.getGuid(), caster.getName(), caster.isGameCharacter() ? object_type::Character : object_type::Unit);

		if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
		{
			HitResult procInfo(m_attackerProc, m_victimProc, game::hit_info::NoAction, game::victim_state::Normal);
			m_hitResults.emplace(targetUnit->getGuid(), procInfo);
		}
		else
		{
			HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
			procInfo.add(game::hit_info::NoAction, game::victim_state::Normal);
		}
	}

	void SingleCastState::spellEffectResurrectNew(const proto::SpellEffect & effect)
	{
		if (!isPlayerGUID(m_target.getUnitTarget()))
		{
			return;
		}

		auto *world = m_cast.getExecuter().getWorldInstance();

		if (!world)
		{
			return;
		}

		GameUnit &caster = m_cast.getExecuter();
		GameUnit *targetUnit = nullptr;
		m_target.resolvePointers(*world, &targetUnit, nullptr, nullptr, nullptr);

		m_affectedTargets.insert(targetUnit->shared_from_this());

		if (!targetUnit || targetUnit->isAlive())
		{
			return;
		}

		auto *target = reinterpret_cast<GameCharacter *>(targetUnit);

		if (target->isResurrectRequested())
		{
			return;
		}

		UInt32 health = calculateEffectBasePoints(effect);
		UInt32 mana = effect.miscvaluea();

		target->setResurrectRequestData(caster.getGuid(), caster.getMapId(), caster.getLocation(), health, mana);
		target->resurrectRequested(caster.getGuid(), caster.getName(), caster.isGameCharacter() ? object_type::Character : object_type::Unit);

		if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
		{
			HitResult procInfo(m_attackerProc, m_victimProc, game::hit_info::NoAction, game::victim_state::Normal);
			m_hitResults.emplace(targetUnit->getGuid(), procInfo);
		}
		else
		{
			HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
			procInfo.add(game::hit_info::NoAction, game::victim_state::Normal);
		}
	}

	void SingleCastState::spellEffectKnockBack(const proto::SpellEffect & effect)
	{
		if (!isPlayerGUID(m_target.getUnitTarget()))
		{
			WLOG("TODO: KnockBack on creatures");
			return;
		}

		auto *world = m_cast.getExecuter().getWorldInstance();
		if (!world)
		{
			return;
		}

		GameUnit *targetUnit = nullptr;
		m_target.resolvePointers(*world, &targetUnit, nullptr, nullptr, nullptr);

		if (targetUnit->isRootedForSpell() || targetUnit->isStunned())
		{
			return;
		}

		m_affectedTargets.insert(targetUnit->shared_from_this());

		GameUnit &caster = m_cast.getExecuter();

		float speedxy = static_cast<float>(effect.miscvaluea() * 0.1f);
		float speedz = static_cast<float>(calculateEffectBasePoints(effect) * 0.1f);

		if (speedxy < 0.1f && speedz < 0.1f)
		{
			return;
		}

		float angle = targetUnit->getGuid() == caster.getGuid() ? caster.getOrientation() : caster.getAngle(*targetUnit);
		float vcos = std::cos(angle);
		float vsin = std::sin(angle);

		targetUnit->cancelCast(game::spell_interrupt_flags::Movement);

		sendPacketToCaster(*targetUnit,
			std::bind(game::server_write::moveKnockBack, std::placeholders::_1,
				targetUnit->getGuid(),
				vcos,
				vsin,
				speedxy,
				speedz));

		if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
		{
			HitResult procInfo(m_attackerProc, m_victimProc, game::hit_info::NoAction, game::victim_state::Normal);
			m_hitResults.emplace(targetUnit->getGuid(), procInfo);
		}
		else
		{
			HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
			procInfo.add(game::hit_info::NoAction, game::victim_state::Normal);
		}
	}

	void SingleCastState::spellEffectSkill(const proto::SpellEffect & effect)
	{

	}

	void SingleCastState::spellEffectDrainPower(const proto::SpellEffect &effect)
	{
		// Calculate the power to drain
		UInt32 powerToDrain = calculateEffectBasePoints(effect);
		Int32 powerType = effect.miscvaluea();

		// Resolve GUIDs
		GameObject *target = nullptr;
		GameUnit *unitTarget = nullptr;
		GameUnit &caster = m_cast.getExecuter();
		auto *world = caster.getWorldInstance();

		if (m_target.getTargetMap() == game::spell_cast_target_flags::Self) {
			target = &caster;
		}
		else if (world)
		{
			UInt64 targetGuid = 0;
			if (m_target.hasUnitTarget()) {
				targetGuid = m_target.getUnitTarget();
			}
			else if (m_target.hasGOTarget()) {
				targetGuid = m_target.getGOTarget();
			}
			else if (m_target.hasItemTarget()) {
				targetGuid = m_target.getItemTarget();
			}

			if (targetGuid != 0) {
				target = world->findObjectByGUID(targetGuid);
			}

			if (m_target.hasUnitTarget() && isUnitGUID(targetGuid)) {
				unitTarget = reinterpret_cast<GameUnit *>(target);
			}
		}

		// Check target
		if (!unitTarget)
		{
			WLOG("EFFECT_POWER_DRAIN: No valid target found!");
			return;
		}

		m_affectedTargets.insert(unitTarget->shared_from_this());
		unitTarget->threaten(caster, 0.0f);

		// Does this have any effect on the target?
		if (unitTarget->getByteValue(unit_fields::Bytes0, 3) != powerType) {
			return;    // Target does not use this kind of power
		}
		if (powerToDrain == 0) {
			return;    // Nothing to drain...
		}

		UInt32 currentPower = unitTarget->getUInt32Value(unit_fields::Power1 + powerType);
		if (currentPower == 0) {
			return;    // Target doesn't have any power left
		}

		// Now drain the power
		if (powerToDrain > currentPower) {
			powerToDrain = currentPower;
		}

		// Remove power
		unitTarget->setUInt32Value(unit_fields::Power1 + powerType, currentPower - powerToDrain);

		// If mana was drain, give the same amount of mana to the caster (or energy, if the caster does
		// not use mana)
		if (powerType == game::power_type::Mana)
		{
			// Give the same amount of power to the caster, but convert it to energy if needed
			UInt8 casterPowerType = caster.getByteValue(unit_fields::Bytes0, 3);
			if (casterPowerType != powerType)
			{
				// Only mana will be given
				return;
			}

			// Send spell damage packet
			sendPacketFromCaster(caster,
				std::bind(game::server_write::spellEnergizeLog, std::placeholders::_1,
					caster.getGuid(),
					caster.getGuid(),
					m_spell.id(),
					casterPowerType,
					powerToDrain));

			// Modify casters power values
			UInt32 casterPower = caster.getUInt32Value(unit_fields::Power1 + casterPowerType);
			UInt32 maxCasterPower = caster.getUInt32Value(unit_fields::MaxPower1 + casterPowerType);
			if (casterPower + powerToDrain > maxCasterPower) {
				powerToDrain = maxCasterPower - casterPower;
			}
			caster.setUInt32Value(unit_fields::Power1 + casterPowerType, casterPower + powerToDrain);
		}

		if (m_hitResults.find(unitTarget->getGuid()) == m_hitResults.end())
		{
			HitResult procInfo(m_attackerProc, m_victimProc, game::hit_info::NoAction, game::victim_state::Normal);
			m_hitResults.emplace(unitTarget->getGuid(), procInfo);
		}
		else
		{
			HitResult &procInfo = m_hitResults.at(unitTarget->getGuid());
			procInfo.add(game::hit_info::NoAction, game::victim_state::Normal);
		}
	}

	void SingleCastState::spellEffectProficiency(const proto::SpellEffect &effect)
	{
		// Self target
		GameCharacter *character = nullptr;
		if (isPlayerGUID(m_cast.getExecuter().getGuid()))
		{
			character = dynamic_cast<GameCharacter *>(&m_cast.getExecuter());
		}

		if (!character)
		{
			WLOG("SPELL_EFFECT_PROFICIENCY: Requires character unit target!");
			return;
		}

		m_affectedTargets.insert(character->shared_from_this());

		const UInt32 mask = m_spell.itemsubclassmask();
		if (m_spell.itemclass() == 2 && !(character->getWeaponProficiency() & mask))
		{
			character->addWeaponProficiency(mask);
		}
		else if (m_spell.itemclass() == 4 && !(character->getArmorProficiency() & mask))
		{
			character->addArmorProficiency(mask);
		}
	}

	void SingleCastState::spellEffectInstantKill(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			if (victimStates[i] != game::victim_state::Evades)
			{
				m_affectedTargets.insert(targetUnit->shared_from_this());

				targetUnit->dealDamage(targetUnit->getUInt32Value(unit_fields::Health), m_spell.schoolmask(), game::DamageType::Direct, &caster, 0.0f);
			}

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i], targetUnit->getUInt32Value(unit_fields::Health), 0, true);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i], targetUnit->getUInt32Value(unit_fields::Health), 0, true);
			}
		}
	}

	void SingleCastState::spellEffectDummy(const proto::SpellEffect & effect)
	{
		// Get unit target by target map
		GameUnit *unitTarget = nullptr;

		if (!m_target.resolvePointers(*m_cast.getExecuter().getWorldInstance(), &unitTarget, nullptr, nullptr, nullptr))
		{
			return;
		}

		if (unitTarget)
		{
			m_affectedTargets.insert(unitTarget->shared_from_this());
		}

		GameUnit &caster = m_cast.getExecuter();

		switch (m_spell.family())
		{
			case game::spell_family::Generic:
			{
				// Berserking (racial)
				if (m_spell.id() == 20554 ||
					m_spell.id() == 26296 ||
					m_spell.id() == 26297)
				{
					float health = static_cast<float>(caster.getUInt32Value(unit_fields::Health));
					float maxHealth = static_cast<float>(caster.getUInt32Value(unit_fields::MaxHealth));
					UInt32 healthPct = static_cast<UInt32>(health / maxHealth * 100);

					Int32 speedMod = 10;
					if (healthPct <= 40)
					{
						speedMod = 30;
					}
					else if (healthPct < 100 && healthPct > 40)
					{
						speedMod = 10 + (100 - healthPct) / 3;
					}

					game::SpellPointsArray basePoints;
					basePoints.fill(speedMod);
					SpellTargetMap targetMap;
					targetMap.m_targetMap = game::spell_cast_target_flags::Self;
					targetMap.m_unitTarget = caster.getGuid();

					caster.addFlag(unit_fields::AuraState, game::aura_state::Berserking);
					caster.castSpell(std::move(targetMap), 26635, std::move(basePoints), 0, true);
				}
				break;
			}
			case game::spell_family::Warrior:
			{
				if (m_spell.familyflags() & 0x20000000)		// Execute
				{
					spellScriptEffectExecute(effect);
				}
				break;
			}
			case game::spell_family::Druid:
			{
				if (m_spell.id() == 5229)		// Enrage
				{
					spellScriptEffectEnrage(effect);
				}
				break;
			}
			case game::spell_family::Warlock:
			{
				// Life Tap
				if (m_spell.familyflags() & 0x0000000000040000ULL)
				{
					spellScriptEffectLifeTap(effect);
				}
				break;
			}
		}

		// Lys test spell
		if (m_spell.id() == 5581)
		{
			// Show targets aura infos
			caster.getAuras().logAuraInfos();
		}
	}

	void SingleCastState::spellEffectTeleportUnits(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		UInt32 targetMap = 0;
		math::Vector3 targetPos(0.0f, 0.0f, 0.0f);
		float targetO = 0.0f;
		switch (effect.targetb())
		{
			case game::targets::DstHome:
			{
				if (caster.isGameCharacter())
				{
					GameCharacter *character = dynamic_cast<GameCharacter *>(&caster);
					character->getHome(targetMap, targetPos, targetO);
				}
				else
				{
					WLOG("Only characters do have a home point");
					return;
				}
				break;
			}
			case game::targets::DstDB:
			{
				targetMap = m_spell.targetmap();
				targetPos.x = m_spell.targetx();
				targetPos.y = m_spell.targety();
				targetPos.z = m_spell.targetz();
				targetO = m_spell.targeto();
				break;
			}
			case game::targets::DstCaster:
				targetMap = caster.getMapId();
				targetPos = caster.getLocation();
				targetO = caster.getOrientation();
				break;
			default:
				WLOG("Unhandled destination type " << effect.targetb() << " - not teleporting!");
				return;
		}

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			if (targetUnit->isGameCharacter())
			{
				targetUnit->teleport(targetMap, targetPos, targetO);
			}
			else if (targetUnit->getMapId() == targetMap)
			{
				// Simply relocate creatures and other stuff
				targetUnit->relocate(targetPos, targetO);
			}

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i]);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i]);
			}
		}
	}

	void SingleCastState::spellEffectSchoolDamage(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		UInt8 school = m_spell.schoolmask();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			const game::VictimState &state = victimStates[i];
			UInt32 totalDamage;
			bool crit = false;
			UInt32 resisted = 0;
			UInt32 absorbed = 0;
			if (state == game::victim_state::IsImmune ||
				state == game::victim_state::Evades)
			{
				totalDamage = 0;
			}
			else if (hitInfos[i] == game::hit_info::Miss)
			{
				totalDamage = 0;
			}
			else
			{
				UInt32 spellPower = caster.getBonus(school);
				UInt32 spellBonusPct = caster.getBonusPct(school);
				totalDamage = getSpellPointsTotal(effect, spellPower, spellBonusPct);

				// Apply damage taken bonus
				targetUnit->applyDamageTakenBonus(school, 1, totalDamage);

				if (caster.isGameCharacter())
				{
					reinterpret_cast<GameCharacter&>(caster).applySpellMod(
						spell_mod_op::Damage, m_spell.id(), totalDamage);
				}

				if (hitInfos[i] == game::hit_info::CriticalHit)
				{
					crit = true;
					totalDamage *= 1.5f;

					if (caster.isGameCharacter())
					{
						reinterpret_cast<GameCharacter&>(caster).applySpellMod(
							spell_mod_op::CritDamageBonus, m_spell.id(), totalDamage);
					}
				}

				resisted = totalDamage * (resists[i] / 100.0f);
				absorbed = targetUnit->consumeAbsorb(totalDamage - resisted, school);
			}

			// Update health value
			const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
			float threat = noThreat ? 0.0f : totalDamage - resisted - absorbed;
			if (!noThreat && m_cast.getExecuter().isGameCharacter())
			{
				reinterpret_cast<GameCharacter&>(m_cast.getExecuter()).applySpellMod(spell_mod_op::Threat, m_spell.id(), threat);
			}
			if (targetUnit->dealDamage(totalDamage - resisted - absorbed, school, game::DamageType::Direct, &caster, threat))
			{
				if (totalDamage == 0 && resisted == 0) {
					totalDamage = resisted = 1;
				}

				// Send spell damage packet
				m_completedEffectsExecution[targetUnit->getGuid()] = completedEffects.connect([this, &caster, targetUnit, totalDamage, school, absorbed, resisted, crit, state]()
				{
					std::map<UInt64, game::SpellMissInfo> missedTargets;
					if (state == game::victim_state::Evades)
					{
						missedTargets[targetUnit->getGuid()] = game::spell_miss_info::Evade;
					}
					else if (state == game::victim_state::IsImmune)
					{
						missedTargets[targetUnit->getGuid()] = game::spell_miss_info::Immune;
					}
					else if (state == game::victim_state::Dodge)
					{
						missedTargets[targetUnit->getGuid()] = game::spell_miss_info::Dodge;
					}

					if (missedTargets.empty())
					{
						sendPacketFromCaster(caster,
							std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
								targetUnit->getGuid(),
								caster.getGuid(),
								m_spell.id(),
								totalDamage,
								school,
								absorbed,
								resisted,
								false,
								0,
								crit));
					}
					else
					{
						sendPacketFromCaster(caster,
							std::bind(game::server_write::spellLogMiss, std::placeholders::_1,
								m_spell.id(),
								caster.getGuid(),
								0,
								std::cref(missedTargets)
							));
					}
				});	// End connect
			}

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i], totalDamage, absorbed, true);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i], totalDamage, absorbed, true);
			}
		}
	}

	void SingleCastState::spellEffectNormalizedWeaponDamage(const proto::SpellEffect &effect)
	{
		meleeSpecialAttack(effect, false);
	}

	void SingleCastState::spellEffectStealBeneficialBuff(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		UInt8 school = m_spell.schoolmask();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			UInt32 totalPoints = 0;
			bool spellFailed = false;

			if (hitInfos[i] == game::hit_info::Miss)
			{
				spellFailed = true;
			}
			else if (victimStates[i] == game::victim_state::IsImmune)
			{
				spellFailed = true;
			}
			else if (victimStates[i] == game::victim_state::Normal)
			{
				if (resists[i] == 100.0f)
				{
					spellFailed = true;
				}
				else
				{
					totalPoints = calculateEffectBasePoints(effect);
				}
			}

			if (spellFailed)
			{
				// TODO send fail packet
				sendPacketFromCaster(caster,
					std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
						targetUnit->getGuid(),
						caster.getGuid(),
						m_spell.id(),
						1,
						school,
						0,
						1,	//resisted
						false,
						0,
						false));
			}
			else if (targetUnit->isAlive())
			{
				//UInt32 auraDispelType = effect.miscvaluea();
				for (UInt32 i = 0; i < totalPoints; i++)
				{
					// TODO:
					/*AuraEffect *stolenAura = targetUnit->getAuras().popBack(auraDispelType, true);
					if (stolenAura)
					{
						proto::SpellEntry spell(stolenAura->getSpell());
						proto::SpellEffect effect(stolenAura->getEffect());
						UInt32 basepoints(stolenAura->getBasePoints());
						//stolenAura->misapplyAura();

						auto *world = caster.getWorldInstance();
						auto &universe = world->getUniverse();
						std::shared_ptr<AuraEffect> aura = std::make_shared<AuraEffect>(spell, effect, basepoints, caster, caster, m_target, m_itemGuid, false, [&universe](std::function<void()> work)
						{
							universe.post(work);
						}, [](AuraEffect & self)
						{
							// Prevent aura from being deleted before being removed from the list
							auto strong = self.shared_from_this();

							// Remove aura from the list
							self.getTarget().getAuras().removeAura(self);
						});
						caster.getAuras().addAura(std::move(aura));
					}
					else
					{
						break;
					}*/
				}
			}

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i]);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i]);
			}
		}
	}

	void SingleCastState::spellEffectInterruptCast(const proto::SpellEffect & effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			targetUnit->cancelCast(game::spell_interrupt_flags::Interrupt, m_spell.duration());

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i]);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i]);
			}
		}
	}

	void SingleCastState::spellEffectLearnSpell(const proto::SpellEffect & effect)
	{
		UInt32 spellId = effect.triggerspell();
		if (!spellId)
		{
			// Check item
			auto item = getItem();
			if (item)
			{
				// Look for spell id
				for (const auto &spellCastEntry : item->getEntry().spells())
				{
					if (spellCastEntry.trigger() == game::item_spell_trigger::LearnSpellId)
					{
						spellId = spellCastEntry.spell();
						break;
					}
				}

				if (!spellId)
				{
					ELOG("SPELL_EFFECT_LEARN_SPELL: Unable to get spell id to learn");
					return;
				}
			}
		}

		// Look for spell
		const auto *spell = m_cast.getExecuter().getProject().spells.getById(spellId);
		if (!spell)
		{
			ELOG("SPELL_EFFECT_LEARN_SPELL: Could not find spell " << spellId);
			return;
		}

		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpellNoCrit(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			if (targetUnit->isGameCharacter())
			{
				auto *character = reinterpret_cast<GameCharacter*>(targetUnit);
				if (character->addSpell(*spell))
				{
					// Activate passive spell if it is one
					if (spell->attributes(0) & game::spell_attributes::Passive)
					{
						SpellTargetMap targetMap;
						targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
						targetMap.m_unitTarget = character->getGuid();
						character->castSpell(std::move(targetMap), effect.triggerspell(), { 0, 0, 0 }, 0, true);
					}

					// TODO: Send packets
				}
			}
		}
	}

	void SingleCastState::spellEffectScriptEffect(const proto::SpellEffect & effect)
	{
		// TODO: Do something here...
		UInt32 spellId = 0;

		switch (m_spell.id())
		{
			case  6201:                                 // Healthstone creating spells
			case  6202:
			case  5699:
			case 11729:
			case 11730:
				spellScriptEffectCreateHealthstone(effect);
				return;

			// Translocation orb handling
			case 25140:
				spellId = 32571;
				break;
			case 25143:
				spellId = 32572;
				break;
			case 25650:
				spellId = 30140;
				break;
			case 25652:
				spellId = 30141;
				break;
			case 29128:
				spellId = 32568;
				break;
			case 29129:
				spellId = 32569;
				break;
			case 35376:
				spellId = 25649;
				break;
			case 35727:
				spellId = 35730;
				break;

				// Judgement
			case 20271:
				// Find respective seal dummy aura
			{
				// Cast custom spell on current target
				GameUnit *unitTarget = nullptr;
				m_target.resolvePointers(*m_cast.getExecuter().getWorldInstance(), &unitTarget, nullptr, nullptr, nullptr);
				if (!unitTarget || !unitTarget->isAlive())
				{
					ELOG("Invalid unit target for judgement spell!");
					break;
				}

				AuraEffect *sealAura = nullptr;
				m_cast.getExecuter().getAuras().forEachAuraOfType(game::aura_type::Dummy, [this, &sealAura](AuraEffect &aura) -> bool {
					if (aura.getCaster() == &m_cast.getExecuter() &&
						aura.getEffect().index() > 0 &&	// Never the first effect!
						isSealSpell(aura.getSlot().getSpell()))
					{
						// Found the seal aura
						sealAura = &aura;
						return false;
					}

					return true;
				});

				if (!sealAura)
				{
					ELOG("Could not find seal aura!");
					break;
				}

				// Try to find judgement spell
				const UInt32 judgementSpellId = sealAura->getBasePoints();
				const auto *spell = m_cast.getExecuter().getProject().spells.getById(judgementSpellId);

				// Remove seal aura now as it is no longer needed
				m_cast.getExecuter().getAuras().removeAllAurasDueToSpell(sealAura->getSlot().getSpell().id());
				sealAura = nullptr;

				// Found a spell?
				if (!spell)
				{
					ELOG("Could not find judgement spell for seal " << judgementSpellId);
					break;
				}

				if (spell->family() != game::spell_family::Paladin)
				{
					ELOG("Judgement spell " << spell->id() << " is not a paladin spell");
					break;
				}

				m_cast.getExecuter().castSpell(m_target, spell->id(), { 0, 0, 0 }, 0, true);
			}
			break;
		}

		if (spellId != 0)
		{
			// Look for the spell
			const auto *entry = m_cast.getExecuter().getProject().spells.getById(spellId);
			if (!entry)
			{
				return;
			}

			// Get the cast time of this spell
			Int64 castTime = entry->casttime();
			if (m_cast.getExecuter().isGameCharacter())
			{
				reinterpret_cast<GameCharacter&>(m_cast.getExecuter()).applySpellMod(spell_mod_op::CastTime, spellId, castTime);
			}
			if (castTime < 0) castTime = 0;

			m_cast.getExecuter().castSpell(m_target, spellId, { 0, 0, 0 }, castTime, false);
		}
	}

	void SingleCastState::spellEffectTransDoor(const proto::SpellEffect &effect)
	{
		// Find the requested game object
		const auto *objectEntry = m_cast.getExecuter().getProject().objects.getById(effect.miscvaluea());
		if (!objectEntry)
			return;

		// Determine object target location
		math::Vector3 location = m_cast.getExecuter().getLocation() + math::Vector3(0.0f, 0.0f, 2.0f);
		if (effect.targeta() == game::targets::DestCasterFront)
		{
			// TODO: Determine location in front of character
			
		}

		// Spawn new object
		auto *world = m_cast.getExecuter().getWorldInstance();
		if (world)
		{
			auto spawned = world->spawnWorldObject(*objectEntry, location, 0.0f, 0.0f);

			spawned->setUInt32Value(world_object_fields::AnimProgress, 255);
			spawned->setUInt32Value(world_object_fields::State, 1);
			spawned->setUInt64Value(world_object_fields::Createdby, m_cast.getExecuter().getGuid());
			spawned->setUInt32Value(world_object_fields::Level, m_cast.getExecuter().getLevel());

			m_cast.getExecuter().addWorldObject(spawned);
			world->addGameObject(*spawned);
		}
	}
}
