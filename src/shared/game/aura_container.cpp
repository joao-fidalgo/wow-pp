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
#include "aura_container.h"
#include "aura_spell_slot.h"
#include "aura_effect.h"
#include "game_unit.h"
#include "world_instance.h"
#include "log/default_log_levels.h"
#include "common/linear_set.h"
#include "common/macros.h"

namespace wowpp
{
	AuraContainer::AuraContainer(GameUnit &owner)
		: m_owner(owner)
	{
	}

	bool AuraContainer::addAura(AuraPtr aura, bool restoration/* = false*/)
	{
		// If aura shouldn't be hidden on the client side...
		if (!aura->isPassive() && (aura->getSpell().attributes(0) & game::spell_attributes::HiddenClientSide) == 0)
		{
			UInt8 newSlot = 0xFF;

			// Check old auras for new slot
			for (auto it = m_auras.begin(); it != m_auras.end();)
			{
				if ((*it)->hasValidSlot() && aura->shouldOverwriteAura(*(*it)))
				{
					// Only overwrite if higher or equal base rank
					const bool sameSpell = ((*it)->getSpell().baseid() == aura->getSpell().baseid());
					if ((sameSpell && (*it)->getSpell().rank() <= aura->getSpell().rank()) || !sameSpell)
					{
						// Check if we need to add a stack
						if ((*it)->getSpell().id() == aura->getSpell().id() &&
							aura->getSpell().stackamount() > 0 &&
							(*it)->getCaster() == aura->getCaster() && aura->getCaster())
						{
							// We simply increase the stack and stop here
							(*it)->addStack(*aura);
							return true;
						}
						else
						{
							// We need to replace the old spell
							newSlot = (*it)->getSlot();
							removeAura(it);
							break;
						}
					}
					else if (sameSpell)
					{
						// Aura should have been overwritten, but has a lesser rank
						// So stop checks here and don't apply new aura at all
						// TODO: Send proper error message to the client
						return false;
					}
					else
					{
						++it;
					}
				}
				else
				{
					++it;
				}
			}

			// Aura hasn't been overwritten yet, determine new slot
			if (newSlot == 0xFF)
			{
				if (aura->isPositive())
				{
					for (UInt8 i = 0; i < 40; ++i)
					{
						if (m_owner.getUInt32Value(unit_fields::AuraEffect + i) == 0)
						{
							newSlot = i;
							break;
						}
					}
				}
				else
				{
					for (UInt8 i = 40; i < 56; ++i)
					{
						if (m_owner.getUInt32Value(unit_fields::AuraEffect + i) == 0)
						{
							newSlot = i;
							break;
						}
					}
				}
			}

			// No more free slots
			if (newSlot == 0xFF)
				return false;

			// Apply slot
			aura->setSlot(newSlot);
		}

		// Remove other shapeshifting auras in case this is a shapeshifting aura
		if (aura->hasEffect(game::aura_type::ModShapeShift))
			removeAurasByType(game::aura_type::ModShapeShift);

		// Add aura
		m_auras.push_back(aura);

		// Increase aura counters
		aura->forEachEffect([&](AuraSpellSlot::AuraEffectPtr effect) -> bool {
			m_auraTypeCount[effect->getEffect().aura()]++;
			return true;
		});

		// Apply aura effects
		aura->applyEffects(restoration);
		return true;
	}

	AuraContainer::AuraList::iterator AuraContainer::findAura(AuraSpellSlot &aura)
	{
		for (auto it = m_auras.begin(); it != m_auras.end(); ++it)
		{
			if (it->get() == &aura)
			{
				return it;
			}
		}

		return m_auras.end();
	}

	void AuraContainer::removeAura(AuraList::iterator &it)
	{
		// Make sure that the aura is not destroy when releasing
		ASSERT(it != m_auras.end());
		auto strong = (*it);
		
		// Remove the aura from the list of auras
		it = m_auras.erase(it);

		// Reduce counter
		strong->forEachEffect([&](AuraSpellSlot::AuraEffectPtr effect) -> bool {
			ASSERT(m_auraTypeCount[effect->getEffect().aura()] > 0 && "At least one aura of this type should still exist");
			m_auraTypeCount[effect->getEffect().aura()]--;
			return true;
		});
		
		// NOW misapply the aura. It is important to call this method AFTER the aura has been
		// removed from the list of auras. First: To prevent a stack overflow when removing
		// an aura causes the remove of the same aura types like in ModShapeShift. Second:
		// Stun effects need to check whether there are still ModStun auras on the target, AFTER
		// the aura has been removed (or else it would count itself)!!
		strong->misapplyEffects();
	}

