#include "live.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "pcg/pcg_random.hpp"
#include <random>

using namespace std;
using namespace rapidjson;


bool Live::prepare(const char * json) {
	Document doc;
	if (doc.Parse(json).HasParseError()) return false;
	if (!doc.IsArray()) return false;
	unsigned noteNum = doc.Size();
	combos.resize(noteNum);
	notes.resize(noteNum);
	for (unsigned i = 0; i < noteNum; i++) {
		if (!doc[i].IsObject()) return false;
		auto & note = combos[i];
		if (!doc[i]["position"].IsInt()) return false;
		note.position = doc[i]["position"].GetInt();
		if (!doc[i]["effect"].IsInt()) return false;
		note.effect(doc[i]["effect"].GetInt());
		if (!doc[i]["timing_sec"].IsDouble()) return false;
		double t = doc[i]["timing_sec"].GetDouble();
		// SIF built-in offset :<
		t -= 0.1;
		notes[i] = t - 0.7;
		if (note.isHold) {
			if (!doc[i]["effect_value"].IsDouble()) return false;
			t += doc[i]["effect_value"].GetDouble();
		}
		note.hitTime = t;
	}
	return true;
}


int Live::simulate() {
	return 0;
}
