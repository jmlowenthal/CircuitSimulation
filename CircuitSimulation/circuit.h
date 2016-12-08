#pragma once

#include <list>
#include <vector>
#include <map>

using namespace std;

struct Pin {
	bool state = false;
	bool driven = false;

	Pin() {};
	Pin(bool driven) : driven(driven) {};
	Pin(bool s, bool driven) : state(s), driven(driven) {};
};

struct Event {
	unsigned long time = 0;
	bool state = false;
	Pin& pin;

	Event(unsigned long t, Pin& p, bool s) : time(t), pin(p), state(s) {};
};

class Wire {
	bool state = false;

public:
	list<Pin*> connections;

	void update();
	void connect(Pin* p) { connections.push_back(p); };
	void connect(Pin& p) { connections.push_back(&p); };
	bool getState() { return state; };
};

class Timeline {
	list<Event> events;

public:
	void add(Event& e);
	list<Event>::iterator begin() { return events.begin(); };
	list<Event>::iterator end() { return events.end(); };
	void erase(list<Event>::iterator it) { events.erase(it); };
};

struct Component;

struct ComponentDef {
	typedef void(*updatefunc)(Component&, Timeline&, unsigned long);

	const unsigned int pins_in;
	const unsigned int pins_out;
	updatefunc update;

	ComponentDef()
		: pins_in(0), pins_out(0), update(nullptr) {};

	ComponentDef(unsigned int in, unsigned int out, updatefunc tick)
		: pins_in(in), pins_out(out), update(tick) {}

	/*void operator=(ComponentDef& comp) {
		pins_in = comp.pins_in;
		pins_out = comp.pins_out;
		update = comp.update;
	}*/
};

struct Component {
	const ComponentDef& def;
	vector<Pin> in;
	vector<Pin> out;

	Component(ComponentDef& def_)
		: def(def_), in(def_.pins_in), out(def_.pins_out) {
		for (unsigned int i = 0; i < def.pins_in; ++i) in[i] = Pin();
		for (unsigned int o = 0; o < def.pins_out; ++o) out[o] = Pin(true);
	}

	Component(const ComponentDef& def_)
		: def(def_), in(def_.pins_in), out(def_.pins_out) {
		for (unsigned int i = 0; i < def.pins_in; ++i) in[i] = Pin();
		for (unsigned int o = 0; o < def.pins_out; ++o) out[o] = Pin(true);
	}

	void update(Timeline& t, unsigned long now) {
		def.update(*this, t, now);
	};
};

struct PinMapping {
	unsigned int index;
	bool input;

	PinMapping() {};
	PinMapping(unsigned int i, bool inp) : index(i), input(inp) {};
};

struct LoadResult {
	bool success = false;
	map<string, unsigned int> comps;
	map<string, PinMapping> pins;
};

struct Circuit {
	vector<Wire> wires;
	//vector<Circuit> circuits;
	vector<Component> comps;
	map<string, ComponentDef>& defs;

	Circuit(map<string, ComponentDef>& define) : defs(define) {};

	vector<Pin> in;
	vector<Pin> out;

	LoadResult load(const char* filename);
	void update(Timeline &t, unsigned long now);
};

// Update function should check if it gets the right type of component
void AND(Component& c, Timeline& t, unsigned long now);
void OR(Component& c, Timeline& t, unsigned long now);
void NOT(Component& c, Timeline& t, unsigned long now);

void NAND(Component& c, Timeline& t, unsigned long now);
void NOR(Component& c, Timeline& t, unsigned long now);
void XOR(Component& c, Timeline& t, unsigned long now);
void XNOR(Component& c, Timeline& t, unsigned long now);

// Selectors are first, then data lines
template<unsigned int selectors>
void MUX(Component& c, Timeline& t, unsigned long now) {
	if (c.in.size() != selectors + (1 << selectors) || c.out.size() != 1)
		throw exception(("Incorrect gate type for function " + string(__FUNCTION__)).c_str());
	
	unsigned int select = 0;
	for (unsigned int i = 0; i < selectors; ++i) {
		if (c.in[i].state) select += 1 << i;
	}
	bool res = c.in[selectors + select].state;
	if (c.out[0].state == res) return;
	t.add(Event(now + 10, c.out[0], res));
}