	void AuraContainer::removeAura(AuraSpellSlot &aura)
	{
		auto it = findAura(aura);
		if (it != m_auras.end())
		{
			removeAura(it);
		}
		else {
			WLOG("Could not find aura to remove!");
		}
	}

	void AuraContainer::handleTargetDeath()
	{
		AuraList::iterator it = m_auras.begin();
		while (it != m_auras.end())
		{
			// Keep passive and death persistent auras
			if ((*it)->isDeathPersistent())
			{
				++it;
			}
			else
			{
				// Remove aura
				removeAura(it);
			}
		}
	}

	bool AuraContainer::hasAura(game::AuraType type) const
	{
		auto it = m_auraTypeCount.find(type);
		if (it == m_auraTypeCount.end())
			return false;

		return it->second > 0;
	}

	UInt32 AuraContainer::consumeAbsorb(UInt32 damage, UInt8 school)
	{
		UInt32 absorbed = 0;
		UInt32 ownerMana = m_owner.getUInt32Value(unit_fields::Power1);
		bool manaConsumed = false;

		for (auto it = m_auras.begin(); it != m_auras.end(); )
		{
			auto slot = *it;

			bool shouldRemove = false;
			slot->forEachEffect([&](AuraSpellSlot::AuraEffectPtr effect) -> bool {
				// Determine if this absorb effect is a mana shield
				const bool isManaShield = effect->getEffect().aura() == game::aura_type::ManaShield;
				// Determine if this is an absorb effect
				const bool isAbsorbEffect = 
					(effect->getEffect().aura() == game::aura_type::SchoolAbsorb || isManaShield);

				// Determine if the absorb school mask matches
				if (isAbsorbEffect && (effect->getEffect().miscvaluea() & school))
				{
					// Get maximum damage amount this aura can absorb
					const float multiple = effect->getEffect().multiplevalue() == 0.0f ? 1.0f : effect->getEffect().multiplevalue();
					UInt32 consumableByMana = isManaShield ? (float)ownerMana / multiple : 0;
					UInt32 consumable = ::abs(effect->getBasePoints());

					// Cap consumable value to mana if mana shield
					if (isManaShield)
					{
						// Consume as much damage as possible by mana up to aura limit
						consumable = std::min(consumableByMana, consumable);
						if (consumable <= 0)
						{
							// No mana left?
							return false;
						}
					}

					// Check if there is enough to consume
					if (consumable >= damage)
					{
						// Aura has more base points than needed
						absorbed += damage;
						effect->setBasePoints(consumable - damage);
						if (isManaShield) 
						{
							ownerMana -= (damage * multiple);
							manaConsumed = true;
						}

						// All damage consumed
						damage = 0;

						// Remove aura if base points are set to 0
						if (effect->getBasePoints() == 0)
							shouldRemove = true;
					}
					else
					{
						// Aura will be completely consumed
						absorbed += consumable;
						damage -= consumable;
						shouldRemove = true;

						if (isManaShield)
						{
							ownerMana -= (consumable * multiple);
							manaConsumed = true;
						}
					}

					// Stop aura effect iteration
					return false;
				}

				// Continue iteration if 
				return (damage != 0);
			});

			if (shouldRemove)
				removeAura(it);
			else
				++it;

			if (damage == 0)
				break;
		}

		if (manaConsumed)
			m_owner.setUInt32Value(unit_fields::Power1, ownerMana);

		return absorbed;
	}

	Int32 AuraContainer::getMaximumBasePoints(game::AuraType type) const
	{
		Int32 treshold = 0;

		for (auto it = m_auras.begin(); it != m_auras.end(); ++it)
		{
			(*it)->forEachEffectOfType(type, [&treshold](AuraSpellSlot::AuraEffectPtr effect) -> bool {
				if (effect->getBasePoints() > treshold)
					treshold = effect->getBasePoints();
				return true;
			});
		}

		return treshold;
	}

