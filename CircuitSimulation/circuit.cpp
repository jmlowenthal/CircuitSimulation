#include "circuit.h"

#include <string>
#include <fstream>
#include <string>
#include <sstream>
#include <iterator>
#include <map>

using namespace std;

void Timeline::add(Event& e) {
	auto lowest = events.end();
	for (auto it = events.rbegin(); it != events.rend(); ++it) {
		lowest = it.base();
		if (it->time <= e.time) break;
	}
	events.insert(lowest, e);
}

void Wire::update() {
	state = false;
	for (Pin* pin : connections) {
		if (pin->driven && pin->state) state = true;
	}
	for (Pin* pin : connections) {
		if (!pin->driven) pin->state = state;
	}
}

string toupper(string a) {
	string res = "";
	for (char& c : a) {
		res += toupper(c);
	}
	return res;
}

LoadResult Circuit::load(const char* filename) {
	// Clean circuit
	wires.clear();
	//circuits.clear();
	comps.clear();
	in.clear();
	out.clear();

	// Parse file
	ifstream file(filename);
	if (!file.is_open()) {
		printf("File not found (%s)\n", filename);
		return LoadResult();
	}

	string line;
	Wire* wire = nullptr;
	unsigned int ln = 0;

	LoadResult res;
	res.success = true;
	while (getline(file, line)) {
		++ln;

		istringstream iss(line);
		vector<string> tokens;

		string token;
		while (iss >> token) {
			// Hash indicates a comment, can be followed by whitespace or not
			if (token[0] != '#') tokens.push_back(toupper(token));
			else break;
		}

		if (tokens.size() < 1) continue;

		try {
			if (tokens[0] == "+") { // Add connection to wire
				if (wire == nullptr)
					throw exception("Trying to connecting to wire when no wire defined");

				if (tokens.size() == 2) { // Circuit I/O pin
					if (res.pins.count(tokens[1]) < 1)
						throw exception(("No pin named " + tokens[1]).c_str());

					if (res.pins[tokens[1]].input) wire->connect(in[res.pins[tokens[1]].index]);
					else wire->connect(out[res.pins[tokens[1]].index]);
				}
				else if (tokens.size() == 4) { // Component pin
					if (res.comps.count(tokens[1]) < 1)
						throw exception(("No component named " + tokens[1]).c_str());
					
					Component& comp = comps[res.comps[tokens[1]]];
					try {
						int index = stoi(tokens[3]);
						if (tokens[2] == "IN") {
							if (index < 0 || index >= (int)comp.in.size())
								throw out_of_range("");
							wire->connect(comp.in[index]);
						}
						else if (tokens[2] == "OUT") {
							if (index < 0 || index >= (int)comp.out.size())
								throw out_of_range("");
							wire->connect(comp.out[index]);
						}
						else throw exception("Second argument should be 'IN' or 'OUT'");
					}
					catch (invalid_argument e) {
						throw exception("Index (last argument) not an integer");
					}
					catch (out_of_range e) {
						throw exception("Index (last argument) is out of range");
					}
				}
				else throw exception("Malformed connection");
			}
			else if (tokens[0] == "WIRE") {
				wires.push_back(Wire());
				wire = &wires.back();
			}
			else if (tokens.size() == 2) { // New component
				if (res.comps.count(tokens[0]) > 0 || res.pins.count(tokens[0]) > 0)
					throw exception("Object by that name has already been defined");

				if (tokens[1] == "IN") {
					in.push_back(Pin(true));
					res.pins.insert(pair<string, PinMapping>(tokens[0], PinMapping(in.size() - 1, true)));
				}
				else if (tokens[1] == "OUT") {
					out.push_back(Pin());
					res.pins.insert(pair<string, PinMapping>(tokens[0], PinMapping(out.size() - 1, false)));
				}
				else if (defs.count(tokens[1]) > 0) {
					comps.push_back(Component(defs[tokens[1]]));
					res.comps.insert(pair<string, unsigned int>(tokens[0], comps.size() - 1));
				}
				else throw exception("Malformed definition");
			}
			else throw exception("Unknown directive");
		}
		catch (exception e) {
			printf("Error in %s on line %i: %s\n", filename, ln, e.what());
			res.success = false;
		}
	}

	return res;
}

void Circuit::update(Timeline &t, unsigned long now) {
	for (unsigned int i = 0; i < wires.size(); ++i) {
		wires[i].update();
	}

	for (unsigned int i = 0; i < comps.size(); ++i) {
		comps[i].update(t, now);
	}

	/*for (Circuit& c : circuits) {
		c.update(t, now);
	}*/
}

#define DELAY 10
void AND(Component& c, Timeline& t, unsigned long now) {
	if (c.in.size() != 2 || c.out.size() != 1)
		throw exception(("Incorrect gate type for function " + string(__FUNCTION__)).c_str());

	bool res = c.in[0].state && c.in[1].state;
	if (c.out[0].state == res) return;
	t.add(Event(now + DELAY, c.out[0], res));
}

void OR(Component& c, Timeline& t, unsigned long now) {
	if (c.in.size() != 2 || c.out.size() != 1)
		throw exception(("Incorrect gate type for function " + string(__FUNCTION__)).c_str());

	bool res = c.in[0].state || c.in[1].state;
	if (c.out[0].state == res) return;
	t.add(Event(now + DELAY, c.out[0], res));
}

void NOT(Component& c, Timeline& t, unsigned long now) {
	if (c.in.size() != 1 || c.out.size() != 1)
		throw exception(("Incorrect gate type for function " + string(__FUNCTION__)).c_str());

	bool res = !c.in[0].state;
	if (c.out[0].state == res) return;
	t.add(Event(now + DELAY, c.out[0], res));
}

void NAND(Component& c, Timeline& t, unsigned long now){
	if (c.in.size() != 2 || c.out.size() != 1)
		throw exception(("Incorrect gate type for function " + string(__FUNCTION__)).c_str());

	bool res = !(c.in[0].state && c.in[1].state);
	if (c.out[0].state == res) return;
	t.add(Event(now + DELAY, c.out[0], res));
}

void NOR(Component& c, Timeline& t, unsigned long now){
	if (c.in.size() != 2 || c.out.size() != 1)
		throw exception(("Incorrect gate type for function " + string(__FUNCTION__)).c_str());

	bool res = !(c.in[0].state || c.in[1].state);
	if (c.out[0].state == res) return;
	t.add(Event(now + DELAY, c.out[0], res));
}

void XOR(Component& c, Timeline& t, unsigned long now) {
	if (c.in.size() != 2 || c.out.size() != 1)
		throw exception(("Incorrect gate type for function " + string(__FUNCTION__)).c_str());

	bool res = (c.in[0].state || c.in[1].state) && !(c.in[0].state && c.in[1].state);
	if (c.out[0].state == res) return;
	t.add(Event(now + DELAY, c.out[0], res));
}

void XNOR(Component& c, Timeline& t, unsigned long now){
	if (c.in.size() != 2 || c.out.size() != 1)
		throw exception(("Incorrect gate type for function " + string(__FUNCTION__)).c_str());

	bool res = !(c.in[0].state || c.in[1].state) || (c.in[0].state && c.in[1].state);
	if (c.out[0].state == res) return;
	t.add(Event(now + DELAY, c.out[0], res));
}
