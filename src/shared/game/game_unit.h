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

#pragma once

#include "game_object.h"
#include "game/defines.h"
#include "common/timer_queue.h"
#include "common/countdown.h"
#include "movement_info.h"
#include "spell_cast.h"
#include "spell_target_map.h"
#include "aura_effect.h"
#include "aura_container.h"
#include "common/macros.h"
#include "common/linear_set.h"
#include "attack_table.h"
#include "proto_data/trigger_helper.h"
#include "game_world_object.h"
#include "game_dyn_object.h"
#include "common/id_generator.h"

namespace wowpp
{
	namespace unit_stand_state
	{
		enum Enum
		{
			Stand			= 0x00,
			Sit				= 0x01,
			SitChair		= 0x02,
			Sleep			= 0x03,
			SitLowChair		= 0x04,
			SitMediumChais	= 0x05,
			SitHighChair	= 0x06,
			Dead			= 0x07,
			Kneel			= 0x08,

			Count
		};
	}

	typedef unit_stand_state::Enum UnitStandState;

	namespace unit_fields
	{
		enum Enum
		{
			Charm						= 0x00 + object_fields::ObjectFieldCount,
			Summon						= 0x02 + object_fields::ObjectFieldCount,
			CharmedBy					= 0x04 + object_fields::ObjectFieldCount,
			SummonedBy					= 0x06 + object_fields::ObjectFieldCount,
			CreatedBy					= 0x08 + object_fields::ObjectFieldCount,
			Target						= 0x0A + object_fields::ObjectFieldCount,
			Persuaded					= 0x0C + object_fields::ObjectFieldCount,
			ChannelObject				= 0x0E + object_fields::ObjectFieldCount,
			Health						= 0x10 + object_fields::ObjectFieldCount,
			Power1						= 0x11 + object_fields::ObjectFieldCount,
			PowerMana					= Power1,
			Power2						= 0x12 + object_fields::ObjectFieldCount,
			PowerRage					= Power2,
			Power3						= 0x13 + object_fields::ObjectFieldCount,
			Power4						= 0x14 + object_fields::ObjectFieldCount,
			PowerEnergy					= Power4,
			Power5						= 0x15 + object_fields::ObjectFieldCount,
			MaxHealth					= 0x16 + object_fields::ObjectFieldCount,
			MaxPower1					= 0x17 + object_fields::ObjectFieldCount,
			MaxPower2					= 0x18 + object_fields::ObjectFieldCount,
			MaxPower3					= 0x19 + object_fields::ObjectFieldCount,
			MaxPower4					= 0x1A + object_fields::ObjectFieldCount,
			MaxPower5					= 0x1B + object_fields::ObjectFieldCount,
			Level						= 0x1C + object_fields::ObjectFieldCount,
			FactionTemplate				= 0x1D + object_fields::ObjectFieldCount,
			Bytes0						= 0x1E + object_fields::ObjectFieldCount,
			VirtualItemSlotDisplay		= 0x1F + object_fields::ObjectFieldCount,
			VirtualItemInfo				= 0x22 + object_fields::ObjectFieldCount,
			UnitFlags					= 0x28 + object_fields::ObjectFieldCount,
			UnitFlags2					= 0x29 + object_fields::ObjectFieldCount,
			AuraEffect					= 0x2A + object_fields::ObjectFieldCount,
			AuraList					= 0x58 + object_fields::ObjectFieldCount,
			AuraFlags					= 0x62 + object_fields::ObjectFieldCount,
			AuraLevels					= 0x70 + object_fields::ObjectFieldCount,
			AuraApplications			= 0x7E + object_fields::ObjectFieldCount,
			AuraState					= 0x8C + object_fields::ObjectFieldCount,
			BaseAttackTime				= 0x8D + object_fields::ObjectFieldCount,
			RangedAttackTime			= 0x8F + object_fields::ObjectFieldCount,
			BoundingRadius				= 0x90 + object_fields::ObjectFieldCount,
			CombatReach					= 0x91 + object_fields::ObjectFieldCount,
			DisplayId					= 0x92 + object_fields::ObjectFieldCount,
			NativeDisplayId				= 0x93 + object_fields::ObjectFieldCount,
			MountDisplayId				= 0x94 + object_fields::ObjectFieldCount,
			MinDamage					= 0x95 + object_fields::ObjectFieldCount,
			MaxDamage					= 0x96 + object_fields::ObjectFieldCount,
			MinOffHandDamage			= 0x97 + object_fields::ObjectFieldCount,
			MaxOffHandDamage			= 0x98 + object_fields::ObjectFieldCount,
			Bytes1						= 0x99 + object_fields::ObjectFieldCount,
			PetNumber					= 0x9A + object_fields::ObjectFieldCount,
			PetNameTimestamp			= 0x9B + object_fields::ObjectFieldCount,
			PetExperience				= 0x9C + object_fields::ObjectFieldCount,
			PetNextLevelExp				= 0x9D + object_fields::ObjectFieldCount,
			DynamicFlags				= 0x9E + object_fields::ObjectFieldCount,
			ChannelSpell				= 0x9F + object_fields::ObjectFieldCount,
			ModCastSpeed				= 0xA0 + object_fields::ObjectFieldCount,
			CreatedBySpell				= 0xA1 + object_fields::ObjectFieldCount,
			NpcFlags					= 0xA2 + object_fields::ObjectFieldCount,
			NpcEmoteState				= 0xA3 + object_fields::ObjectFieldCount,
			TrainingPoints				= 0xA4 + object_fields::ObjectFieldCount,
			Stat0						= 0xA5 + object_fields::ObjectFieldCount,
			Stat1						= 0xA6 + object_fields::ObjectFieldCount,
			Stat2						= 0xA7 + object_fields::ObjectFieldCount,
			Stat3						= 0xA8 + object_fields::ObjectFieldCount,
			Stat4						= 0xA9 + object_fields::ObjectFieldCount,
			PosStat0					= 0xAA + object_fields::ObjectFieldCount,
			PosStat1					= 0xAB + object_fields::ObjectFieldCount,
			PosStat2					= 0xAC + object_fields::ObjectFieldCount,
			PosStat3					= 0xAD + object_fields::ObjectFieldCount,
			PosStat4					= 0xAE + object_fields::ObjectFieldCount,
			NegStat0					= 0xAF + object_fields::ObjectFieldCount,
			NegStat1					= 0xB0 + object_fields::ObjectFieldCount,
			NegStat2					= 0xB1 + object_fields::ObjectFieldCount,
			NegStat3					= 0xB2 + object_fields::ObjectFieldCount,
			NegStat4					= 0xB3 + object_fields::ObjectFieldCount,
			Resistances					= 0xB4 + object_fields::ObjectFieldCount,
			ResistancesBuffModsPositive	= 0xBB + object_fields::ObjectFieldCount,
			ResistancesBuffModsNegative	= 0xC2 + object_fields::ObjectFieldCount,
			BaseMana					= 0xC9 + object_fields::ObjectFieldCount,
			BaseHealth					= 0xCA + object_fields::ObjectFieldCount,
			Bytes2						= 0xCB + object_fields::ObjectFieldCount,
			AttackPower					= 0xCC + object_fields::ObjectFieldCount,
			AttackPowerMods				= 0xCD + object_fields::ObjectFieldCount,
			AttackPowerMultiplier		= 0xCE + object_fields::ObjectFieldCount,
			RangedAttackPower			= 0xCF + object_fields::ObjectFieldCount,
			RangedAttackPowerMods		= 0xD0 + object_fields::ObjectFieldCount,
			RangedAttackPowerMultiplier	= 0xD1 + object_fields::ObjectFieldCount,
			MinRangedDamage				= 0xD2 + object_fields::ObjectFieldCount,
			MaxRangedDamage				= 0xD3 + object_fields::ObjectFieldCount,
			PowerCostModifier			= 0xD4 + object_fields::ObjectFieldCount,
			PowerCostMultiplier			= 0xDB + object_fields::ObjectFieldCount,
			MaxHealthModifier			= 0xE2 + object_fields::ObjectFieldCount,
			Padding						= 0xE3 + object_fields::ObjectFieldCount,