	Int32 AuraContainer::getMinimumBasePoints(game::AuraType type) const
	{
		Int32 treshold = 0;

		for (auto it = m_auras.begin(); it != m_auras.end(); ++it)
		{
			(*it)->forEachEffectOfType(type, [&treshold](AuraSpellSlot::AuraEffectPtr effect) -> bool {
				if (effect->getBasePoints() < treshold)
					treshold = effect->getBasePoints();
				return true;
			});
		}

		return treshold;
	}

	Int32 AuraContainer::getTotalBasePoints(game::AuraType type) const
	{
		Int32 treshold = 0;

		for (auto it = m_auras.begin(); it != m_auras.end(); ++it)
		{
			(*it)->forEachEffectOfType(type, [&treshold](AuraSpellSlot::AuraEffectPtr effect) -> bool {
				treshold += effect->getBasePoints();
				return true;
			});
		}

		return treshold;
	}

	float AuraContainer::getTotalMultiplier(game::AuraType type) const
	{
		float multiplier = 1.0f;

		for (auto it = m_auras.begin(); it != m_auras.end(); ++it)
		{
			(*it)->forEachEffectOfType(type, [&multiplier](AuraSpellSlot::AuraEffectPtr effect) -> bool {
				multiplier *= (100.0f + static_cast<float>(effect->getBasePoints())) / 100.0f;
				return true;
			});
		}

		return multiplier;
	}

	void AuraContainer::forEachAura(std::function<bool(AuraEffect&)> functor)
	{
		for (auto aura : m_auras)
		{
			aura->forEachEffect([&](AuraSpellSlot::AuraEffectPtr effect) -> bool {
				return functor(*effect);
			});
		}
	}

	void AuraContainer::forEachAuraOfType(game::AuraType type, std::function<bool(AuraEffect&)> functor)
	{
		// Performance check before iteration
		if (!hasAura(type))
			return;

		for (auto it = m_auras.begin(); it != m_auras.end(); ++it)
		{
			if ((*it)->hasEffect(type))
			{
				(*it)->forEachEffectOfType(type, [&](AuraSpellSlot::AuraEffectPtr effect) -> bool {
					return functor(*effect);
				});
			}
		}
	}

	void AuraContainer::logAuraInfos()
	{
		// TODO
	}

	bool AuraContainer::serializeAuraData(std::vector<AuraData>& out_data) const
	{
		out_data.clear();

		for (const auto aura : m_auras)
		{
			// Skip passive auras
			if (aura->isPassive() || aura->isChanneled())
				continue;

			AuraData data;
			data.spell = aura->getSpell().id();
			if (aura->getCaster()) data.casterGuid = aura->getCaster()->getGuid();
			data.itemGuid = aura->getItemGuid();
			data.maxDuration = aura->getTotalDuration();
			data.remainingTime = aura->getRemainingTime();
			data.remainingCharges = aura->getRemainingCharges();
			data.stackCount = aura->getStackCount();

			// Gather effect data
			Int32 effectIndex = 0;
			aura->forEachEffect([&effectIndex, &data](std::shared_ptr<AuraEffect> eff) -> bool {
				data.basePoints[effectIndex] = eff->getBasePoints();

				effectIndex++;
				return true;
			});

			// Add data to the vector
			out_data.push_back(std::move(data));
		}

		return true;
	}

