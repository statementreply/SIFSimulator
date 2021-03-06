#include "rapidjsonutil.h"
#include "rapidjson/filereadstream.h"
#include <string>

using namespace std::literals;


rapidjson::Document ParseJsonFile(std::FILE * fp) {
	constexpr size_t BUF_SIZE = 0x1000;
	char buf[BUF_SIZE];
	rapidjson::FileReadStream is(fp, buf, sizeof(buf));
	rapidjson::Document doc;
	if (doc.ParseStream(is).HasParseError()) {
		throw JsonParseError("Invalid JSON format");
	}
	return doc;
}


const rapidjson::Value & GetJsonItem(const rapidjson::Value & obj, rapidjson::SizeType index) {
	if (index >= obj.Size()) {
		throw JsonParseError("JSON: Index out of range");
	}
	return obj[index];
}

const rapidjson::Value & GetJsonMember(const rapidjson::Value & obj, const char * name) {
	auto member = obj.FindMember(name);
	if (member == obj.MemberEnd()) {
		throw JsonParseError("JSON: Member not found: "s + name);
	}
	return member->value;
}


const rapidjson::Value & GetJsonItemObject(const rapidjson::Value & obj, rapidjson::SizeType index) {
	const auto & member = GetJsonItem(obj, index);
	if (!member.IsObject()) {
		throw JsonParseError("JSON: Invalid type");
	}
	return member;
}

const rapidjson::Value & GetJsonMemberObject(const rapidjson::Value & obj, const char * name) {
	const auto & member = GetJsonMember(obj, name);
	if (!member.IsObject()) {
		throw JsonParseError("JSON: Invalid type: "s + name);
	}
	return member;
}


const rapidjson::Value & GetJsonMemberArray(const rapidjson::Value & obj, const char * name) {
	const auto & member = GetJsonMember(obj, name);
	if (!member.IsArray()) {
		throw JsonParseError("JSON: Invalid type: "s + name);
	}
	return member;
}


int GetJsonMemberInt(const rapidjson::Value & obj, const char * name) {
	const auto & member = GetJsonMember(obj, name);
	if (!member.IsInt()) {
		throw JsonParseError("JSON: Invalid type: "s + name);
	}
	return member.GetInt();
}


double GetJsonItemDouble(const rapidjson::Value & obj, rapidjson::SizeType index) {
	const auto & member = GetJsonItem(obj, index);
	if (!member.IsNumber()) {
		throw JsonParseError("JSON: Invalid type");
	}
	return member.GetDouble();
}

double GetJsonMemberDouble(const rapidjson::Value & obj, const char * name) {
	const auto & member = GetJsonMember(obj, name);
	if (!member.IsNumber()) {
		throw JsonParseError("JSON: Invalid type: "s + name);
	}
	return member.GetDouble();
}

optional<double> TryGetJsonMemberDouble(const rapidjson::Value & obj, const char * name) {
	auto member = obj.FindMember(name);
	if (member == obj.MemberEnd() || member->value.IsNull()) {
		return nullopt;
	}
	if (!member->value.IsNumber()) {
		throw JsonParseError("JSON: Invalid type: "s + name);
	}
	return member->value.GetDouble();
}


optional<const char *> TryGetJsonMemberString(const rapidjson::Value & obj, const char * name) {
	auto member = obj.FindMember(name);
	if (member == obj.MemberEnd() || member->value.IsNull()) {
		return nullopt;
	}
	if (!member->value.IsString()) {
		throw JsonParseError("JSON: Invalid type: "s + name);
	}
	return member->value.GetString();
}