			UnitFieldCount				= 0xE4 + object_fields::ObjectFieldCount,
		};
	}

	namespace attack_swing_error
	{
		enum Type
		{
			/// Can't auto attack while moving (used for ranged auto attacks)
			NotStanding			= 0,
			/// Target is out of range (or too close in case of ranged auto attacks).
			OutOfRange			= 1,
			/// Can't attack that target (invalid target).
			CantAttack			= 2,
			/// Target has to be in front of us (we need to look at the target).
			WrongFacing			= 3,
			/// The target is dead and thus can not be attacked.
			TargetDead			= 4,

			/// Successful auto attack swing. This code is never sent to the client.
			Success				= 0xFFFFFFFE,
			/// Unknown attack swing error. This code is never sent to the client.
			Unknown				= 0xFFFFFFFF
		};
	}

	typedef attack_swing_error::Type AttackSwingError;

	namespace unit_mod_type
	{
		enum Type
		{
			/// Absolute base value of this unit based on it's level.
			BaseValue			= 0,
			/// Base value mulitplier (1.0 = 100%)
			BasePct				= 1,
			/// Absolute total value. Final value: BaseValue * BasePct + TotalValue * TotalPct;
			TotalValue			= 2,
			/// Total value multiplier.
			TotalPct			= 3,

			End					= 4
		};
	}

	typedef unit_mod_type::Type UnitModType;

	namespace unit_mods
	{
		enum Type
		{
			/// Strength stat value modifier.
			StatStrength			= 0,
			/// Agility stat value modifier.
			StatAgility				= 1,
			/// Stamina stat value modifier.
			StatStamina				= 2,
			/// Intellect stat value modifier.
			StatIntellect			= 3,
			/// Spirit stat value modifier.
			StatSpirit				= 4,
			/// Health value modifier.
			Health					= 5,
			/// Mana power value modifier.
			Mana					= 6,
			/// Rage power value modifier.
			Rage					= 7,
			/// Focus power value modifier.
			Focus					= 8,
			/// Energy power value modifier.
			Energy					= 9,
			/// Happiness power value modifier.
			Happiness				= 10,
			/// Armor resistance value modifier.
			Armor					= 11,
			/// Holy resistance value modifier.
			ResistanceHoly			= 12,
			/// Fire resistance value modifier.
			ResistanceFire			= 13,
			/// Nature resistance value modifier.
			ResistanceNature		= 14,
			/// Frost resistance value modifier.
			ResistanceFrost			= 15,
			/// Shadow resistance value modifier.
			ResistanceShadow		= 16,
			/// Arcane resistance value modifier.
			ResistanceArcane		= 17,
			/// Melee attack power value modifier.
			AttackPower				= 18,
			/// Ranged attack power value modifier.
			AttackPowerRanged		= 19,
			/// Main hand weapon damage modifier.
			DamageMainHand			= 20,
			/// Off hand weapon damage modifier.
			DamageOffHand			= 21,
			/// Ranged weapon damage modifier.
			DamageRanged			= 22,
			/// Main hand weapon attack speed modifier.
			AttackSpeed				= 23,
			/// Ranged weapon attack speed modifier.
			AttackSpeedRanged		= 24,

			End						= 25,

