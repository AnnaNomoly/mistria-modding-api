// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Object.hpp"

#include <string>
#include <vector>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Room
{
	/// Iterates over every active instance in the current room.
	/// @param callback A callable that accepts YYTK::CInstance*.
	/// @return True if the current room was available and iteration ran.
	template <typename Callback>
	inline bool ForEachActiveInstance(Callback&& callback)
	{
		YYTK::CRoom* current_room = nullptr;
		if (!Aurie::AurieSuccess(MMAPI::Internal::module_interface->GetCurrentRoomData(current_room)))
			return false;

		for (YYTK::CInstance* instance = current_room->GetMembers().m_ActiveInstances.m_First;
		     instance != nullptr;
		     instance = instance->GetMembers().m_Flink)
		{
			callback(instance);
		}

		return true;
	}

	/// Iterates over each active instance in the current room that has an object node.
	/// @param callback A callable that accepts YYTK::CInstance* and YYTK::RValue node.
	/// @return True if the current room was available and iteration ran.
	template <typename Callback>
	inline bool ForEachObjectNode(Callback&& callback)
	{
		return ForEachActiveInstance([&](YYTK::CInstance* instance) {
			YYTK::RValue node = MMAPI::Object::GetNode(instance);
			if (node.m_Kind == YYTK::VALUE_UNDEFINED)
				return;

			callback(instance, node);
		});
	}

	/// Iterates over each active object node that matches a predicate.
	/// @param predicate A callable that accepts YYTK::CInstance* and YYTK::RValue node, returning bool.
	/// @param callback A callable that accepts YYTK::CInstance* and YYTK::RValue node.
	/// @return True if the current room was available and iteration ran.
	template <typename Predicate, typename Callback>
	inline bool ForEachObjectNodeWhere(Predicate&& predicate, Callback&& callback)
	{
		return ForEachObjectNode([&](YYTK::CInstance* instance, YYTK::RValue node) {
			if (predicate(instance, node))
				callback(instance, node);
		});
	}

	/// Finds the top-left tile positions of active room objects with the given internal object name.
	/// @param internal_name The exact internal object name to find.
	/// @return A vector of matching top-left tile positions.
	inline std::vector<MMAPI::Object::Position> FindObjectPositions(const std::string& internal_name)
	{
		std::vector<MMAPI::Object::Position> positions;
		YYTK::RValue target_id = MMAPI::Object::GetIdFromInternalName(internal_name);
		if (target_id.m_Kind == YYTK::VALUE_UNDEFINED)
			return positions;

		ForEachObjectNode([&](YYTK::CInstance* instance, YYTK::RValue node) {
			YYTK::RValue object_id = MMAPI::Object::GetObjectId(node);
			if (object_id.m_Kind == YYTK::VALUE_UNDEFINED || object_id.ToInt64() != target_id.ToInt64())
				return;

			std::optional<MMAPI::Object::Position> position = MMAPI::Object::GetTopLeftPosition(node);
			if (position.has_value())
				positions.push_back(position.value());
		});

		return positions;
	}

	/// Returns true if the current room contains an active object with the given internal object name.
	/// @param internal_name The exact internal object name to find.
	inline bool ContainsObject(const std::string& internal_name)
	{
		bool found = false;
		YYTK::RValue target_id = MMAPI::Object::GetIdFromInternalName(internal_name);
		if (target_id.m_Kind == YYTK::VALUE_UNDEFINED)
			return false;

		ForEachObjectNode([&](YYTK::CInstance* instance, YYTK::RValue node) {
			if (found)
				return;

			YYTK::RValue object_id = MMAPI::Object::GetObjectId(node);
			found = object_id.m_Kind != YYTK::VALUE_UNDEFINED && object_id.ToInt64() == target_id.ToInt64();
		});

		return found;
	}
}
