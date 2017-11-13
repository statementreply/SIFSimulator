#pragma once

#include <string>
#include <stdexcept>
#include <utility>


template <class RapidJsonValue>
const auto & GetJsonMemberObject(const RapidJsonValue & obj, unsigned index) {
	if (!obj.IsArray()) {
		throw std::runtime_error(std::string("JSON: Invalid type"));
	}
	if (index >= obj.Size()) {
		throw std::runtime_error(std::string("JSON: Index out of range"));
	}
	const auto & member = obj[index];
	if (!member.IsObject()) {
		throw std::runtime_error(std::string("JSON: Invalid type"));
	}
	return member;
}


template <class RapidJsonValue>
const auto & GetJsonMemberObject(const RapidJsonValue & obj, const char * name) {
	if (!obj.IsObject()) {
		throw std::runtime_error(std::string("JSON: Invalid type"));
	}
	if (!obj.HasMember(name)) {
		throw std::runtime_error(std::string("JSON: Member not found"));
	}
	const auto & member = obj[name];
	if (!member.IsObject()) {
		throw std::runtime_error(std::string("JSON: Invalid type"));
	}
	return member;
}


template <class RapidJsonValue>
int GetJsonMemberInt(RapidJsonValue && obj, const char * name) {
	if (!obj.IsObject()) {
		throw std::runtime_error(std::string("JSON: Invalid type"));
	}
	if (!obj.HasMember(name)) {
		throw std::runtime_error(std::string("JSON: Member not found"));
	}
	auto && member = obj[name];
	if (!member.IsInt()) {
		throw std::runtime_error(std::string("JSON: Invalid type"));
	}
	return member.GetInt();
}


template <class RapidJsonValue>
double GetJsonMemberDouble(const RapidJsonValue & obj, const char * name) {
	if (!obj.IsObject()) {
		throw std::runtime_error(std::string("JSON: Invalid type"));
	}
	if (!obj.HasMember(name)) {
		throw std::runtime_error(std::string("JSON: Member not found"));
	}
	auto && member = obj[name];
	if (!member.IsNumber()) {
		throw std::runtime_error(std::string("JSON: Invalid type"));
	}
	return member.GetDouble();
}
