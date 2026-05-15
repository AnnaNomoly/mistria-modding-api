// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Bark.hpp"
#include "Core.hpp"
#include "Engine.hpp"
#include "Hook.hpp"
#include "Instance.hpp"
#include "Log.hpp"
#include "Spell.hpp"
#include "Status.hpp"

#include <optional>
#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Player
{
	/// Source: globalInstance.__player_state__
	enum class States : int
	{
		Default        = 0,
		Celebrate      = 1,
		Cutscene       = 2,
		Knockback      = 3,
		Jump           = 4,
		DownSmash      = 5,
		Hurt           = 6,
		Electrocute    = 7,
		AnimateAndThen = 8,
		Swim           = 9,
		ThrowItem      = 10,
		HoldToUse      = 11,
		Tool           = 12,
		Sword          = 13,
		Fishing        = 14,
		WhirlPool      = 15,
		Spell          = 16,
		Diving         = 17,
		Underwater     = 18,
		Resurface      = 19,
		Pathfind       = 20,
		Dummy          = 21,
		MountDefault   = 22,
		MountJump      = 23
	};

	/// Total number of enumerators in States. Iterating [0, StateCount) covers every States value.
	inline constexpr int StateCount = 24;

	/// Invokes fn with every States value, in ascending order.
	template <typename Fn>
	inline void ForEachState(Fn fn)
	{
		for (int i = 0; i < StateCount; ++i)
			fn(static_cast<States>(i));
	}

	/// Tool kinds that drive Ari's per-tool FSM actions in `AriFsm`. Each value corresponds to one
	/// of the anonymous closures the game registers as the tool-action handler for that tool
	/// (e.g. `Tool::Hoe` ↔ `gml_Script_hoe@anon@84872@AriFsm@AriFsm`).
	enum class Tool : int
	{
		Hoe         = 0,
		Axe         = 1,
		PickAxe     = 2,
		Shovel      = 3,
		Net         = 4,
		WateringCan = 5,
		Sow         = 6,
	};

	/// Total number of enumerators in Tool. Iterating [0, ToolCount) covers every Tool value.
	inline constexpr int ToolCount = 7;

	/// Invokes fn with every Tool value, in ascending order.
	template <typename Fn>
	inline void ForEachTool(Fn fn)
	{
		for (int i = 0; i < ToolCount; ++i)
			fn(static_cast<Tool>(i));
	}

	/// Compass direction Ari can face. The integer values match the game's `cardinal` field used by
	/// `obj_ari.set_cardinal` — East=0, North=1, West=2, South=3 (GameMaker convention: 0°=right,
	/// 90°=up, counterclockwise).
	enum class Cardinal : int
	{
		East  = 0,
		North = 1,
		West  = 2,
		South = 3,
	};

	/// Returns the GameMaker-degree angle a cardinal direction faces.
	/// East = 0°, North = 90°, West = 180°, South = 270°.
	inline constexpr double CardinalToDegrees(Cardinal c)
	{
		switch (c)
		{
			case Cardinal::East:  return   0.0;
			case Cardinal::North: return  90.0;
			case Cardinal::West:  return 180.0;
			case Cardinal::South: return 270.0;
		}
		return 0.0;
	}

	struct Position
	{
		double x = 0.0;
		double y = 0.0;
	};

	struct MoveSpeedContext
	{
		double m_result = 0.0;
		double m_modifier_sum = 0.0;
		bool m_has_override = false;
		double m_override_value = 0.0;

		/// Returns the movement speed calculated by the game before any MMAPI modifications.
		double GetResult() const { return m_result; }

		/// Adds a signed value to the game's calculated movement speed. Accumulates across multiple callbacks.
		void AddModifier(double value) { m_modifier_sum += value; }

		/// Overrides the final movement speed to the given value, ignoring any accumulated modifiers.
		/// If multiple callbacks call SetOverride, the last one wins.
		void SetOverride(double value)
		{
			m_has_override = true;
			m_override_value = value;
		}
	};

	struct BeforeHealthChangeContext
	{
		double m_amount = 0.0;

		/// Returns the health change amount. Negative values are damage; positive values are healing.
		double GetAmount() const { return m_amount; }

		/// Overrides the health change amount passed to the game.
		void SetAmount(double amount) { m_amount = amount; }
	};

	struct AfterHealthChangeContext
	{
		double m_amount = 0.0;

		/// Returns the health change amount the game's modify_health script saw (post any
		/// BeforeHealthChange mutations). Negative values are damage; positive values are healing.
		double GetAmount() const { return m_amount; }
	};

	struct BeforeStaminaChangeContext
	{
		double m_amount = 0.0;

		/// Returns the stamina change amount. Negative values are costs; positive values are recovery.
		double GetAmount() const { return m_amount; }

		/// Overrides the stamina change amount passed to the game.
		void SetAmount(double amount) { m_amount = amount; }
	};

	struct AfterStaminaChangeContext
	{
		double m_amount = 0.0;

		/// Returns the stamina change amount the game's modify_stamina script saw (post any
		/// BeforeStaminaChange mutations). Useful as a "did stamina actually get consumed?" signal —
		/// if a BeforeStaminaChange callback zeroed the amount, this returns 0, otherwise the live
		/// applied value.
		double GetAmount() const { return m_amount; }
	};

	struct ToolActionContext
	{
		MMAPI::Player::Tool m_tool = MMAPI::Player::Tool::Hoe;

		/// Returns the tool whose AriFsm action just fired.
		MMAPI::Player::Tool GetTool() const { return m_tool; }
	};

	struct NodeInteractionContext
	{
		double m_damage_modifier = 0.0;

		/// Returns the damage modifier the game is about to apply to the node interaction
		/// (Arguments[4] on pick_node / chop_node). Negative values are the "charged tool damage
		/// penalty" the game applies when the action covers a wider area at reduced per-hit damage.
		double GetDamageModifier() const { return m_damage_modifier; }

		/// Overrides the damage modifier — writes through to the script's Arguments[4]. Use to
		/// clamp the penalty (e.g. `SetDamageModifier(0.0)` to neutralize negative penalties).
		void SetDamageModifier(double value) { m_damage_modifier = value; }
	};

	struct BeforeManaChangeContext
	{
		double m_amount = 0.0;

		/// Returns the mana change amount. Negative values are costs; positive values are recovery.
		double GetAmount() const { return m_amount; }

		/// Overrides the mana change amount passed to the game.
		void SetAmount(double amount) { m_amount = amount; }
	};

	struct FaceDirContext
	{
		double m_direction_degrees = 0.0;

		/// Returns the direction the game's face_dir script was called with, in GameMaker degrees
		/// (0 = right, 90 = up, 180 = left, 270 = down).
		double GetDirectionDegrees() const { return m_direction_degrees; }
	};

	struct HeldItemContext
	{
		int m_item_id = -1;

		/// Returns the item_id of Ari's currently held item, or -1 if the game's held_item script
		/// returned an undefined Result (no item held) or the Result struct lacks an `item_id` member.
		int GetItemId() const { return m_item_id; }
	};

	struct AfterShouldDieContext
	{
		bool m_will_die = false;

		/// Returns the game's verdict — true if Ari is about to die from this hit/event.
		bool GetWillDie() const { return m_will_die; }
	};

	struct AfterUseActionContext
	{
		int               m_item_id = -1;
		YYTK::CInstance*  m_self    = nullptr;

		/// Returns the item_id of the item Ari was using when the action completed. Sourced from
		/// the most recent non-undefined `held_item` callback, so it remains the consumed item
		/// even though the game has already removed it from inventory by the time this fires.
		/// -1 only if no held_item value has ever been observed this session (e.g. the action
		/// somehow fired before the game's first held_item resolution).
		int GetItemId() const { return m_item_id; }

		/// Ari's obj_ari CInstance — useful for invoking follow-up scripts that need the obj_ari calling context.
		YYTK::CInstance* GetSelf() const { return m_self; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_GET_HEALTH             = "gml_Script_get_health@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_SET_HEALTH             = "gml_Script_set_health@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_MODIFY_HEALTH          = "gml_Script_modify_health@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_GET_MAX_HEALTH         = "gml_Script_get_max_health@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_GET_STAMINA            = "gml_Script_get_stamina@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_MODIFY_STAMINA         = "gml_Script_modify_stamina@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_MODIFY_GOLD            = "gml_Script_modify_gold@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_MODIFY_RENOWN          = "gml_Script_modify_renown@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_GET_MANA               = "gml_Script_get_mana@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_MODIFY_MANA            = "gml_Script_modify_mana@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_GET_ESSENCE            = "gml_Script_get_essence@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_MODIFY_ESSENCE         = "gml_Script_modify_essence@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_GET_MOVE_SPEED         = "gml_Script_get_move_speed@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_HELD_ITEM              = "gml_Script_held_item@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_FACE_DIR               = "gml_Script_face_dir@gml_Object_obj_ari_Create_0";
		inline constexpr const char* GML_SCRIPT_SET_CARDINAL           = "gml_Script_set_cardinal@gml_Object_obj_ari_Create_0";
		inline constexpr const char* GML_SCRIPT_SHOULD_DIE             = "gml_Script_should_die@gml_Object_obj_ari_Create_0";

		// AriFsm tool-action closures. The `@anon@84872@AriFsm@AriFsm` suffix is the compiler-assigned
		// anonymous-function id for the action handler registered in AriFsm's Tool-state dispatch
		// table. The numeric id is stable across game patches that don't reorder AriFsm's scripts —
		// re-dump from globalInstance if a patch shifts the id and these constants stop resolving.
		inline constexpr const char* GML_SCRIPT_TOOL_HOE          = "gml_Script_hoe@anon@84872@AriFsm@AriFsm";
		inline constexpr const char* GML_SCRIPT_TOOL_AXE          = "gml_Script_axe@anon@84872@AriFsm@AriFsm";
		inline constexpr const char* GML_SCRIPT_TOOL_PICK_AXE     = "gml_Script_pick_axe@anon@84872@AriFsm@AriFsm";
		inline constexpr const char* GML_SCRIPT_TOOL_SHOVEL       = "gml_Script_shovel@anon@84872@AriFsm@AriFsm";
		inline constexpr const char* GML_SCRIPT_TOOL_NET          = "gml_Script_net@anon@84872@AriFsm@AriFsm";
		inline constexpr const char* GML_SCRIPT_TOOL_WATERING_CAN = "gml_Script_watering_can@anon@84872@AriFsm@AriFsm";
		inline constexpr const char* GML_SCRIPT_TOOL_SOW          = "gml_Script_sow@anon@84872@AriFsm@AriFsm";

		inline constexpr const char* GML_SCRIPT_PICK_NODE = "gml_Script_pick_node";
		inline constexpr const char* GML_SCRIPT_CHOP_NODE = "gml_Script_chop_node";
		using AfterMoveSpeedCallback     = void(*)(MMAPI::Player::MoveSpeedContext&);
		using BeforeHealthChangeCallback  = void(*)(MMAPI::Player::BeforeHealthChangeContext&);
		using AfterHealthChangeCallback   = void(*)(MMAPI::Player::AfterHealthChangeContext&);
		using BeforeStaminaChangeCallback = void(*)(MMAPI::Player::BeforeStaminaChangeContext&);
		using AfterStaminaChangeCallback  = void(*)(MMAPI::Player::AfterStaminaChangeContext&);
		using BeforeManaChangeCallback    = void(*)(MMAPI::Player::BeforeManaChangeContext&);
		using BeforeFaceDirCallback       = void(*)(MMAPI::Player::FaceDirContext&);
		using AfterHeldItemCallback           = void(*)(MMAPI::Player::HeldItemContext&);
		using AfterUseActionCompleteCallback  = void(*)(MMAPI::Player::AfterUseActionContext&);
		using AfterShouldDieCallback          = void(*)(MMAPI::Player::AfterShouldDieContext&);
		using BeforeToolActionCallback        = void(*)(MMAPI::Player::ToolActionContext&);
		using AfterToolActionCallback         = void(*)(MMAPI::Player::ToolActionContext&);
		using BeforeNodeInteractionCallback   = void(*)(MMAPI::Player::NodeInteractionContext&);

		inline AfterMoveSpeedCallback         after_move_speed_callback             = nullptr;
		inline BeforeHealthChangeCallback     before_health_change_callback         = nullptr;
		inline AfterHealthChangeCallback      after_health_change_callback          = nullptr;
		inline BeforeStaminaChangeCallback    before_stamina_change_callback        = nullptr;
		inline AfterStaminaChangeCallback     after_stamina_change_callback         = nullptr;
		inline BeforeManaChangeCallback       before_mana_change_callback           = nullptr;
		inline BeforeFaceDirCallback          before_face_dir_callback              = nullptr;
		inline AfterHeldItemCallback          after_held_item_callback              = nullptr;
		inline AfterUseActionCompleteCallback after_use_action_complete_callback    = nullptr;
		inline AfterShouldDieCallback         after_should_die_callback             = nullptr;
		inline BeforeToolActionCallback       before_tool_action_callback           = nullptr;
		inline AfterToolActionCallback        after_tool_action_callback            = nullptr;
		inline BeforeNodeInteractionCallback  before_pick_node_callback             = nullptr;
		inline BeforeNodeInteractionCallback  before_chop_node_callback             = nullptr;

		// Edge-detect state for the use-action-complete signal: true when the previous obj_ari tick
		// observed (state == HoldToUse && state.did_action). Cleared on return to title.
		inline bool was_in_use_action = false;

		// Most recent non-undefined item_id observed from the held_item script. The script returns
		// undefined when Ari has nothing held — including the brief window after consuming a
		// consumable but before the next slot's item resolves. Tracking the last known value lets
		// AfterUseActionComplete report the item that was being used even if the game has already
		// removed it from inventory by the time `did_action` rises. Cleared on return to title.
		inline int last_known_held_item_id = -1;

		inline YYTK::RValue GetStateId()
		{
			const auto& refs = MMAPI::Internal::instance_reference_map;
			if (!refs.contains(MMAPI::Instance::Internal::INSTANCE_OBJ_ARI))
				return {};
			YYTK::CInstance* Ari = refs.at(MMAPI::Instance::Internal::INSTANCE_OBJ_ARI)[0];

			YYTK::RValue ari = Ari->ToRValue();
			YYTK::RValue fsm = ari.GetMember("fsm");
			YYTK::RValue state = fsm.GetMember("state");
			return state.GetMember("state_id");
		}

		inline YYTK::RValue& GmlScriptGetMoveSpeedCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(
					MMAPI::Internal::self_module,
					GML_SCRIPT_GET_MOVE_SPEED
				)
			);

			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_move_speed_callback && MMAPI::Engine::IsNumeric(Result))
			{
				MMAPI::Player::MoveSpeedContext context{ Result.ToDouble() };
				after_move_speed_callback(context);
				Result = context.m_has_override ? context.m_override_value : context.m_result + context.m_modifier_sum;
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptModifyHealthCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_health_change_callback && Arguments && ArgumentCount >= 1 && Arguments[0] && MMAPI::Engine::IsNumeric(*Arguments[0]))
			{
				MMAPI::Player::BeforeHealthChangeContext context{ Arguments[0]->ToDouble() };
				before_health_change_callback(context);
				*Arguments[0] = context.m_amount;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(
					MMAPI::Internal::self_module,
					GML_SCRIPT_MODIFY_HEALTH
				)
			);

			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_health_change_callback && Arguments && ArgumentCount >= 1 && Arguments[0] && MMAPI::Engine::IsNumeric(*Arguments[0]))
			{
				MMAPI::Player::AfterHealthChangeContext context{ Arguments[0]->ToDouble() };
				after_health_change_callback(context);
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptModifyStaminaCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_stamina_change_callback && Arguments && ArgumentCount >= 1 && Arguments[0] && MMAPI::Engine::IsNumeric(*Arguments[0]))
			{
				MMAPI::Player::BeforeStaminaChangeContext context{ Arguments[0]->ToDouble() };
				before_stamina_change_callback(context);
				*Arguments[0] = context.m_amount;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(
					MMAPI::Internal::self_module,
					GML_SCRIPT_MODIFY_STAMINA
				)
			);

			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_stamina_change_callback && Arguments && ArgumentCount >= 1 && Arguments[0] && MMAPI::Engine::IsNumeric(*Arguments[0]))
			{
				MMAPI::Player::AfterStaminaChangeContext context{ Arguments[0]->ToDouble() };
				after_stamina_change_callback(context);
			}

			return Result;
		}

		// Shared dispatcher for the 7 AriFsm tool-action closures. The thunks below pin the Tool kind
		// and script name; the helper just fires the Before/After user callbacks around the trampoline.
		inline YYTK::RValue& DispatchToolAction(
			MMAPI::Player::Tool tool,
			const char* script_name,
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_tool_action_callback)
			{
				MMAPI::Player::ToolActionContext context{ tool };
				before_tool_action_callback(context);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, script_name)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_tool_action_callback)
			{
				MMAPI::Player::ToolActionContext context{ tool };
				after_tool_action_callback(context);
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptHoeToolActionCallback(IN YYTK::CInstance* Self, IN YYTK::CInstance* Other, OUT YYTK::RValue& Result, IN int ArgumentCount, IN YYTK::RValue** Arguments)
		{
			return DispatchToolAction(MMAPI::Player::Tool::Hoe, GML_SCRIPT_TOOL_HOE, Self, Other, Result, ArgumentCount, Arguments);
		}

		inline YYTK::RValue& GmlScriptAxeToolActionCallback(IN YYTK::CInstance* Self, IN YYTK::CInstance* Other, OUT YYTK::RValue& Result, IN int ArgumentCount, IN YYTK::RValue** Arguments)
		{
			return DispatchToolAction(MMAPI::Player::Tool::Axe, GML_SCRIPT_TOOL_AXE, Self, Other, Result, ArgumentCount, Arguments);
		}

		inline YYTK::RValue& GmlScriptPickAxeToolActionCallback(IN YYTK::CInstance* Self, IN YYTK::CInstance* Other, OUT YYTK::RValue& Result, IN int ArgumentCount, IN YYTK::RValue** Arguments)
		{
			return DispatchToolAction(MMAPI::Player::Tool::PickAxe, GML_SCRIPT_TOOL_PICK_AXE, Self, Other, Result, ArgumentCount, Arguments);
		}

		inline YYTK::RValue& GmlScriptShovelToolActionCallback(IN YYTK::CInstance* Self, IN YYTK::CInstance* Other, OUT YYTK::RValue& Result, IN int ArgumentCount, IN YYTK::RValue** Arguments)
		{
			return DispatchToolAction(MMAPI::Player::Tool::Shovel, GML_SCRIPT_TOOL_SHOVEL, Self, Other, Result, ArgumentCount, Arguments);
		}

		inline YYTK::RValue& GmlScriptNetToolActionCallback(IN YYTK::CInstance* Self, IN YYTK::CInstance* Other, OUT YYTK::RValue& Result, IN int ArgumentCount, IN YYTK::RValue** Arguments)
		{
			return DispatchToolAction(MMAPI::Player::Tool::Net, GML_SCRIPT_TOOL_NET, Self, Other, Result, ArgumentCount, Arguments);
		}

		inline YYTK::RValue& GmlScriptWateringCanToolActionCallback(IN YYTK::CInstance* Self, IN YYTK::CInstance* Other, OUT YYTK::RValue& Result, IN int ArgumentCount, IN YYTK::RValue** Arguments)
		{
			return DispatchToolAction(MMAPI::Player::Tool::WateringCan, GML_SCRIPT_TOOL_WATERING_CAN, Self, Other, Result, ArgumentCount, Arguments);
		}

		inline YYTK::RValue& GmlScriptSowToolActionCallback(IN YYTK::CInstance* Self, IN YYTK::CInstance* Other, OUT YYTK::RValue& Result, IN int ArgumentCount, IN YYTK::RValue** Arguments)
		{
			return DispatchToolAction(MMAPI::Player::Tool::Sow, GML_SCRIPT_TOOL_SOW, Self, Other, Result, ArgumentCount, Arguments);
		}

		// pick_node / chop_node share a context shape but live as separate game scripts. Both expose
		// the damage modifier as Arguments[4]; mods read/write it via the shared NodeInteractionContext.
		inline YYTK::RValue& DispatchNodeInteraction(
			BeforeNodeInteractionCallback callback,
			const char* script_name,
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (callback && Arguments && ArgumentCount >= 5 && Arguments[4] && MMAPI::Engine::IsNumeric(*Arguments[4]))
			{
				MMAPI::Player::NodeInteractionContext context{ Arguments[4]->ToDouble() };
				callback(context);
				*Arguments[4] = context.m_damage_modifier;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, script_name)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

		inline YYTK::RValue& GmlScriptBeforePickNodeCallback(IN YYTK::CInstance* Self, IN YYTK::CInstance* Other, OUT YYTK::RValue& Result, IN int ArgumentCount, IN YYTK::RValue** Arguments)
		{
			return DispatchNodeInteraction(before_pick_node_callback, GML_SCRIPT_PICK_NODE, Self, Other, Result, ArgumentCount, Arguments);
		}

		inline YYTK::RValue& GmlScriptBeforeChopNodeCallback(IN YYTK::CInstance* Self, IN YYTK::CInstance* Other, OUT YYTK::RValue& Result, IN int ArgumentCount, IN YYTK::RValue** Arguments)
		{
			return DispatchNodeInteraction(before_chop_node_callback, GML_SCRIPT_CHOP_NODE, Self, Other, Result, ArgumentCount, Arguments);
		}

		inline YYTK::RValue& GmlScriptModifyManaCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_mana_change_callback && Arguments && ArgumentCount >= 1 && Arguments[0] && MMAPI::Engine::IsNumeric(*Arguments[0]))
			{
				MMAPI::Player::BeforeManaChangeContext context{ Arguments[0]->ToDouble() };
				before_mana_change_callback(context);
				*Arguments[0] = context.m_amount;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(
					MMAPI::Internal::self_module,
					GML_SCRIPT_MODIFY_MANA
				)
			);

			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

		inline YYTK::RValue& GmlScriptBeforeFaceDirCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_face_dir_callback && Arguments && ArgumentCount >= 1 && Arguments[0] && MMAPI::Engine::IsNumeric(*Arguments[0]))
			{
				MMAPI::Player::FaceDirContext context{ Arguments[0]->ToDouble() };
				before_face_dir_callback(context);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_FACE_DIR)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterHeldItemCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_HELD_ITEM)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			int item_id = -1;
			if (Result.m_Kind != YYTK::VALUE_UNDEFINED
			    && MMAPI::Engine::StructVariableExists(Result, "item_id"))
			{
				item_id = static_cast<int>(Result.GetMember("item_id").ToInt64());

				// Persist the most recent non-undefined held item id so AfterUseActionComplete
				// can report what was being used even after the game has consumed it. Updating
				// only on defined results preserves the value across the consume-then-resolve gap.
				last_known_held_item_id = item_id;
			}

			if (after_held_item_callback)
			{
				MMAPI::Player::HeldItemContext context{ item_id };
				after_held_item_callback(context);
			}

			return Result;
		}

		inline void ResetUseActionEdge(YYTK::CInstance* /*Self*/, YYTK::CInstance* /*Other*/)
		{
			was_in_use_action = false;
			last_known_held_item_id = -1;
		}

		/// Internal Instance::OnObjectCall handler subscribed to obj_ari. Detects the rising edge
		/// of `state == HoldToUse && state.did_action` in Ari's FSM and fires the public
		/// after_use_action_complete_callback once per edge.
		///
		/// Edge tracking only advances on ticks where Ari is in `HoldToUse`. If the FSM briefly
		/// flickers out of `HoldToUse` mid-use (or the fsm/state members momentarily fail to
		/// resolve), the tracker freezes — preventing the post-flicker re-entry from being
		/// misread as a new rising edge while `did_action` is still latched true.
		inline void DetectUseActionComplete(YYTK::CInstance* Self)
		{
			if (!Self || !after_use_action_complete_callback)
				return;

			YYTK::RValue ari = Self->ToRValue();
			if (!MMAPI::Engine::StructVariableExists(ari, "fsm"))
				return;
			YYTK::RValue state = ari.GetMember("fsm").GetMember("state");
			if (!MMAPI::Engine::StructVariableExists(state, "state_id"))
				return;

			bool in_hold_to_use = state.GetMember("state_id").ToInt64()
				== static_cast<int64_t>(MMAPI::Player::States::HoldToUse);
			bool did_action_now = MMAPI::Engine::StructVariableExists(state, "did_action")
				&& state.GetMember("did_action").ToBoolean();
			bool in_use_action = in_hold_to_use && did_action_now;

			if (in_use_action && !was_in_use_action)
			{
				// Read from last_known_held_item_id rather than re-querying the held_item script.
				// At this moment, the game has likely already removed the item from inventory, so
				// calling held_item directly would return undefined. The held_item hook callback
				// keeps last_known_held_item_id pinned to the most recent valid value, which is
				// the item that was being used.
				MMAPI::Player::AfterUseActionContext context{ last_known_held_item_id, Self };
				after_use_action_complete_callback(context);
			}

			// Only update the edge tracker while in HoldToUse. Outside that state, freezing the
			// tracker preserves the last in-state observation across transients (FSM flicker, a
			// missing fsm/state member for a tick), so re-entering HoldToUse with did_action
			// still latched true is not misread as a fresh rising edge.
			if (in_hold_to_use)
				was_in_use_action = in_use_action;
		}

		inline YYTK::RValue& GmlScriptAfterShouldDieCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_SHOULD_DIE)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_should_die_callback)
			{
				MMAPI::Player::AfterShouldDieContext context{ Result.ToBoolean() };
				after_should_die_callback(context);
			}

			return Result;
		}
	}

	/// Returns the number of invulnerability hits remaining for Ari.
	/// Each hit absorbed while this value is greater than zero decrements it by one instead of dealing damage.
	/// @return The current value of MMAPI::Internal::global_instance.__ari.invulnerable_hits as an RValue.
	inline YYTK::RValue GetInvulnerabilityHits()
	{
		YYTK::RValue ari = MMAPI::Internal::global_instance->GetMember("__ari");
		return ari.GetMember("invulnerable_hits");
	}

	/// Sets the number of invulnerability hits for Ari.
	/// Each hit absorbed while this value is greater than zero decrements it by one instead of dealing damage.
	/// @param value The number of invulnerability hits to assign.
	inline void SetInvulnerabilityHits(int value)
	{
		YYTK::RValue ari = *MMAPI::Internal::global_instance->GetRefMember("__ari");
		*ari.GetRefMember("invulnerable_hits") = value;
	}

	/// Adjusts the number of invulnerability hits for Ari by the given signed amount.
	/// Each hit absorbed while this value is greater than zero decrements it by one instead of dealing damage.
	/// @param value The amount to add to Ari's current invulnerability hits. Negative values reduce the count.
	inline void ModifyInvulnerabilityHits(int value)
	{
		YYTK::RValue ari = *MMAPI::Internal::global_instance->GetRefMember("__ari");
		int current = static_cast<int>(ari.GetMember("invulnerable_hits").ToInt64());
		*ari.GetRefMember("invulnerable_hits") = current + value;
	}

	/// Returns Ari's currently held item.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @return The held item struct as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue GetHeldItem()
	{
		MMAPI_REQUIRE_ENABLED("Player", {});

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return {};

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_HELD_ITEM, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);
		return result;
	}

	/// Returns the item_id of Ari's currently held item.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @return The held item ID as an RValue, or undefined if no item or context is available.
	inline YYTK::RValue GetHeldItemId()
	{
		YYTK::RValue held_item = GetHeldItem();
		if (held_item.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		return held_item.GetMember("item_id");
	}

	/// Gets Ari's current player state.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @param state Ari's current player state.
	/// @return True if the player state was resolved, false if the required context is unavailable.
	inline bool TryGetState(MMAPI::Player::States& state)
	{
		MMAPI_REQUIRE_ENABLED("Player", false);

		YYTK::RValue state_id = Internal::GetStateId();
		if (state_id.m_Kind == YYTK::VALUE_UNDEFINED || state_id.m_Kind == YYTK::VALUE_UNSET)
			return false;

		int player_state_id = static_cast<int>(state_id.ToInt64());
		if (player_state_id < static_cast<int>(MMAPI::Player::States::Default) ||
		    player_state_id > static_cast<int>(MMAPI::Player::States::MountJump))
			return false;

		state = static_cast<MMAPI::Player::States>(player_state_id);
		return true;
	}

	/// Returns true if Ari is currently in the given player state.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @param state The player state to check.
	inline bool IsInState(MMAPI::Player::States state)
	{
		MMAPI::Player::States current_state;
		if (!TryGetState(current_state))
			return false;

		return current_state == state;
	}

	/// Gets Ari's current room position from the live obj_ari instance.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @return Ari's current x/y position, or std::nullopt if the required context is unavailable.
	inline std::optional<MMAPI::Player::Position> GetPosition()
	{
		MMAPI_REQUIRE_ENABLED("Player", std::nullopt);

		const auto& refs = MMAPI::Internal::instance_reference_map;
		if (!refs.contains(MMAPI::Instance::Internal::INSTANCE_OBJ_ARI))
			return std::nullopt;
		YYTK::CInstance* Ari = refs.at(MMAPI::Instance::Internal::INSTANCE_OBJ_ARI)[0];

		YYTK::RValue x;
		MMAPI::Internal::module_interface->GetBuiltin("x", Ari, NULL_INDEX, x);

		YYTK::RValue y;
		MMAPI::Internal::module_interface->GetBuiltin("y", Ari, NULL_INDEX, y);

		return MMAPI::Player::Position{ x.ToDouble(), y.ToDouble() };
	}

	/// Sets Ari's room position by writing directly to the live obj_ari instance's `x` and `y`
	/// built-in variables. Bypasses any game-side movement logic — use when teleporting Ari to
	/// an arbitrary position after a room transition (the room's spawn logic typically clobbers
	/// the position, so callers should sequence the write after the room has fully loaded).
	/// @attention Requires MMAPI::Player::Enable() to have been called and at least one obj_ari
	/// tick to have been observed (so MMAPI has latched the live instance).
	/// @param x The new X coordinate.
	/// @param y The new Y coordinate.
	inline void SetPosition(double x, double y)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Player");

		const auto& refs = MMAPI::Internal::instance_reference_map;
		auto it = refs.find(MMAPI::Instance::Internal::INSTANCE_OBJ_ARI);
		if (it == refs.end() || it->second.empty()) return;
		YYTK::CInstance* obj_ari = it->second[0];

		YYTK::RValue rx = x;
		YYTK::RValue ry = y;
		MMAPI::Internal::module_interface->SetBuiltin("x", obj_ari, NULL_INDEX, rx);
		MMAPI::Internal::module_interface->SetBuiltin("y", obj_ari, NULL_INDEX, ry);
	}

	/// Returns Ari's current health.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @return Ari's current health as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue GetHealth()
	{
		MMAPI_REQUIRE_ENABLED("Player", {});

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return {};

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_GET_HEALTH, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);
		return result;
	}

	/// Sets Ari's current health to the given value.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @param value The health value to set.
	inline void SetHealth(int value)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Player");

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_SET_HEALTH, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue health = value;
		YYTK::RValue result;
		YYTK::RValue* args[1] = { &health };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
	}

	/// Returns Ari's current maximum health.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @return Ari's current maximum health as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue GetMaxHealth()
	{
		MMAPI_REQUIRE_ENABLED("Player", {});

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return {};

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_GET_MAX_HEALTH, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);
		return result;
	}

	/// Sets Ari's maximum health by writing directly to __ari.base_health.
	/// If Ari's current health exceeds the new maximum, current health is clamped to it.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @param value The new maximum health value.
	inline void SetMaxHealth(int value)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Player");

		YYTK::RValue ari = *MMAPI::Internal::global_instance->GetRefMember("__ari");
		*ari.GetRefMember("base_health") = value;

		if (GetHealth().ToDouble() > static_cast<double>(value))
			SetHealth(value);
	}

	/// Adjusts Ari's current health by the given signed amount.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @param value The amount to add to Ari's current health. Negative values reduce health.
	inline void ModifyHealth(int value)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Player");

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_MODIFY_HEALTH, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue modifier = value;
		YYTK::RValue result;
		YYTK::RValue* args[1] = { &modifier };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
	}

	/// Adjusts Ari's maximum health by the given signed amount, writing directly to __ari.base_health.
	/// Does not clamp current health — call SetMaxHealth if clamping is required.
	/// @param value The amount to add to Ari's maximum health. Negative values reduce the maximum.
	/// @return The resulting maximum health value as an RValue.
	inline YYTK::RValue ModifyMaxHealth(int value)
	{
		YYTK::RValue ari = *MMAPI::Internal::global_instance->GetRefMember("__ari");
		int max_health = static_cast<int>(ari.GetMember("base_health").ToInt64()) + value;
		*ari.GetRefMember("base_health") = max_health;
		return max_health;
	}

	/// Adjusts Ari's current stamina by the given signed amount.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @param value The amount to add to Ari's current stamina. Negative values reduce stamina.
	inline void ModifyStamina(int value)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Player");

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_MODIFY_STAMINA, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue modifier = value;
		YYTK::RValue result;
		YYTK::RValue* args[1] = { &modifier };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
	}

	/// Returns Ari's current stamina.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @return Ari's current stamina as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue GetStamina()
	{
		MMAPI_REQUIRE_ENABLED("Player", {});

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return {};

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_GET_STAMINA, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);
		return result;
	}

	/// Adjusts Ari's current gold by the given signed amount.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @param value The amount to add to Ari's current gold. Negative values reduce gold.
	inline void ModifyGold(int value)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Player");

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_MODIFY_GOLD, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue modifier = value;
		YYTK::RValue result;
		YYTK::RValue* args[1] = { &modifier };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
	}

	/// Adjusts Ari's current renown by the given signed amount.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @param value The amount to add to Ari's current renown. Negative values reduce renown.
	inline void ModifyRenown(int value)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Player");

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_MODIFY_RENOWN, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue modifier = value;
		YYTK::RValue result;
		YYTK::RValue* args[1] = { &modifier };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
	}

	/// Returns Ari's current mana.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @return Ari's current mana as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue GetMana()
	{
		MMAPI_REQUIRE_ENABLED("Player", {});

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return {};

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_GET_MANA, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);
		return result;
	}

	/// Adjusts Ari's current mana by the given signed amount.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @param value The amount to add to Ari's current mana. Negative values reduce mana.
	inline void ModifyMana(int value)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Player");

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_MODIFY_MANA, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue modifier = value;
		YYTK::RValue result;
		YYTK::RValue* args[1] = { &modifier };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
	}

	/// Returns the spell Ari currently has pinned to her quick-cast slot, read directly off
	/// `globalInstance.__ari.pinned_spell`. Mirrors how `SetMaxHealth` reads `base_health` — no
	/// game script call, no calling context required.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @return The pinned spell, or std::nullopt if global_instance isn't set, the field is missing
	///         or non-numeric, or the value is outside the documented `Spell::Ids` range.
	inline std::optional<MMAPI::Spell::Ids> GetPinnedSpell()
	{
		MMAPI_REQUIRE_ENABLED("Player", std::nullopt);

		if (!MMAPI::Internal::global_instance)
			return std::nullopt;

		YYTK::RValue ari = *MMAPI::Internal::global_instance->GetRefMember("__ari");
		if (!MMAPI::Engine::StructVariableExists(ari, "pinned_spell"))
			return std::nullopt;

		YYTK::RValue pinned = ari.GetMember("pinned_spell");
		if (!MMAPI::Engine::IsNumeric(pinned))
			return std::nullopt;

		int id = static_cast<int>(pinned.ToInt64());
		if (id < 0 || id >= MMAPI::Spell::IdCount)
			return std::nullopt;

		return static_cast<MMAPI::Spell::Ids>(id);
	}

	/// Forces Ari to face the given direction (in GameMaker degrees: 0°=right, 90°=up, counterclockwise).
	/// Invokes `face_dir@obj_ari` with the live obj_ari instance as Self — the script is defined inside
	/// obj_ari's Create event, so it expects obj_ari as the calling context (not the `__ari` struct
	/// that most other Ari scripts use).
	/// @attention Requires MMAPI::Player::Enable() to have been called and at least one obj_ari tick
	/// to have been observed (so MMAPI has latched the live instance).
	/// @param degrees The direction Ari should face, in GameMaker degrees.
	inline void FaceDir(double degrees)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Player");

		const auto& refs = MMAPI::Internal::instance_reference_map;
		auto it = refs.find(MMAPI::Instance::Internal::INSTANCE_OBJ_ARI);
		if (it == refs.end() || it->second.empty()) return;
		YYTK::CInstance* obj_ari = it->second[0];

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_FACE_DIR, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue arg = degrees;
		YYTK::RValue result;
		YYTK::RValue* args[1] = { &arg };
		gml_script->m_Functions->m_ScriptFunction(obj_ari, obj_ari, result, 1, args);
	}

	/// Sets Ari's cardinal direction (0=East, 1=North, 2=West, 3=South). The cardinal is the
	/// discrete-direction field used by the game for attack/movement direction logic, distinct from
	/// the visual angle controlled by `FaceDir`. Most callers want to set both in sync — see
	/// `FaceCardinal` for the combined helper.
	/// @attention Requires MMAPI::Player::Enable() to have been called and at least one obj_ari tick
	/// to have been observed (so MMAPI has latched the live instance).
	/// @param cardinal The cardinal direction to set.
	inline void SetCardinal(Cardinal cardinal)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Player");

		const auto& refs = MMAPI::Internal::instance_reference_map;
		auto it = refs.find(MMAPI::Instance::Internal::INSTANCE_OBJ_ARI);
		if (it == refs.end() || it->second.empty()) return;
		YYTK::CInstance* obj_ari = it->second[0];

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_SET_CARDINAL, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue arg = static_cast<int>(cardinal);
		YYTK::RValue result;
		YYTK::RValue* args[1] = { &arg };
		gml_script->m_Functions->m_ScriptFunction(obj_ari, obj_ari, result, 1, args);
	}

	/// Sets both Ari's cardinal direction and her visual angle to match. Convenience for the common
	/// "snap Ari to face this direction" pattern — equivalent to `SetCardinal(c); FaceDir(CardinalToDegrees(c));`.
	/// @attention Requires MMAPI::Player::Enable() to have been called and at least one obj_ari tick
	/// to have been observed.
	/// @param cardinal The cardinal direction Ari should face.
	inline void FaceCardinal(Cardinal cardinal)
	{
		SetCardinal(cardinal);
		FaceDir(CardinalToDegrees(cardinal));
	}

	/// Returns Ari's current essence.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @return Ari's current essence as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue GetEssence()
	{
		MMAPI_REQUIRE_ENABLED("Player", {});

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return {};

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_GET_ESSENCE, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);
		return result;
	}

	/// Adjusts Ari's current essence by the given signed amount.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @param value The amount to add to Ari's current essence. Negative values reduce essence.
	inline void ModifyEssence(int value)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Player");

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_MODIFY_ESSENCE, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue modifier = value;
		YYTK::RValue result;
		YYTK::RValue* args[1] = { &modifier };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
	}

	/// Returns Ari's current movement speed.
	/// @attention Requires MMAPI::Player::Enable() to have been called.
	/// @return Ari's current movement speed as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue GetMoveSpeed()
	{
		MMAPI_REQUIRE_ENABLED("Player", {});

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return {};

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_GET_MOVE_SPEED, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);
		return result;
	}

	/// Activates Player utility functions that directly call game scripts. Eagerly installs every Player
	/// script hook used by Hooks::* registrars (get_move_speed, modify_health/stamina/mana, face_dir, held_item).
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Player::Enable() called");

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Player, MMAPI::Bark);

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Player, MMAPI::Instance);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_GET_MOVE_SPEED,      reinterpret_cast<PVOID>(Internal::GmlScriptGetMoveSpeedCallback) },
			{ Internal::GML_SCRIPT_MODIFY_HEALTH,       reinterpret_cast<PVOID>(Internal::GmlScriptModifyHealthCallback) },
			{ Internal::GML_SCRIPT_MODIFY_STAMINA,      reinterpret_cast<PVOID>(Internal::GmlScriptModifyStaminaCallback) },
			{ Internal::GML_SCRIPT_MODIFY_MANA,         reinterpret_cast<PVOID>(Internal::GmlScriptModifyManaCallback) },
			{ Internal::GML_SCRIPT_FACE_DIR,            reinterpret_cast<PVOID>(Internal::GmlScriptBeforeFaceDirCallback) },
			{ Internal::GML_SCRIPT_HELD_ITEM,           reinterpret_cast<PVOID>(Internal::GmlScriptAfterHeldItemCallback) },
			{ Internal::GML_SCRIPT_SHOULD_DIE,          reinterpret_cast<PVOID>(Internal::GmlScriptAfterShouldDieCallback) },
			{ Internal::GML_SCRIPT_TOOL_HOE,            reinterpret_cast<PVOID>(Internal::GmlScriptHoeToolActionCallback) },
			{ Internal::GML_SCRIPT_TOOL_AXE,            reinterpret_cast<PVOID>(Internal::GmlScriptAxeToolActionCallback) },
			{ Internal::GML_SCRIPT_TOOL_PICK_AXE,       reinterpret_cast<PVOID>(Internal::GmlScriptPickAxeToolActionCallback) },
			{ Internal::GML_SCRIPT_TOOL_SHOVEL,         reinterpret_cast<PVOID>(Internal::GmlScriptShovelToolActionCallback) },
			{ Internal::GML_SCRIPT_TOOL_NET,            reinterpret_cast<PVOID>(Internal::GmlScriptNetToolActionCallback) },
			{ Internal::GML_SCRIPT_TOOL_WATERING_CAN,   reinterpret_cast<PVOID>(Internal::GmlScriptWateringCanToolActionCallback) },
			{ Internal::GML_SCRIPT_TOOL_SOW,            reinterpret_cast<PVOID>(Internal::GmlScriptSowToolActionCallback) },
			{ Internal::GML_SCRIPT_PICK_NODE,           reinterpret_cast<PVOID>(Internal::GmlScriptBeforePickNodeCallback) },
			{ Internal::GML_SCRIPT_CHOP_NODE,           reinterpret_cast<PVOID>(Internal::GmlScriptBeforeChopNodeCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		// Subscribe an internal obj_ari tick handler that edge-detects use-action completion
		// for Hooks::AfterUseActionComplete. Plus a setup_main_screen handler to reset the edge
		// state on return to title.
		MMAPI::Instance::Internal::RegisterInternalOnObjectCall(
			MMAPI::Instance::Internal::INSTANCE_OBJ_ARI,
			Internal::DetectUseActionComplete
		);
		MMAPI::Internal::RegisterOnSetupMainScreenHandler(Internal::ResetUseActionEdge);

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	namespace Hooks
	{
		/// Registers a callback that can modify Ari's movement speed after the game calculates it.
		/// Use ctx.AddModifier(value) to add a signed offset, or ctx.SetOverride(value) to force a specific speed.
		/// If multiple callbacks call SetOverride, the last registered callback wins.
		/// @param callback A function called with a mutable move speed context after the game calculates it.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterMoveSpeed(Internal::AfterMoveSpeedCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Player::Hooks::AfterMoveSpeed, MMAPI::Player);

			return MMAPI::Internal::RegisterHook(
				"Player::AfterMoveSpeed",
				Internal::after_move_speed_callback,
				callback
			);
		}

		/// Registers a callback that can modify the amount passed to the game's modify_health script.
		/// Use ctx.SetAmount(value) to change the health delta before the game applies it.
		/// @param callback A function called with a mutable health change context before the game processes it.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeHealthChange(Internal::BeforeHealthChangeCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Player::Hooks::BeforeHealthChange, MMAPI::Player);

			return MMAPI::Internal::RegisterHook(
				"Player::BeforeHealthChange",
				Internal::before_health_change_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's modify_health script. Read `ctx.GetAmount()`
		/// to inspect the final delta the game applied (post any BeforeHealthChange mutations) — pair
		/// with set-bonus / threshold logic that should react only once the game has processed the change.
		/// @param callback A function called with a `MMAPI::Player::AfterHealthChangeContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterHealthChange(Internal::AfterHealthChangeCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Player::Hooks::AfterHealthChange, MMAPI::Player);

			return MMAPI::Internal::RegisterHook(
				"Player::AfterHealthChange",
				Internal::after_health_change_callback,
				callback
			);
		}

		/// Registers a callback that can modify the amount passed to the game's modify_stamina script.
		/// Use ctx.SetAmount(value) to change the stamina delta before the game applies it.
		/// @param callback A function called with a mutable stamina change context before the game processes it.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeStaminaChange(Internal::BeforeStaminaChangeCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Player::Hooks::BeforeStaminaChange, MMAPI::Player);

			return MMAPI::Internal::RegisterHook(
				"Player::BeforeStaminaChange",
				Internal::before_stamina_change_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's modify_stamina script. Read `ctx.GetAmount()`
		/// for the amount actually applied (post any BeforeStaminaChange mutations) — useful as a
		/// "stamina was really consumed this fire" signal for mods that bookkeep per-action costs.
		/// @param callback A function called with a `MMAPI::Player::AfterStaminaChangeContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterStaminaChange(Internal::AfterStaminaChangeCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Player::Hooks::AfterStaminaChange, MMAPI::Player);

			return MMAPI::Internal::RegisterHook(
				"Player::AfterStaminaChange",
				Internal::after_stamina_change_callback,
				callback
			);
		}

		/// Registers a callback that runs before each AriFsm tool-action closure. Fires for every
		/// `Tool` (Hoe, Axe, PickAxe, Shovel, Net, WateringCan, Sow) — branch on `ctx.GetTool()` to
		/// route per-tool logic. Use this for pre-action state setup; pair with `AfterToolAction`
		/// for post-action observation (e.g. detecting whether stamina was consumed during the action).
		/// @param callback A function called with a `MMAPI::Player::ToolActionContext` before each tool action.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeToolAction(Internal::BeforeToolActionCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Player::Hooks::BeforeToolAction, MMAPI::Player);

			return MMAPI::Internal::RegisterHook(
				"Player::BeforeToolAction",
				Internal::before_tool_action_callback,
				callback
			);
		}

		/// Registers a callback that runs after each AriFsm tool-action closure. Fires for every
		/// `Tool` (Hoe, Axe, PickAxe, Shovel, Net, WateringCan, Sow) — branch on `ctx.GetTool()` to
		/// route per-tool logic. Pair with `Player::Hooks::AfterStaminaChange` if you need to know
		/// whether stamina was consumed during the action.
		/// @param callback A function called with a `MMAPI::Player::ToolActionContext` after each tool action.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterToolAction(Internal::AfterToolActionCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Player::Hooks::AfterToolAction, MMAPI::Player);

			return MMAPI::Internal::RegisterHook(
				"Player::AfterToolAction",
				Internal::after_tool_action_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game's `pick_node` script — the resource-node
		/// interaction routine for pickable nodes (ores, rocks, etc.). Use `ctx.GetDamageModifier()` /
		/// `ctx.SetDamageModifier(value)` to inspect or mutate the damage modifier (Arguments[4]). The
		/// game applies negative modifiers as the "charged tool damage penalty" — set to 0 to clamp.
		/// @param callback A function called with a mutable `MMAPI::Player::NodeInteractionContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforePickNode(Internal::BeforeNodeInteractionCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Player::Hooks::BeforePickNode, MMAPI::Player);

			return MMAPI::Internal::RegisterHook(
				"Player::BeforePickNode",
				Internal::before_pick_node_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game's `chop_node` script — the resource-node
		/// interaction routine for choppable nodes (trees). Uses the same `NodeInteractionContext`
		/// shape as `BeforePickNode`; Arguments[4] is the damage modifier.
		/// @param callback A function called with a mutable `MMAPI::Player::NodeInteractionContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeChopNode(Internal::BeforeNodeInteractionCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Player::Hooks::BeforeChopNode, MMAPI::Player);

			return MMAPI::Internal::RegisterHook(
				"Player::BeforeChopNode",
				Internal::before_chop_node_callback,
				callback
			);
		}

		/// Registers a callback that can modify the amount passed to the game's modify_mana script.
		/// Use ctx.SetAmount(value) to change the mana delta before the game applies it.
		/// @param callback A function called with a mutable mana change context before the game processes it.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeManaChange(Internal::BeforeManaChangeCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Player::Hooks::BeforeManaChange, MMAPI::Player);

			return MMAPI::Internal::RegisterHook(
				"Player::BeforeManaChange",
				Internal::before_mana_change_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game's `face_dir@obj_ari` script — fired whenever
		/// the game updates Ari's facing direction. Read `ctx.GetDirectionDegrees()` to inspect the
		/// direction the game is about to apply (raw GameMaker degrees).
		/// @param callback A function called with a `MMAPI::Player::FaceDirContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeFaceDir(Internal::BeforeFaceDirCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Player::Hooks::BeforeFaceDir, MMAPI::Player);

			return MMAPI::Internal::RegisterHook(
				"Player::BeforeFaceDir",
				Internal::before_face_dir_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `held_item@Ari@Ari` script. The wrapper
		/// resolves the held item's `item_id` from Result (guarded for undefined and missing `item_id`).
		/// Read `ctx.GetItemId()` to react to changes in what Ari is holding — push-style counterpart
		/// to the pull-style [`Player::GetHeldItem`](API-MMAPI-Player-GetHeldItem.md).
		/// @param callback A function called with a `MMAPI::Player::HeldItemContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterHeldItem(Internal::AfterHeldItemCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Player::Hooks::AfterHeldItem, MMAPI::Player);

			return MMAPI::Internal::RegisterHook(
				"Player::AfterHeldItem",
				Internal::after_held_item_callback,
				callback
			);
		}

		/// Registers a callback that fires once when Ari's use-item action completes — the rising
		/// edge of `state == HoldToUse && state.did_action == true` in Ari's FSM. This is the right
		/// signal for "the item-use animation has played to its consume point" (eating, drinking,
		/// throwing, etc.). `Item::Hooks::BeforeUseItem` fires at the *start* of the use; this
		/// fires at the *end*.
		///
		/// Edge-detected: fires at most once per use action, not every frame did_action stays true.
		/// Edge state is reset on return to title.
		///
		/// Read `ctx.GetItemId()` for the item Ari was using (sourced from the most recent
		/// non-undefined held_item observation — survives the consume-then-resolve gap), and
		/// `ctx.GetSelf()` for the obj_ari instance.
		///
		/// @param callback A function called with a `MMAPI::Player::AfterUseActionContext` on each rising edge.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterUseActionComplete(Internal::AfterUseActionCompleteCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Player::Hooks::AfterUseActionComplete, MMAPI::Player);

			return MMAPI::Internal::RegisterHook(
				"Player::AfterUseActionComplete",
				Internal::after_use_action_complete_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `should_die@gml_Object_obj_ari_Create_0`
		/// script — the check the game runs when something would otherwise kill Ari. Read
		/// `ctx.GetWillDie()` to see the game's verdict: `true` means Ari is about to die from
		/// this event, `false` means the death was prevented (e.g. by an invulnerability shield).
		/// Useful as an "Ari died" signal for mods that need to reset state on death.
		/// @param callback A function called with a `MMAPI::Player::AfterShouldDieContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterShouldDie(Internal::AfterShouldDieCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Player::Hooks::AfterShouldDie, MMAPI::Player);

			return MMAPI::Internal::RegisterHook(
				"Player::AfterShouldDie",
				Internal::after_should_die_callback,
				callback
			);
		}
	}

}