			/// Start of stat value modifiers. Used for iteration.
			StatStart				= StatStrength,
			/// End of stat value modifiers. Used for iteration.
			StatEnd					= StatSpirit + 1,
			/// Start of resistance value modifiers. Used for iteration.
			ResistanceStart			= Armor,
			/// End of resistance value modifiers. Used for iteration.
			ResistanceEnd			= ResistanceArcane + 1,
			/// Start of power value modifiers. Used for iteration.
			PowerStart				= Mana,
			/// End of power value modifiers. Used for iteration.
			PowerEnd				= Happiness + 1
		};
	}

	typedef unit_mods::Type UnitMods;

	namespace base_mod_group
	{
		enum Type
		{
			///
			CritPercentage			= 0,
			///
			RangedCritPercentage	= 1,
			///
			OffHandCritPercentage	= 2,
			///
			ShieldBlockValue		= 3,

			End						= 4
		};
	}

	typedef base_mod_group::Type BaseModGroup;

	namespace base_mod_type
	{
		enum Type
		{
			/// Absolute modifier value type.
			Flat		= 0,
			/// Percentual modifier value type (float, where 1.0 = 100%).
			Percentage	= 1,

			End			= 2
		};
	}

	typedef base_mod_type::Type BaseModType;

	/// Enumerates crowd control states of a unit, which do affect control over the unit.
	namespace unit_state
	{
		enum Type
		{
			/// Default state - no effect applied.
			Default = 0x00,
			/// Unit is stunned.
			Stunned = 0x01,
			/// Unit is confused.
			Confused = 0x02,
			/// Unit is rooted.
			Rooted = 0x04,
			/// Unit is charmed by another unit.
			Charmed = 0x08,
			/// Unit is feared.
			Feared = 0x10
		};
	}

	namespace proto
	{
		class ClassEntry;
		class RaceEntry;
		class LevelEntry;
		class FactionTemplateEntry;
		class TriggerEntry;
	}

	class UnitMover;

	/// Enumerates possible movement changes which need to be acknowledged by the client.
	enum class MovementChangeType
	{
		/// Default value. Do not use!
		Invalid,

		/// Character has been rooted or unrooted.
		Root,
		/// Character can or can no longer walk on water.
		WaterWalk,
		/// Character is hovering or no longer hovering.
		Hover,
		/// Character can or can no longer fly.
		CanFly,
		/// Character has or has no longer slow fall
		FeatherFall,
		
		/// Walk speed changed
		SpeedChangeWalk,
		/// Run speed changed
		SpeedChangeRun,
		/// Run back speed changed
		SpeedChangeRunBack,
		/// Swim speed changed
		SpeedChangeSwim,
		/// Swim back speed changed
		SpeedChangeSwimBack,
		/// Turn rate changed
		SpeedChangeTurnRate,
		/// Flight speed changed
		SpeedChangeFlightSpeed,
		/// Flight back speed changed
		SpeedChangeFlightBackSpeed,
		
		/// Character teleported
		Teleport,
		/// Character was knocked back
		KnockBack
	};

	/// Bundles informations which are only used for knock back acks.
	struct KnockBackInfo
	{
		float vcos = 0.0f;
		float vsin = 0.0f;
		/// 2d speed value
		float speedXY = 0.0f;
		/// z axis speed value
		float speedZ = 0.0f;
	};

