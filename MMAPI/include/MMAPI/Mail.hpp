// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Engine.hpp"

#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Mail
{
	namespace Internal
	{
		inline YYTK::RValue GetInboxContents()
		{
			YYTK::RValue ari = MMAPI::Internal::global_instance->GetMember("__ari");
			YYTK::RValue inbox = ari.GetMember("inbox");
			return inbox.GetMember("contents");
		}
	}

	/// Adds mail to Ari's inbox.
	/// @param mail_name The internal mail name to add.
	inline void SendMail(std::string mail_name)
	{
		YYTK::RValue contents = Internal::GetInboxContents();
		YYTK::RValue contents_buffer = contents.GetMember("__buffer");

		YYTK::RValue items_taken = false;
		YYTK::RValue name = mail_name.c_str();
		YYTK::RValue read = false;
		YYTK::RValue mail;

		MMAPI::Internal::module_interface->GetRunnerInterface().StructCreate(&mail);
		MMAPI::Internal::module_interface->GetRunnerInterface().StructAddRValue(&mail, "items_taken", &items_taken);
		MMAPI::Internal::module_interface->GetRunnerInterface().StructAddRValue(&mail, "name", &name);
		MMAPI::Internal::module_interface->GetRunnerInterface().StructAddRValue(&mail, "read", &read);

		MMAPI::Internal::module_interface->CallBuiltin("array_push", { contents_buffer, mail });

		double new_size = contents.GetMember("__count").ToDouble() + 1.0;
		MMAPI::Engine::StructVariableSet(contents, "__count", new_size);
		MMAPI::Engine::StructVariableSet(contents, "__internal_size", new_size);
	}

	/// Returns true if Ari's inbox contains the given internal mail name.
	/// @param mail_name The internal mail name to check.
	inline bool Exists(std::string mail_name)
	{
		YYTK::RValue contents = Internal::GetInboxContents();
		YYTK::RValue contents_buffer = contents.GetMember("__buffer");

		size_t mail_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(contents_buffer, mail_count);

		for (size_t i = 0; i < mail_count; i++)
		{
			YYTK::RValue* entry = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(contents_buffer, i, entry);

			if (entry->GetMember("name").ToString() == mail_name)
				return true;
		}

		return false;
	}
}
