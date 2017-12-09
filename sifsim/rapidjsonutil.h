#pragma once

#include "rapidjson/rapidjson.h"
#include <string>
#include <stdexcept>
#include <utility>
#include <optional>


class JsonParseError : public std::runtime_error {
public:
	explicit JsonParseError(const std::string& what_arg) : std::runtime_error(what_arg) {}
	explicit JsonParseError(const char* what_arg) : std::runtime_error(what_arg) {}
};


const rapidjson::Value & GetJsonItem(const rapidjson::Value & obj, rapidjson::SizeType index) {
	if (index >= obj.Size()) {
		throw JsonParseError(std::string("JSON: Index out of range"));
	}
	return obj[index];
}

const rapidjson::Value & GetJsonMember(const rapidjson::Value & obj, const char * name) {
	auto member = obj.FindMember(name);
	if (member == obj.MemberEnd()) {
		throw JsonParseError(std::string("JSON: Member not found"));
	}
	return member->value;
}


const rapidjson::Value & GetJsonItemObject(const rapidjson::Value & obj, rapidjson::SizeType index) {
	const auto & member = GetJsonItem(obj, index);
	if (!member.IsObject()) {
		throw JsonParseError(std::string("JSON: Invalid type"));
	}
	return member;
}

const rapidjson::Value & GetJsonMemberObject(const rapidjson::Value & obj, const char * name) {
	const auto & member = GetJsonMember(obj, name);
	if (!member.IsObject()) {
		throw JsonParseError(std::string("JSON: Invalid type"));
	}
	return member;
}


int GetJsonMemberInt(const rapidjson::Value & obj, const char * name) {
	const auto & member = GetJsonMember(obj, name);
	if (!member.IsInt()) {
		throw JsonParseError(std::string("JSON: Invalid type"));
	}
	return member.GetInt();
}


double GetJsonMemberDouble(const rapidjson::Value & obj, const char * name) {
	const auto & member = GetJsonMember(obj, name);
	if (!member.IsNumber()) {
		throw JsonParseError(std::string("JSON: Invalid type"));
	}
	return member.GetDouble();
}


std::optional<double> TryGetJsonMemberDouble(const rapidjson::Value & obj, const char * name) {
	auto member = obj.FindMember(name);
	if (member == obj.MemberEnd() || member->value.IsNull()) {
		return std::nullopt;
	}
	if (!member->value.IsNumber()) {
		throw JsonParseError(std::string("JSON: Invalid type"));
	}
	return member->value.GetDouble();
}