	struct TeleportInfo
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float o = 0.0f;
	};
		
	/// Contains infos about a pending movement change which first needs to
	/// be acknowledged by the client before it's effects can be applied.
	struct PendingMovementChange final
	{
		/// A counter which is used to verify that the ackknowledged change
		/// is for the expected pending change.
		UInt32 counter;
		/// Defines what kind of change should be applied.
		MovementChangeType changeType;
		/// A timestamp value used for timeouts.
		UInt64 timestamp;

		// Additional data to perform checks whether the ack packet data is correct
		// and hasn't been modified at the client side.
		union
		{
			/// The new value which should be applied. Currently only used for speed changes.
			float speed;
			/// Whether the respective movement flags should be applied or misapplied. This is
			/// only used for Hover / FeatherFall etc., and ignored for speed changes.
			bool apply;

			KnockBackInfo knockBackInfo;

			TeleportInfo teleportInfo;
		};

		/// Default constructor clears all variables
		PendingMovementChange();
	};

	/// Interface for watching unit events which need to generate net packets that
	/// are sent to one or multiple clients.
	struct INetUnitWatcher
	{
		/// Virtual destructor
		virtual ~INetUnitWatcher() {}

	public:
		/// Executed when a speed change was applied on the watched unit.
		/// @param type The movement type whose speed value was changed.
		/// @param speed The new movement speed in units per second.
		/// @param ackId An index which will be sent back by the client to identify the ack packet.
		virtual void onSpeedChangeApplied(MovementType type, float speed, UInt32 ackId) = 0;

		virtual void onCanFlyChangeApplied(bool canFly, UInt32 ackId) = 0;

		virtual void onCanWaterWalkChangeApplied(bool canWaterWalk, UInt32 ackId) = 0;

		virtual void onHoverChangeApplied(bool hover, UInt32 ackId) = 0;

		virtual void onFeatherFallChangeApplied(bool featherFall, UInt32 ackId) = 0;

		virtual void onRootChangeApplied(bool rooted, UInt32 ackId) = 0;
	};

	/// Base class for all units in the world. A unit is an object with health, which can
	/// be controlled, fight etc. This class will be inherited by the GameCreature and the
	/// GameCharacter classes.
	class GameUnit : public GameObject
	{
		/// Serializes an instance of a GameUnit class (binary)
		friend io::Writer &operator << (io::Writer &w, GameUnit const &object);
		/// Deserializes an instance of a GameUnit class (binary)
		friend io::Reader &operator >> (io::Reader &r, GameUnit &object);

	public:

		typedef std::function<void(game::SpellCastResult)> SpellSuccessCallback;
		typedef std::function<bool()> AttackSwingCallback;
		typedef std::unordered_map<UInt32, GameTime> CooldownMap;
		typedef std::unordered_map<UInt32, GameUnit *> TrackAuraTargetsMap;

		/// Fired when this unit was killed. Parameter: GameUnit* killer (may be nullptr if killer
		/// information is not available (for example due to environmental damage))
		simple::signal<void(GameUnit *)> killed;
		/// Fired when an auto attack error occurred. Used in World Node by the Player class to
		/// send network packets based on the error code.
		simple::signal<void(AttackSwingError)> autoAttackError;
		/// Fired when a spell cast error occurred.
		simple::signal<void(const proto::SpellEntry &, game::SpellCastResult)> spellCastError;
		/// Fired when the unit level changed.
		/// Parameters: Previous Level, Health gained, Mana gained, Stats gained (all 5 stats)
		simple::signal<void(UInt32, Int32, Int32, Int32, Int32, Int32, Int32, Int32)> levelGained;
		/// Fired when some aura information was updated.
		/// Parameters: Slot, Spell-ID, Duration (ms), Max Duration (ms)
		simple::signal<void(UInt8, UInt32, Int32, Int32)> auraUpdated;
		/// Fired when some aura information was updated on a target.
		/// Parameters: Slot, Spell-ID, Duration (ms), Max Duration (ms)
		simple::signal<void(UInt64, UInt8, UInt32, Int32, Int32)> targetAuraUpdated;
		/// Fired when the unit should be teleported. This event is only fired when the unit changes world.
		/// Parameters: Target Map, X, Y, Z, O
		simple::signal<void(UInt16, math::Vector3, float)> teleport;
		/// Fired when the units faction changed. This might cause the unit to become friendly to attackers.
		simple::signal<void(GameUnit &)> factionChanged;
		///
		simple::signal<void(GameUnit &, float)> threatened;
		///
		simple::signal<float(GameUnit &threatener)> getThreat;
		///
		simple::signal<void(GameUnit &threatener, float amount)> setThreat;
		///
		simple::signal<GameUnit *()> getTopThreatener;
		/// Fired when done an melee attack hit  (include miss/dodge...)
		simple::signal<void(GameUnit *, game::VictimState)> doneMeleeAttack;
		/// Fired when hit by a melee attack (include miss/dodge...)
		simple::signal<void(GameUnit *, game::VictimState)> takenMeleeAttack;
		/// Fired when hit by any damage.
		simple::signal<void(GameUnit *, UInt32, game::DamageType)> takenDamage;
		/// Fired when this unit was healed by another unit.
		simple::signal<void(GameUnit *, UInt32)> healed;
		/// Fired when unit enters water
		simple::signal<void()> enteredWater;
		/// Fired when unit started attacking
		simple::signal<void()> startedAttacking;
		/// Fired when unit started active casting (excluding proc)
		simple::signal<void(const proto::SpellEntry &)> startedCasting;
		/// Fired when a unit trigger should be executed.
		simple::signal<void(const proto::TriggerEntry &, GameUnit &, GameUnit *)> unitTrigger;
		/// Fired when a unit state changed.
		simple::signal<void(UInt32, bool)> unitStateChanged;
		/// Fired when this unit enters or leaves stealth mode.
		simple::signal<void(bool)> stealthStateChanged;
		/// Fired when the movement speed of this unit changes.
		simple::signal<void(MovementType)> speedChanged;
		/// Fired when a custom cooldown event was rised (for example, "Stealth" cooldown is only fired when stealth ends).
		simple::signal<void(UInt32)> cooldownEvent;
		/// Fired when the units stand state changed.
		simple::signal<void(UnitStandState)> standStateChanged;
		/// Fired on any proc event (damage done, taken, healed, etc).
		simple::signal<void(bool, GameUnit *, UInt32, UInt32, const proto::SpellEntry *, UInt32, UInt8, bool)> spellProcEvent;
		/// Fired when the unit expects a client ack.
		//simple::signal<void(UInt32 opCode, UInt32 counter)> queueClientAck;

	public:

		/// Creates a new instance of the GameUnit object, which will still be uninitialized.
		explicit GameUnit(
			proto::Project &project,
			TimerQueue &timers);
		virtual ~GameUnit();

		/// @copydoc GameObject::initialize()
		virtual void initialize() override;

		/// @copydoc GameObject::getTypeId()
		virtual ObjectType getTypeId() const override {
			return object_type::Unit;
		}

		/// 
		void threaten(GameUnit &threatener, float amount);

		/// Updates the race index and will also update the race entry object.
		void setRace(UInt8 raceId);
		/// Updates the class index and will also update the class entry object.
		void setClass(UInt8 classId);
		/// Updates the gender and will also update the appearance.
		void setGender(game::Gender gender);
		/// Updates the level and will also update all stats based on the new level.
		void setLevel(UInt8 level);
		/// Gets the current race index.
		UInt8 getRace() const 
		{
			return getByteValue(unit_fields::Bytes0, 0);
		}
		/// Gets the current class index.
		UInt8 getClass() const 
		{
			return getByteValue(unit_fields::Bytes0, 1);
		}
		/// Gets the current gender.
		UInt8 getGender() const 
		{
			return getByteValue(unit_fields::Bytes0, 2);
		}
		/// Gets the current level.
		UInt32 getLevel() const 
		{
			return getUInt32Value(unit_fields::Level);
		}
		/// Gets Power Type
		game::PowerType getPowerType() const 
		{
			return static_cast<game::PowerType>(getByteValue(unit_fields::Bytes0, 3));
		}
		/// Sets the current power type of the unit.
		void setPowerType(game::PowerType powerType) 
		{
			setByteValue(unit_fields::Bytes0, 3, static_cast<UInt8>(powerType));
		}
		/// Gets the current shapeshift form.
		game::ShapeshiftForm getShapeShiftForm() const
		{
			return static_cast<game::ShapeshiftForm>(getByteValue(unit_fields::Bytes2, 3));
		}
		/// Sets the current shapeshift form.
		void setShapeShiftForm(game::ShapeshiftForm form)
		{
			setByteValue(unit_fields::Bytes2, 3, static_cast<UInt8>(form));
		}
		/// Gets the respective power value.
		UInt32 getPower(game::PowerType powerType) const
		{
			return getUInt32Value(unit_fields::Power1 + powerType);
		}
		/// Sets the current power value for the given power type.
		void setPower(game::PowerType powerType, UInt32 value)
		{
			const UInt32 maxValue = getUInt32Value(unit_fields::MaxPower1 + powerType);
			setUInt32Value(unit_fields::Power1 + powerType, value > maxValue ? maxValue : value);
		}
		
		/// Gets this units faction template.
		const proto::FactionTemplateEntry &getFactionTemplate() const;
		///
		void setFactionTemplate(const proto::FactionTemplateEntry &faction);

		/// 
		bool isHostileToPlayers() const;
		/// 
		bool isNeutralToAll() const;
		/// 
		bool isFriendlyTo(const proto::FactionTemplateEntry &faction) const;
		/// 
		bool isFriendlyTo(GameUnit &unit) const;
		/// 
		bool isHostileTo(const proto::FactionTemplateEntry &faction) const;
		/// 
		bool isHostileTo(const GameUnit &unit) const;

		/// Gets the timer queue object needed for countdown events.
		TimerQueue &getTimers() {
			return m_timers;
		}
		/// Get the current race entry information.
		const proto::RaceEntry *getRaceEntry() const {
			return m_raceEntry;
		}
		/// Get the current class entry information.
		const proto::ClassEntry *getClassEntry() const {
			return m_classEntry;
		}
		///
		virtual const String &getName() const;

		bool isPet() const {
			return getUInt64Value(unit_fields::CreatedBy) != 0;
		}

		/// Starts to cast a spell using the given target map.
		void castSpell(SpellTargetMap target, UInt32 spell, const game::SpellPointsArray &basePoints = { 0, 0, 0 }, GameTime castTime = 0, bool isProc = false, UInt64 itemGuid = 0, SpellSuccessCallback callback = SpellSuccessCallback());
		/// Stops the current cast (if any).
		/// @param interruptCooldown Interrupt cooldown time in milliseconds (or 0 if no cooldown).
		void cancelCast(game::SpellInterruptFlags reason, UInt64 interruptCooldown = 0);
		/// Starts auto attack on the given target.
		/// @param target The unit to attack.
		void startAttack();
		/// Stops auto attacking the given target. Does nothing if auto attack mode
		/// isn't active right now.
		void stopAttack();
		/// Determines whether this unit is in auto attack mode right now.
		bool isAutoAttacking() const {
			return m_attackSwingCountdown.running;
		}
		/// Gets the current auto attack victim of this unit (if any).
		GameUnit *getVictim() {
			return m_victim;
		}
		/// Updates the unit's victim. Will stop auto attack if no victim is provided, otherwise,
		/// an ongoing auto attack will simply switch targets.
		void setVictim(GameUnit *victim);
		/// TODO: Move the logic of this method somewhere else.
		void triggerDespawnTimer(GameTime despawnDelay);
		/// Starts the regeneration countdown.
		void startRegeneration();
		/// Stops the regeneration countdown.
		void stopRegeneration();
		/// Gets the last time when mana was used. Used for determining mana regeneration mode.
		GameTime getLastManaUse() const {
			return m_lastManaUse;
		}
		/// Updates the time when we last used mana, so that mana regeneration mode will be changed.
		void notifyManaUse();
		/// Gets the specified unit modifier value.
		/// @param mod The unit mod we want to get.
		/// @param type Which mod type do we want to get (base, total, percentage or absolute).
		float getModifierValue(UnitMods mod, UnitModType type) const;
		/// Sets the unit modifier value to the given value.
		/// @param mod The unit mod to set.
		/// @param type Which mod type to be changed (base, total, percentage or absolute).
		/// @param value The new value of the modifier.
		void setModifierValue(UnitMods mod, UnitModType type, float value);
		/// Modifies the given unit modifier value by adding or subtracting it from the current value.
		/// @param mode The unit mod to change.
		/// @param type Which mod type to be changed (base, total, percentage or absolute).
		/// @param amount The value amount.
		/// @param apply Whether to apply or remove the provided amount.
		void updateModifierValue(UnitMods mod, UnitModType type, float amount, bool apply);
		/// Deals damage to this unit. Does not work on dead units!
		/// @param damage The damage value to deal.
		/// @param school The damage school mask.
		/// @param attacker The attacking unit or nullptr, if unknown. If nullptr, no threat will be generated.
		/// @param noThreat If set to true, no threat will be generated from this damage.
		bool dealDamage(UInt32 damage, UInt32 school, game::DamageType damageType, GameUnit *attacker, float threat);
		/// Heals this unit. Does not work on dead units! Use the revive method for this one.
		/// @param amount The amount of damage to heal.
		/// @param healer The healing unit or nullptr, if unknown. If nullptr, no threat will be generated.
		/// @param noThreat If set to true, no threat will be generated.
		bool heal(UInt32 amount, GameUnit *healer, bool noThreat = false);
		/// Add or Remove mana to unit. Does not work on dead units!
		/// @param amount The amount of power. Negative values will remove power
		Int32 addPower(game::PowerType power, Int32 amount);
		/// Revives this unit with the given amount of health and mana. Does nothing if the unit is alive.
		/// @param health The new health value of this unit.
		/// @param mana The new mana value of this unit. If set to 0, mana won't be changed. If unit does not use
		///             mana as it's resource, this value is ignored.
		void revive(UInt32 health, UInt32 mana);
		/// Rewards experience points to this unit.
		/// @param experience The amount of experience points to be added.
		virtual void rewardExperience(GameUnit *victim, UInt32 experience);
		/// Gets the aura container of this unit.
		inline AuraContainer &getAuras() {
			return m_auras;
		}
		inline const AuraContainer &getAuras() const {
			return m_auras;
		}
		///
		bool isAlive() const {
			return (getUInt32Value(unit_fields::Health) != 0);
		}
		/// Determines whether this unit is actually in combat with at least one other unit.
		bool isInCombat() const;
		/// Determines whether another game object is in line of sight.
		bool isInLineOfSight(const GameObject &other) const;
		/// Determines whether a position is in line of sight.
		bool isInLineOfSight(const math::Vector3 &position) const;
		///
		bool isMounted() const {
			return getUInt32Value(unit_fields::MountDisplayId) != 0;
		}
		/// 
		bool isStealthed() const {
			return m_isStealthed;
		}
		/// 
		void notifyStealthChanged();
		/// 
		virtual bool canDetectStealth(GameUnit &target) const;

		/// 
		float getMissChance(GameUnit &attacker, UInt8 school, bool isWhiteDamage);
		/// 
		bool isImmune(UInt8 school);
		/// 
		float getDodgeChance(GameUnit &attacker);
		float getParryChance(GameUnit &attacker);
		/// 
		float getGlancingChance(GameUnit &attacker);
		/// 
		float getBlockChance();
		/// 
		float getCrushChance(GameUnit &attacker);
		/// 
		float getResiPercentage(const proto::SpellEntry &spell, GameUnit &attacker, bool isBinary);
		/// 
		float getCritChance(GameUnit &attacker, UInt8 school);
		///
		UInt32 getAttackTime(UInt8 attackType);
		/// 
		UInt32 getBonus(UInt8 school);
		/// 
		UInt32 getBonusPct(UInt8 school);
		/// 
		UInt32 consumeAbsorb(UInt32 damage, UInt8 school);
		/// 
		UInt32 calculateArmorReducedDamage(UInt32 attackerLevel, UInt32 damage);
		/// 
		virtual bool canBlock() const = 0;
		/// 
		virtual bool canParry() const = 0;
		/// 
		virtual bool canDodge() const = 0;
		/// 
		virtual bool canDualWield() const = 0;

		/// 
		virtual bool hasMainHandWeapon() const { return false; }
		/// 
		virtual bool hasOffHandWeapon() const { return false; }
		/// 
		virtual std::shared_ptr<GameItem> getMainHandWeapon() const { return nullptr; }
		/// 
		virtual std::shared_ptr<GameItem> getOffHandWeapon() const { return nullptr; }

		/// Determines if the unit has a stun spell effect on it.
		inline bool isStunned() const {
			return (m_state & unit_state::Stunned);
		}
		/// Determines if a root aura has been applied to the unit. This is an effect that
		/// is immediatly updated on aura changes no matter what and should not be used
		/// for movement related code.
		inline bool isRootedForSpell() const {
			return (m_state & unit_state::Rooted);
		}
		/// Determine whether the root flag is present in the movement info. This flag
		/// is delayed for player characters until the client has ackknowledged the root.
		inline bool isRootedForMovement() const {
			return (m_movementInfo.moveFlags & game::movement_flags::Root) != 0;
		}
		/// Determines whether this unit is feared.
		inline bool isFeared() const {
			return (m_state & unit_state::Feared);
		}
		/// Determines whether this unit is confused.
		inline bool isConfused() const {
			return (m_state & unit_state::Confused);
		}
		/// Determines if auto attacks are possible.
		inline bool canAutoAttack() const {
			return isAlive() && !isFeared() && !isStunned() && !isConfused();
		}

		/// Called by spell auras to notify that a stun aura has been applied or misapplied.
		/// This performs checks whether another stun aura is still present and updates the
		/// internal state.
		void notifyStunChanged();
		/// Called by spell auras to notify that a root aura has been applied or misapplied.
		/// This performs checks whether another root aura is still present and updates the
		/// internal state.
		void notifyRootChanged();
		/// Called by spell auras to notify that a fear aura has been applied or misapplied.
		/// This performs checks whether another fear aura is still present and updates the
		/// internal state.
		void notifyFearChanged();
		/// Called by spell auras to notify that a fear aura has been applied or misapplied.
		/// This performs checks whether another fear aura is still present and updates the
		/// internal state.
		void notifyConfusedChanged();
		/// Called by spell auras to notify that a speed aura has been applied or misapplied.
		/// If this unit is player controlled, a client notification is sent and the speed is
		/// only changed after the client has ackknowledged this change. Otherwise, the speed
		/// is immediatly applied using applySpeedChange.
		void notifySpeedChanged(MovementType type, bool initial = false);
		/// Immediatly applies a speed change for this unit. Never call this method directly
		/// unless you know what you're doing.
		void applySpeedChange(MovementType type, float speed, bool initial = false);
		/// Gets the current (applied) movement speed in units per second.
		float getSpeed(MovementType type) const;
		/// Gets the current expected speed value based on movement informations. Mainly
		/// used in anti cheat code to detect speed / teleport hacks.
		/// @param info The movement info to use. This paramater is required because we might want to
		///             calulate the speed based on client movement infos which haven't been applied to
		///             this unit yet.
		/// @param expectFallingFar If set to true, the movement info is handled as if far falling is activated.
		///                         This is needed to accept higher xy changes for falling.
		float getExpectedSpeed(const MovementInfo& info, bool expectFallingFar) const;
		/// Gets the base movement speed in units per second.
		virtual float getBaseSpeed(MovementType type) const;

		/// 
		void addMechanicImmunity(UInt32 mechanic);
		/// 
		void removeMechanicImmunity(UInt32 mechanic);
		/// 
		bool isImmuneAgainstMechanic(UInt32 mechanic) const;

		/// Adds a unit to the list of attacking units. This list is used to generate threat
		/// if this unit is healed by another unit, and to determine, whether a player should
		/// stay in combat. This method adds may add game::unit_flags::InCombat
		/// @param attacker The attacking unit.
		void addAttackingUnit(GameUnit &attacker);
		/// Forces an attacking unit to be removed from the list of attackers. Note that attacking
		/// units are removed automatically on despawn and/or death, so this is more useful in PvP
		/// scenarios. This method may remove game::unit_flags::InCombat
		/// @param removed The attacking unit to be removed from the list of attackers.
		void removeAttackingUnit(GameUnit &removed);
		/// Determines whether this unit is attacked by other units.
		bool hasAttackingUnits() const;
		/// Gets the number of attacking units.
		UInt32 attackingUnitCount() const;

		/// Calculates the stat based on the specified modifier.
		static UInt8 getStatByUnitMod(UnitMods mod);
		/// Calculates the resistance based on the specified modifier.
		static UInt8 getResistanceByUnitMod(UnitMods mod);
		/// Calculates the power based on the specified unit modifier.
		static game::PowerType getPowerTypeByUnitMod(UnitMods mod);
		///
		static UnitMods getUnitModByStat(UInt8 stat);
		///
		static UnitMods getUnitModByPower(game::PowerType power);
		///
		static UnitMods getUnitModByResistance(UInt8 res);

		/// Determines whether the unit is sitting.
		bool isSitting() const;
		/// Changes the units stand state. This might also remove auras.
		void setStandState(UnitStandState state);

		///
		void setAttackSwingCallback(AttackSwingCallback callback);

		/// 
		virtual void updateAllStats();
		/// 
		virtual void updateMaxHealth();
		/// 
		virtual void updateMaxPower(game::PowerType power);
		/// 
		virtual void updateArmor();
		/// 
		virtual void updateDamage();
		/// 
		virtual void updateManaRegen();
		/// 
		virtual void updateStats(UInt8 stat);
		/// 
		virtual void updateResistance(UInt8 resistance);
		///
		virtual void updateAttackSpeed();
		///
		virtual void updateCritChance(game::WeaponAttack attackType);
		///
		virtual void updateAllCritChances();
		///
		virtual void updateSpellCritChance(game::SpellSchool spellSchool);
		///
		virtual void updateAllSpellCritChances();
		///
		virtual void updateDodgePercentage();
		///
		virtual void updateParryPercentage();
		///
		virtual void updateAllRatings();

		virtual void applyDamageDoneBonus(UInt32 schoolMask, UInt32 tickCount, UInt32 &damage);

		virtual void applyDamageTakenBonus(UInt32 schoolMask, UInt32 tickCount, UInt32 &damage);

		virtual void applyHealingTakenBonus(UInt32 tickCount, UInt32 &healing);

		virtual void applyHealingDoneBonus(UInt32 tickCount, UInt32 &healing);

		virtual void applyHealingDoneBonus(UInt32 spellLevel, UInt32 playerLevel, UInt32 tickCount, UInt32 &healing);

		virtual UInt32 getWeaponSkillValue(game::WeaponAttack attackType, const GameUnit &target) const {
			return getUnitMeleeSkill(target);
		}

		virtual UInt32 getDefenseSkillValue(const GameUnit &attacker) const {
			return getMaxWeaponSkillValueForLevel();
		}

		/// Gets the current unit mover.
		UnitMover &getMover() {
			return *m_mover;
		}

		/// Determines whether the unit has a cooldown on a specific spell.
		/// @param spellId The ID of the spell to check.
		/// @returns true, if there is a remaining cooldown.
		bool hasCooldown(UInt32 spellId) const;
		/// Gets the remaining cooldown time in milliseconds for a specific spell.
		/// @param spellId The ID of the spell to check.
		/// @returns 0 if there is no active cooldown for that spell.
		UInt32 getCooldown(UInt32 spellId) const;
		/// Sets the cooldown time in milliseconds for a specific spell.
		/// @param spellId The spell to set the cooldown time for.
		/// @param timeInMs Cooldown time in milliseconds. Use 0 to clear the cooldown.
		void setCooldown(UInt32 spellId, UInt32 timeInMs);
		/// Gets a constant map of all cooldown entries.
		const CooldownMap &getCooldowns() const {
			return m_spellCooldowns;
		}

		/// Adds a new owned world object to this unit. This means, that if this unit despawns,
		/// all assigned world objects will despawn as well.
		void addWorldObject(std::shared_ptr<WorldObject> object);

		/// Enables or disables flight mode.
		/// @param enable Whether flight mode will be enabled.
		void setFlightMode(bool enable);
		void setPendingMovementFlag(MovementChangeType type, bool enable);

		///
		void procEvent(GameUnit *target, UInt32 procAttacker, UInt32 procVictim, UInt32 procEx, UInt32 amount, UInt8 attackType, const proto::SpellEntry *procSpell, bool canRemove);
		///
		void procEventFor(bool isVictim, GameUnit *target, UInt32 procFlag, UInt32 procEx, UInt32 amount, UInt8 attackType, proto::SpellEntry const *procSpell, bool canRemove);

		///
		TrackAuraTargetsMap &getTrackedAuras() {
			return m_trackAuraTargets;
		}

		///
		void finishChanneling();

		///
		bool isAttackable() const;

		///
		bool isInFeralForm() const;

		///
		bool canUseWeapon(game::WeaponAttack attackType);

		/// TODO: Do this properly, return level of target if found (can be a boss, not sure how it'll be done)
		UInt16 getMaxWeaponSkillValueForLevel() const {
			return static_cast<UInt16>(getLevel() * 5);
		}

		///
		game::WeaponAttack getWeaponAttack() const {
			return m_weaponAttack;
		}

		///
		void setWeaponAttack(game::WeaponAttack weaponAttack) {
			m_weaponAttack = weaponAttack;
		}

		/// TODO: Do this properly, return level of target if found (can be a boss, not sure how it'll be done)
		UInt32 getUnitMeleeSkill(const GameUnit &target) const {
			return target.getLevel() * 5;
		}

		/// Determines if this unit is in evade mode.
		virtual bool isEvading() const {
			return false;
		}
		/// Determines if this unit is currently in walk mode.
		bool isInWalkMode() const { 
			return (m_movementInfo.moveFlags & game::movement_flags::WalkMode) != 0;
		}
		/// Determines if this unit is moving at all.
		bool isMoving() const;

		void modifyAuraState(game::AuraState state, bool apply);


		/// Calculates a position relative to the units location based on it's current orientation.
		math::Vector3 getRelativeLocation(float forwardDist, float rightDist, float upDist) const;
		/// Returns the location of this unit with movement prediction applied.
		/// @param seconds The returned location will be calculated when the unit 
		///                will continue it's current movement for this amount of time.
		math::Vector3 getPredictedPosition(float seconds) const;

		/// Gets the next pending movement change and removes it from the queue of pending movement changes.
		/// You need to make sure that there are any pending changes before calling this method.
		PendingMovementChange popPendingMovementChange();
		/// Pushes a new pending movement change to the queue.
		/// @param change The new pending movement change to push.
		void pushPendingMovementChange(PendingMovementChange change);
		/// Determines whether there are any pending movement changes at all.
		inline bool hasPendingMovementChange() const { return !m_pendingMoveChanges.empty(); }
		/// Determines whether there is a timed out pending movement change.
		bool hasTimedOutPendingMovementChange() const;

		/// Sets or unsets a net unit watcher instance.
		/// @param watcher Pointer to the watcher instance to be notified or nullptr.
		void setNetUnitWatcher(INetUnitWatcher* watcher);

	public:

		/// @copydoc GameObject::relocate
		virtual void relocate(const math::Vector3 &position, float o, bool fire = true) override;

	public:

		/// 
		virtual void levelChanged(const proto::LevelEntry &levelInfo);
		/// 
		virtual void onKilled(GameUnit *killer);

		/// 
		float getHealthBonusFromStamina() const;
		/// 
		float getManaBonusFromIntellect() const;
		/// 
		float getMeleeReach() const;
		/// Determines if this unit is interactable for another unit.
		bool isInteractableFor(const GameUnit &interactor) const;

	protected:

		///
		virtual void raceUpdated();
		/// 
		virtual void classUpdated();
		/// 
		virtual void onThreat(GameUnit &threatener, float amount);
		/// 
		virtual void onRegeneration();

	private:
		/// 
		void updateDisplayIds();
		/// 
		void onDespawnTimer();
		/// 
		void onVictimKilled(GameUnit *killer);
		/// 
		void onVictimDespawned();
		/// 
		void onAttackSwing();
		/// 
		virtual void regenerateHealth() = 0;
		/// 
		void regeneratePower(game::PowerType power);
		/// 
		void onSpellCastEnded(bool succeeded);
		/// 
		void triggerNextAutoAttack();
		/// 
		void triggerNextFearMove();

	public:
		/// Adds a new dynamic object instance and spawns it in the units world (if any).
		void addDynamicObject(std::shared_ptr<DynObject> object);
		/// Despawns and probably deletes a dynamic object instance by it's guid.
		void removeDynamicObject(UInt64 objectGuid);
		/// Despawns and probably deletes all dynamic object instances.
		void removeAllDynamicObjects();

	public:
		/// Generates the next client ack id for this unit.
		inline UInt32 generateAckId() { return m_ackGenerator.generateId(); }
		
	private:

		typedef std::array<float, unit_mod_type::End> UnitModTypeArray;
		typedef std::array<UnitModTypeArray, unit_mods::End> UnitModArray;
		typedef std::vector<std::shared_ptr<AuraEffect>> AuraVector;
		typedef LinearSet<GameUnit *> AttackingUnitSet;
		typedef LinearSet<UInt32> MechanicImmunitySet;

		TimerQueue &m_timers;
		std::unique_ptr<UnitMover> m_mover;
		const proto::RaceEntry *m_raceEntry;
		const proto::ClassEntry *m_classEntry;
		const proto::FactionTemplateEntry *m_factionTemplate;
		std::unique_ptr<SpellCast> m_spellCast;
		Countdown m_despawnCountdown;
		simple::scoped_connection m_victimDespawned;
		simple::scoped_connection m_victimDied, m_fearMoved;
		GameUnit *m_victim;
		Countdown m_attackSwingCountdown;
		GameTime m_lastMainHand, m_lastOffHand;
		game::WeaponAttack m_weaponAttack;
		Countdown m_regenCountdown;
		GameTime m_lastManaUse;
		UnitModArray m_unitMods;
		AuraContainer m_auras;
		AttackSwingCallback m_swingCallback;
		AttackingUnitSet m_attackingUnits;
		UInt32 m_mechanicImmunity;
		bool m_isStealthed;
		UInt32 m_state;
		std::array<float, movement_type::Count> m_speedBonus;
		CooldownMap m_spellCooldowns;
		UnitStandState m_standState;
		std::vector<std::shared_ptr<WorldObject>> m_worldObjects;
		math::Vector3 m_confusedLoc;
		TrackAuraTargetsMap m_trackAuraTargets;
		std::map<UInt64, std::shared_ptr<DynObject>> m_dynamicObjects;
		IdGenerator<UInt32> m_ackGenerator;
		std::list<PendingMovementChange> m_pendingMoveChanges;
		INetUnitWatcher* m_netWatcher;
	};

	io::Writer &operator << (io::Writer &w, GameUnit const &object);
	io::Reader &operator >> (io::Reader &r, GameUnit &object);
}
