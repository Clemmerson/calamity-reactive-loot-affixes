#pragma once

#include <istream>
#include <string>

#include <nlohmann/json.hpp>

namespace CalamityAffixes::RuntimeContractDocumentPolicy
{
	[[nodiscard]] inline bool Parse(std::istream& a_input, nlohmann::json& a_document, std::string& a_error)
	{
		a_document = nlohmann::json::object();
		a_error.clear();
		if (!a_input.good()) {
			a_error = "input stream is not readable";
			return false;
		}
		try {
			a_input >> a_document;
			return true;
		} catch (const std::exception& e) {
			a_error = e.what();
			return false;
		}
	}
}
