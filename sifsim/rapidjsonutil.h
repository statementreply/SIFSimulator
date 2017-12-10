#pragma once

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include <string>
#include <stdexcept>
#include <utility>
#include "optional.h"
#include <cstdio>


class JsonParseError : public std::runtime_error {
public:
	explicit JsonParseError(const std::string& what_arg) : std::runtime_error(what_arg) {}
	explicit JsonParseError(const char* what_arg) : std::runtime_error(what_arg) {}
};

rapidjson::Document ParseJsonFile(std::FILE * fp);

const rapidjson::Value & GetJsonItem(const rapidjson::Value & obj, rapidjson::SizeType index);
const rapidjson::Value & GetJsonMember(const rapidjson::Value & obj, const char * name);

const rapidjson::Value & GetJsonItemObject(const rapidjson::Value & obj, rapidjson::SizeType index);
const rapidjson::Value & GetJsonMemberObject(const rapidjson::Value & obj, const char * name);

const rapidjson::Value & GetJsonMemberArray(const rapidjson::Value & obj, const char * name);

int GetJsonMemberInt(const rapidjson::Value & obj, const char * name);

double GetJsonItemDouble(const rapidjson::Value & obj, rapidjson::SizeType index);
double GetJsonMemberDouble(const rapidjson::Value & obj, const char * name);
optional<double> TryGetJsonMemberDouble(const rapidjson::Value & obj, const char * name);

optional<const char *> TryGetJsonMemberString(const rapidjson::Value & obj, const char * name);
