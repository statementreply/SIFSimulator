#pragma once

#include <string>
#include <stdexcept>
#include <utility>


class JsonParseError : public std::runtime_error {
public:
	explicit JsonParseError(const std::string& what_arg) : std::runtime_error(what_arg) {}
	explicit JsonParseError(const char* what_arg) : std::runtime_error(what_arg) {}
};


template <class RapidJsonValue>
const auto & GetJsonMemberObject(const RapidJsonValue & obj, unsigned index) {
	if (!obj.IsArray()) {
		throw JsonParseError(std::string("JSON: Invalid type"));
	}
	if (index >= obj.Size()) {
		throw JsonParseError(std::string("JSON: Index out of range"));
	}
	const auto & member = obj[index];
	if (!member.IsObject()) {
		throw JsonParseError(std::string("JSON: Invalid type"));
	}
	return member;
}


template <class RapidJsonValue>
const auto & GetJsonMemberObject(const RapidJsonValue & obj, const char * name) {
	if (!obj.IsObject()) {
		throw JsonParseError(std::string("JSON: Invalid type"));
	}
	if (!obj.HasMember(name)) {
		throw JsonParseError(std::string("JSON: Member not found"));
	}
	const auto & member = obj[name];
	if (!member.IsObject()) {
		throw JsonParseError(std::string("JSON: Invalid type"));
	}
	return member;
}


template <class RapidJsonValue>
int GetJsonMemberInt(RapidJsonValue && obj, const char * name) {
	if (!obj.IsObject()) {
		throw JsonParseError(std::string("JSON: Invalid type"));
	}
	if (!obj.HasMember(name)) {
		throw JsonParseError(std::string("JSON: Member not found"));
	}
	auto && member = obj[name];
	if (!member.IsInt()) {
		throw JsonParseError(std::string("JSON: Invalid type"));
	}
	return member.GetInt();
}


template <class RapidJsonValue>
double GetJsonMemberDouble(const RapidJsonValue & obj, const char * name) {
	if (!obj.IsObject()) {
		throw JsonParseError(std::string("JSON: Invalid type"));
	}
	if (!obj.HasMember(name)) {
		throw JsonParseError(std::string("JSON: Member not found"));
	}
	auto && member = obj[name];
	if (!member.IsNumber()) {
		throw JsonParseError(std::string("JSON: Invalid type"));
	}
	return member.GetDouble();
}
