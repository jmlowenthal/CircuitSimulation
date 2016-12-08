#include <SDL.h>
#include <graphics.h>

#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <time.h>

#include "circuit.h"
#include "font.h"

void drawString(GraphicsManager& gm, string s, Vector2D pos, Vector2D scale = Vector2D(0.5, 0.5), GLfloat strength = 1, double spacing = 1) {
	glLineWidth(strength);
	for (char c : s) {
		if (c == '\n') pos[1][0] -= scale[1][0] * spacing;
		else pos[0][0] += drawChar(gm, c, pos, scale) * spacing;
	}
	glLineWidth(1);
}

#define W 1000
#define H 500
int main(int argv, char* argc[]) {
	map<string, ComponentDef> defs;
	defs.insert(pair<string, ComponentDef>("AND", ComponentDef(2, 1, AND)));
	defs.insert(pair<string, ComponentDef>("OR", ComponentDef(2, 1, OR)));
	defs.insert(pair<string, ComponentDef>("NOT", ComponentDef(1, 1, NOT)));
	defs.insert(pair<string, ComponentDef>("NAND", ComponentDef(2, 1, NAND)));
	defs.insert(pair<string, ComponentDef>("NOR", ComponentDef(2, 1, NOR)));
	defs.insert(pair<string, ComponentDef>("XOR", ComponentDef(2, 1, XOR)));
	defs.insert(pair<string, ComponentDef>("XNOR", ComponentDef(2, 1, XNOR)));
	defs.insert(pair<string, ComponentDef>("4TO1", ComponentDef(6, 1, MUX<2>)));

	Timeline t; Circuit root(defs);
	LoadResult res = root.load("..\\mux.net");
	if (!res.success) return -1;

	// Setup graphix manager
	GraphicsManager gm;
	gm.init("Simulation", AARect(Vector2D(50, 50), Vector2D(W, H)), 0);
	
	// Preprocess circuit for events
	root.update(t, 0);

	typedef chrono::system_clock clock;
	typedef clock::time_point tp;
	typedef chrono::duration<long double, std::deci> scale;

	long double circuittime = 0;
	tp last = clock::now();
	float timefactor = 1.0f;
	bool play = true;

	bool quit = false;
	SDL_Event e;
	while (!quit) {
		tp t_now = clock::now();
		if (play) circuittime += chrono::duration_cast<scale>(t_now - last).count() * timefactor;
		last = t_now;

		// Process all events that happen now
		auto it = t.begin();
		while (it != t.end()) {
			if (it->time > circuittime) break;
			it->pin.state = it->state;
			printf("Event: Set pin %p to %d\n", &it->pin, it->state);
			t.erase(it); // auto increments
		}

		root.update(t, (unsigned long)circuittime);

		// Draw circuit state
		gm.rendererClear(WHITE);
		gm.setRendererDrawColour(BLUE);

		drawString(gm, "Time: " + to_string(circuittime), Vector2D(10, 30));
		gm.render();

		// Handle input events
		while (SDL_PollEvent(&e) && !quit) {
			switch (e.type) {
				case SDL_QUIT:
					quit = true;
					break;
				case SDL_KEYDOWN:
					if (e.key.keysym.sym == SDLK_SPACE)
						play = !play;
					break;
			}
		}
	}

	// Iterate timed events
	/*SDL_Event e;
	unsigned long last = 0;
	bool quit = false, paused = false;
	for (auto it = t.begin(); it != t.end() && !quit; ++it) {
		unsigned long now = it->time;

		typedef chrono::system_clock clock;
		typedef clock::time_point tp;
		typedef chrono::duration<long long, std::deci> scale;

		tp start = clock::now();
		for (tp t_now = clock::now(); start + scale(now - last) > t_now && !quit; t_now = clock::now()) {
			gm.rendererClear(WHITE);
			gm.setRendererDrawColour(BLUE);
			glLineWidth(1);

			drawString(gm, "Time: " + to_string(chrono::duration_cast<scale>(t_now - start).count() + last), Vector2D(10, 40));

			double alpha = 2 * M_PI / i;
			for (pair<Pin*, unsigned int> pin : positions) {
				CircleShape c(Vector2D(W / 2 + W / 3 * cos(alpha*pin.second),
					H / 2 + H / 3 * sin(alpha*pin.second)), 5);
				gm.drawCircle(&c, pin.first->state);
			}

			for (Wire& w : root.wires) {
				vector<Vector2D> v;
				for (Pin* p : w.connections) {
					v.push_back(Vector2D(W / 2 + W / 3 * cos(alpha*positions[p]),
						H / 2 + H / 3 * sin(alpha*positions[p])));
				}
				if (w.getState()) glLineWidth(2);
				gm.plot(&v[0], v.size(), GL_LINE_LOOP);
				glLineWidth(1);
			}

			// Label pins
			for (pair<string, PinMapping> p : res.pins) {
				Pin* pin;
				if (p.second.input) pin = &root.in[p.second.index];
				else pin = &root.out[p.second.index];
				drawString(gm, p.first, Vector2D(5, -5) + Vector2D(W / 2 + W / 3 * cos(alpha*positions[pin]),
					H / 2 + H / 3 * sin(alpha*positions[pin])));
			}

			for (pair<string, unsigned int> c : res.comps) {
				for (unsigned int i = 0; i < root.comps[c.second].in.size(); ++i) {
					Pin* pin = &root.comps[c.second].in[i];
					drawString(gm, c.first + " I" + to_string(i), Vector2D(5, -5) + Vector2D(W / 2 + W / 3 * cos(alpha*positions[pin]),
						H / 2 + H / 3 * sin(alpha*positions[pin])));
				}
				for (unsigned int o = 0; o < root.comps[c.second].out.size(); ++o) {
					Pin* pin = &root.comps[c.second].out[o];
					drawString(gm, c.first + " O" + to_string(o), Vector2D(5, -5) + Vector2D(W / 2 + W / 3 * cos(alpha*positions[pin]),
						H / 2 + H / 3 * sin(alpha*positions[pin])));
				}
			}

			gm.render();

			tp delaystart = clock::now();
			do {
				while (SDL_PollEvent(&e) && !quit) {
					if (e.type == SDL_QUIT) quit = true;
					if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_SPACE) paused = !paused;
				}
			}
			while (paused);
			start += clock::now() - delaystart;
		}
		last = now;

		printf("Time: %d\n", now);
		for (; it != t.end(); ++it) {
			if (it->time != now) break;
			if (it->pin.state == it->state) continue;
			it->pin.state = it->state;
			printf("Event: Set pin %p to %d\n", &it->pin, it->state);
		}
		--it;

		printf("\n");
		writeCirc(root, defs);

		root.update(t, now);

		while (paused) {
			while (SDL_PollEvent(&e) && !quit) {
				if (e.type == SDL_QUIT) quit = true;
				if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_SPACE) paused = !paused;
			}
		}

		printf("\n\n");
	}

	if (!quit) {
		writeCirc(root, defs);
		printf("\n[Done]\n\n");
		pause();
	}*/

	return 0;
}