	bool AuraContainer::restoreAuraData(const std::vector<AuraData>& data)
	{
		// Apply every single aura from the container
		for (const AuraData& auraData : data)
		{
			// Get spell entry
			const auto* spellEntry = m_owner.getProject().spells.getById(auraData.spell);
			if (!spellEntry)
			{
				WLOG("Unable to restore aura due to invalid spell id " << auraData.spell);
				continue;
			}

			// Restore aura
			AuraPtr aura = std::make_shared<AuraSpellSlot>(m_owner.getTimers(), *spellEntry, auraData.itemGuid);
			aura->setOwner(std::static_pointer_cast<GameUnit>(m_owner.shared_from_this()));
			aura->setInitialDuration(auraData.remainingTime);
			aura->setStackCount(auraData.stackCount);
			aura->setChargeCount(static_cast<UInt8>(auraData.remainingCharges));

			// Try to find and restore caster information
			GameUnit* caster = nullptr;
			if (m_owner.getWorldInstance())
			{
				caster = dynamic_cast<GameUnit*>(m_owner.getWorldInstance()->findObjectByGUID(auraData.casterGuid));
				if (caster)
				{
					aura->setCaster(std::static_pointer_cast<GameUnit>(caster->shared_from_this()));
				}
			}

			// Prepare target map
			SpellTargetMap targetMap;
			targetMap.m_unitTarget = m_owner.getGuid();
			targetMap.m_targetMap = game::spell_cast_target_flags::Unit;

			// TODO: Add aura effects
			Int32 effectIndex = 0;
			for (const auto& spellEffect : spellEntry->effects())
			{
				if (spellEffect.type() == game::spell_effects::ApplyAura && spellEffect.aura() != 0)
				{
					auto auraEffect = std::make_shared<AuraEffect>(*aura, spellEffect, auraData.basePoints[effectIndex++], caster, m_owner, targetMap, false);
					aura->addAuraEffect(auraEffect);
				}
			}

			// Apply aura
			if (!addAura(aura, true))
			{
				WLOG("Failed to apply restored aura!");
			}
		}

		return true;
	}

	void AuraContainer::removeAllAurasDueToSpell(UInt32 spellId)
	{
		ASSERT(spellId && "Valid spell id should be specified");

		AuraList::iterator it = m_auras.begin();
		while (it != m_auras.end())
		{
			if ((*it)->getSpell().id() == spellId)
			{
				removeAura(it);
			}
			else
			{
				it++;
			}
		}
	}

	void AuraContainer::removeAllAurasDueToItem(UInt64 itemGuid)
	{
		ASSERT(itemGuid && "Valid item guid should be specified");

		for (auto it = m_auras.begin(); it != m_auras.end(); )
		{
			if ((*it)->getItemGuid() == itemGuid)
				removeAura(it);
			else
				++it;
		}
	}

	void AuraContainer::removeAllAurasDueToMechanic(UInt32 immunityMask)
	{
		ASSERT(immunityMask && "At least one mechanic should be provided");

		// We need to remove all auras by their spell
		LinearSet<UInt32> spells;
		for (auto it = m_auras.begin(); it != m_auras.end();)
		{
			if ((*it)->hasMechanics(immunityMask))
				removeAura(it);
			else
				++it;
		}
	}

	UInt32 AuraContainer::removeAurasDueToDispel(UInt32 dispelType, bool dispelPositive, UInt32 count/* = 1*/)
	{
		ASSERT(dispelType && "A valid dispel type should be provided");
		ASSERT(count && "At least a number of 1 should be provided");

		UInt32 successCount = 0;

		for (auto it = m_auras.begin(); it != m_auras.end();)
		{
			if ((*it)->getSpell().dispel() == dispelType &&
				(*it)->isPositive() == dispelPositive)
			{
				removeAura(it);
				if (++successCount >= count)
					return successCount;
			}
			else
			{
				++it;
			}
		}

		return successCount;
	}

	void AuraContainer::removeAurasByType(UInt32 auraType)
	{
		ASSERT(auraType && "A valid aura effect type should be specified");

		// We need to remove all auras by their spell
		for (auto it = m_auras.begin(); it != m_auras.end(); )
		{
			if ((*it)->hasEffect(auraType))
				removeAura(it);
			else
				++it;
		}
	}

	void AuraContainer::removeAllAurasDueToInterrupt(game::SpellAuraInterruptFlags flags)
	{
		// We need to remove all auras by their spell
		for (auto it = m_auras.begin(); it != m_auras.end(); )
		{
			if ((*it)->getSpell().aurainterruptflags() & flags)
				removeAura(it);
			else
				++it;
		}
	}

	void AuraContainer::removeAllAuras()
	{
		for (auto it = m_auras.begin(); it != m_auras.end();)
		{
			removeAura(it);
		}
	}
}
