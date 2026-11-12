// The goop weapons (fire, electric and black goop), the mod's own (through the mod API's general
// tools, API 1.8): FLUDD sprays as ever (its own model, animation, pump, sound and water), but its
// stream is goop, not water. Its drops carry the kind in their tag and are left out of the water's
// drawing (SMS_EV_WATER_EMIT's tag and unseen); they are drawn here as goop (SMS_EV_DRAW_SCENE), and
// where one lands (SMS_EV_WATER_LAND) it leaves goop: a puddle on the ground, or a blob on a wall
// dripping down it, or one there grown bigger.
//
// Mario stepping in a puddle is hurt as by the game's own goop: fire throws him up burning
// (player->burn), electric shocks him. Black goop is slippery and fast. Enemies (with "can defeat
// enemies") touched by the stream or standing in a puddle are shocked (held trembling, sparks on
// them) or set burning (the game's flames), then defeated; fire throws them into the nearest fire,
// and walking enemies are drawn to it. Water landing in a puddle washes it away bit by bit (the
// bazooka's burst at once); left alone a puddle lasts the setting's seconds, shrinking away at its end.
#pragma once
#include "sms_mod.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <vector>

namespace goop {

const int kFire = 5, kElectric = 6, kBlack = 7; // SMS_SPRAY_FIRE_GOOP, _ELECTRIC_GOOP, _BLACK_GOOP
inline bool isKind(int k) { return k == kFire || k == kElectric || k == kBlack; }

// a goop drop's tag: 0x5B, its player and its kind
const uint32_t kTagMark = 0x5B000000u;
inline uint32_t tagOf(int player, int kind) { return kTagMark | ((uint32_t)(player & 1) << 23) | (uint32_t)(kind & 0xF); }
inline int kindOfTag(uint32_t tag) { return (tag & 0xFF000000u) == kTagMark && isKind((int)(tag & 0xF)) ? (int)(tag & 0xF) : 0; }

// the game's numbers
const uint32_t kStatusJumpBackDown = 0x208B3, kStatusFireDown = 0x208B7, kStatusElectric = 0x20338;
const uint32_t kSoundWallHard = 0x1803, kSoundElecDamage = 0x1814, kSoundFire = 0x3803, kSoundLightning = 0x3806;
const uint32_t kSoundElecDown = 0x28ED;

// each kind's numbers (the mod's menu): seconds burning or shocked that defeat an enemy, how long its
// goop lasts (seconds), its puddles' size (times their own), times it hurts an enemy before it is
// defeated, the water it uses (times FLUDD's own)
struct Stats {
	float defeat = 1, life = 60, size = 1;
	int hits     = 1;
	float water  = 1;
};

struct Settings {
	Stats kinds[3];             // fire, electric, black
	float blackSpeed = 1;       // black goop's speed, times his own
	float airTurn    = 0.5f;    // turning in the air after a jump off black goop: 0 none, 1 at once
	float wallLife   = 30;      // seconds goop lasts on walls
	bool walls       = true;    // goop sticks to walls
	bool npcs        = true;    // friendly characters shocked and burnt (never defeated)
	bool defeat      = true;    // enemies defeated by it
	int quality      = 3;       // its look: 0 very low (simplest puddles, fewest flames) to 4 very high
	const Stats& of(int kind) const { return kinds[kind == kElectric ? 1 : kind == kBlack ? 2 : 0]; }
};

struct V3 {
	float x = 0, y = 0, z = 0;
	V3() {}
	V3(float a, float b, float c) : x(a), y(b), z(c) {}
	explicit V3(const SmsVec3& v) : x(v.x), y(v.y), z(v.z) {}
	SmsVec3 s() const
	{
		SmsVec3 v;
		v.x = x, v.y = y, v.z = z;
		return v;
	}
};

#define GOOP_HAS(t, T, f) ((t) && (t)->size >= offsetof(T, f) + sizeof(void*) && (t)->f)

struct Goop {
	const SmsApi* api = 0;
	SmsMod* mod       = 0;
	Settings set;
	float time = 0; // seconds played
	int frame  = 0; // drawn frames (the goop's flow, the flames)

	// ---------------------------------------------------------------- what it has

	struct Player {
		int kind = 0;             // the goop weapon chosen, 0 none
		bool havePrev = false, gliding = false; // black goop: gliding, from where he was last tick
		float prevX = 0, prevZ = 0, glideX = 0, glideZ = 0;
		int lastWater = -1;       // FLUDD's tank last tick (the goop uses more)
		float waterCarry = 0;
		double emittedAt = -10;   // its stream last let out (the tube from the nozzle)
		bool held = false;        // FLUDD held in view (first person: a thinner stream near the eye)
	};
	Player players[2];

	struct Puddle {
		bool on = false;
		int kind = 0;
		float x = 0, y = 0, z = 0;    // its middle, on the ground
		float nx = 0, ny = 1, nz = 0; // the ground's way up there
		float r = 0;                  // its size (radius)
		float born = 0;               // when it was last added to
		float seed = 0;               // its own shape and flicker
		float inside[2] = {0, 0};     // each player standing in it (electric's moment before it shocks)
	};
	static const int kPuddles = 64;
	Puddle puddles[kPuddles];

	struct WallGoop {
		bool on = false;
		int kind = 0;
		float x = 0, y = 0, z = 0, nx = 0, ny = 0, nz = 0, r = 0;
		float born = 0, since = 0, seed = 0;
	};
	static const int kWalls = 48;
	WallGoop walls[kWalls];

	// enemies touched by the goop: shocked (held still, trembling, sparks) or burning, then defeated
	struct Zap {
		SmsObject e = 0;
		int kind = 0;
		float start = 0;
		float bx = 0, by = 0, bz = 0; // where it was shocked (held there)
		int hits = 0;
		bool resting = false;
		float until = 0;
		bool npc = false;
	};
	static const int kZaps = 24;
	Zap zaps[kZaps];

	// thrown into fire: carried on an arc into the middle of a puddle of fire goop
	struct Flight {
		SmsObject a = 0;
		float sx = 0, sy = 0, sz = 0, tx = 0, ty = 0, tz = 0, length = 0, at = 0, high = 0;
	};
	static const int kFlights = 24;
	Flight flights[kFlights];

	// which enemies move by themselves (one that never does, a Piranha Plant say, is never drawn to fire)
	struct Mover {
		SmsObject e = 0;
		float x = 0, z = 0, moved = 0;
		bool held = false;
		float hx = 0, hz = 0, hitAt = -100, heldAt = -100;
		float launchedAt = -100; // thrown into the fire last (once a second and a half at most)
	};
	static const int kMovers = 256;
	Mover movers[kMovers];
	float enemyCheck = 0;

	// an electric goop drop about to reach something: its push by the water taken back after
	SmsObject unpushed = 0;
	SmsVec3 unpushedV;

	uint32_t seed = 777u;
	float random01()
	{
		seed = seed * 1664525u + 1013904223u;
		return (float)((seed >> 8) & 0xFFFF) / 65535.0f;
	}

	static const int kStartSize = 95, kMostSize = 380;
	const float kGrow = 5.0f, kWash = 5.0f, kGone = 14.0f, kFade = 4.0f;

	// ---------------------------------------------------------------- the players

	const SmsHeldFluddApi* hf() const { return api->held_fludd; }
	int playerCount() const
	{
		const SmsHeldFluddApi* h = hf();
		return GOOP_HAS(h, SmsHeldFluddApi, players) ? h->players() : 1;
	}
	struct For {
		const SmsHeldFluddApi* h;
		bool on;
		For(const SmsHeldFluddApi* hh, int p) : h(hh), on(GOOP_HAS(hh, SmsHeldFluddApi, select_player))
		{
			if (on)
				h->select_player(p);
		}
		~For()
		{
			if (on)
				h->select_player(-1);
		}
	};
	bool mario(SmsPlayer* m)
	{
		*m      = SmsPlayer();
		m->size = sizeof *m;
		return api->player->get(m) && m->valid;
	}
	bool hasInvincible(const SmsPlayer& m) const { return m.size >= offsetof(SmsPlayer, invincible) + sizeof(int); }

	// ---------------------------------------------------------------- puddles

	float sizeNow(const Puddle& p) const
	{
		const float left = set.of(p.kind).life - (time - p.born);
		return left >= kFade ? p.r : left > 0.0f ? p.r * left / kFade : 0.0f;
	}
	float wallSizeNow(const WallGoop& g) const
	{
		const float left = set.wallLife - (time - g.born);
		return left >= 3.0f ? g.r : left > 0.0f ? g.r * left / 3.0f : 0.0f;
	}

	// goop never lands round Mario's feet: how big goop at (x, y, z) may be and keep clear of him
	float roomFrom(float x, float y, float z, bool wall)
	{
		float room = 1e30f;
		for (int p = 0; p < playerCount(); p++) {
			For f(hf(), p);
			SmsPlayer m;
			if (!mario(&m))
				continue;
			const float dx = m.position.x - x, dz = m.position.z - z;
			const float dy = wall ? m.position.y + 80.0f - y : m.position.y - y;
			if (!wall && fabsf(dy) > 150.0f)
				continue;
			const float d = wall ? sqrtf(dx * dx + dy * dy + dz * dz) : sqrtf(dx * dx + dz * dz);
			const float k = (d - 45.0f) / 1.25f;
			room          = k < room ? k : room;
		}
		return room;
	}

	void addGoop(int kind, float x, float y, float z, const SmsVec3& n)
	{
		const float size = set.of(kind).size;
		Puddle* best     = 0;
		float bestD      = 1e30f;
		for (int i = 0; i < kPuddles; i++) { // the same kind's puddle it lands in: grown
			Puddle& p = puddles[i];
			if (!p.on || p.kind != kind || fabsf(p.y - y) > 50.0f)
				continue;
			const float dx = p.x - x, dz = p.z - z, d = dx * dx + dz * dz;
			if (d < p.r * p.r && d < bestD)
				best = &p, bestD = d;
		}
		if (best) {
			float most       = kMostSize * size;
			const float room = roomFrom(best->x, best->y, best->z, false);
			most             = most < room ? most : room; // not grown under his feet
			if (best->r + kGrow * size <= most)
				best->r += kGrow * size;
			best->born = time;
			return;
		}
		float start      = kStartSize * size;
		const float room = roomFrom(x, y, z, false);
		if (room < 20.0f)
			return; // at his feet: none
		start     = start < room ? start : room;
		Puddle* p = 0; // a free place, or the oldest's
		for (int i = 0; i < kPuddles && !p; i++)
			if (!puddles[i].on)
				p = &puddles[i];
		if (!p) {
			p = &puddles[0];
			for (int i = 1; i < kPuddles; i++)
				if (puddles[i].born < p->born)
					p = &puddles[i];
		}
		*p      = Puddle();
		p->on   = true;
		p->kind = kind;
		p->x = x, p->y = y, p->z = z;
		if (n.y > 0.3f)
			p->nx = n.x, p->ny = n.y, p->nz = n.z;
		p->r    = start;
		p->born = time;
		p->seed = random01() * 100.0f;
	}

	void washAt(float x, float y, float z, float reach, float amount)
	{
		for (int i = 0; i < kPuddles; i++) {
			Puddle& p = puddles[i];
			if (!p.on || fabsf(p.y - y) > 80.0f)
				continue;
			const float dx = p.x - x, dz = p.z - z, r = p.r + reach;
			if (dx * dx + dz * dz > r * r)
				continue;
			p.r -= amount;
			if (p.r < kGone)
				p.on = false;
		}
	}

	void addWallGoop(int kind, float x, float y, float z, const SmsVec3& n)
	{
		if (fabsf(n.y) > 0.6f)
			return; // not a wall
		const float size = set.of(kind).size;
		const float room = roomFrom(x, y, z, true);
		if (room < 15.0f)
			return; // where he is: none
		for (int i = 0; i < kWalls; i++) { // the same kind's blob it hits: grown
			WallGoop& g = walls[i];
			if (!g.on || g.kind != kind || g.nx * n.x + g.ny * n.y + g.nz * n.z < 0.8f)
				continue;
			const float dx = g.x - x, dy = g.y - y, dz = g.z - z;
			if (dx * dx + dy * dy + dz * dz < g.r * g.r) {
				const float r2   = roomFrom(g.x, g.y, g.z, true);
				const float most = 140.0f * size < r2 ? 140.0f * size : r2;
				if (g.r + 2.5f * size <= most)
					g.r += 2.5f * size;
				g.born = time;
				return;
			}
		}
		WallGoop* g = 0;
		for (int i = 0; i < kWalls && !g; i++)
			if (!walls[i].on)
				g = &walls[i];
		if (!g) {
			g = &walls[0];
			for (int i = 1; i < kWalls; i++)
				if (walls[i].born < g->born)
					g = &walls[i];
		}
		const float hl = sqrtf(n.x * n.x + n.z * n.z);
		if (hl < 1e-3f)
			return;
		g->on = true, g->kind = kind;
		g->x = x, g->y = y, g->z = z;
		g->nx = n.x / hl, g->ny = 0.0f, g->nz = n.z / hl;
		g->r    = 38.0f * size < room ? 38.0f * size : room;
		g->born = g->since = time;
		g->seed = random01() * 100.0f;
	}

	void washWallAt(float x, float y, float z, float reach, float amount)
	{
		for (int i = 0; i < kWalls; i++) {
			WallGoop& g = walls[i];
			if (!g.on)
				continue;
			const float dx = g.x - x, dy = g.y - y, dz = g.z - z, r = g.r + reach;
			if (dx * dx + dy * dy + dz * dz > r * r)
				continue;
			g.r -= amount;
			if (g.r < 10.0f)
				g.on = false;
		}
	}

	// the bazooka's burst washing goop off: every puddle and wall blob in it
	void washBurst(float x, float y, float z, float r)
	{
		washWallAt(x, y, z, r, 1000.0f);
		for (int i = 0; i < kPuddles; i++) {
			Puddle& p = puddles[i];
			if (!p.on || fabsf(p.y - y) > 300.0f)
				continue;
			const float dx = p.x - x, dz = p.z - z;
			if (dx * dx + dz * dz <= r * r)
				p.on = false;
		}
	}

	bool anyGoop() const
	{
		for (int i = 0; i < kPuddles; i++)
			if (puddles[i].on)
				return true;
		for (int i = 0; i < kWalls; i++)
			if (walls[i].on)
				return true;
		return false;
	}

	// ---------------------------------------------------------------- enemies

	bool isNpc(SmsObject e) const { return (api->actor->flags(e) & SMS_ACTOR_FLAG_CHARACTER) != 0; }
	bool isSmall(SmsObject e) const { return (api->actor->flags(e) & SMS_ACTOR_FLAG_SMALL_ENEMY) != 0; }
	bool gone(SmsObject e) const { return (api->actor->flags(e) & (SMS_ACTOR_FLAG_DEAD | SMS_ACTOR_FLAG_HIDDEN)) != 0; }
	V3 posOf(SmsObject e) const
	{
		SmsVec3 p;
		p.x = p.y = p.z = 0.0f;
		api->game->object_position(e, &p);
		return V3(p);
	}
	void place(SmsObject e, float x, float y, float z)
	{
		const SmsVec3 p = V3(x, y, z).s();
		api->actor->set_position(e, &p);
	}

	// the fire goop puddle nearest (x, y, z) within `reach` of its edge, or null; `edge` how far
	// outside its edge (less than 0: inside)
	const Puddle* nearFire(float x, float y, float z, float reach, float* edge) const
	{
		const Puddle* best = 0;
		float bestEdge     = 1e30f;
		for (int i = 0; i < kPuddles; i++) {
			const Puddle& p = puddles[i];
			if (!p.on || p.kind != kFire || fabsf(p.y - y) > 300.0f)
				continue;
			const float dx = p.x - x, dz = p.z - z;
			const float out = sqrtf(dx * dx + dz * dz) - sizeNow(p);
			if (out < reach && out < bestEdge)
				bestEdge = out, best = &p;
		}
		if (edge)
			*edge = bestEdge;
		return best;
	}

	bool flyingNow(SmsObject e) const
	{
		for (int i = 0; i < kFlights; i++)
			if (flights[i].a == e)
				return true;
		return false;
	}
	bool zappedNow(SmsObject e) const
	{
		for (int i = 0; i < kZaps; i++)
			if (zaps[i].e == e)
				return true;
		return false;
	}

	// fire goop stepped in (or the fire stream): thrown up two and a half times its height, into the
	// middle of the nearest other puddle of fire; with none, a hop a random way
	void launchEnemy(SmsObject e)
	{
		if (flyingNow(e))
			return;
		Mover* mv = moverOf(e, true);
		if (mv && time - mv->launchedAt < 1.5f)
			return; // (just thrown)
		if (mv)
			mv->launchedAt = time;
		float rad = 0, tall = 0;
		api->actor->hit_size(e, &rad, &tall);
		tall = tall > 20.0f ? tall : 100.0f;
		const V3 at = posOf(e);
		// the nearest other puddle of fire (the one it is in when there is no other: up and back into
		// its middle)
		const Puddle* to = 0;
		const Puddle* in = 0;
		float best       = 1e30f;
		for (int i = 0; i < kPuddles; i++) {
			const Puddle& p = puddles[i];
			if (!p.on || p.kind != kFire || fabsf(p.y - at.y) > 2000.0f)
				continue;
			const float dx = p.x - at.x, dz = p.z - at.z, d = dx * dx + dz * dz;
			const float r = sizeNow(p);
			if (d < r * r) {
				in = &p;
				continue;
			}
			if (d < best)
				best = d, to = &p;
		}
		if (!to)
			to = in;
		if (!to) { // no fire: a hop a random way
			float g = api->actor->gravity(e);
			g       = g > 0.05f ? g : 0.5f;
			const float a = random01() * 6.2831853f;
			SmsVec3 v;
			api->actor->velocity(e, &v);
			v.y = sqrtf(2.0f * g * 2.5f * tall);
			v.x = sinf(a) * 6.0f, v.z = cosf(a) * 6.0f;
			api->actor->set_velocity(e, &v);
			place(e, at.x, at.y + 8.0f, at.z);
			api->actor->set_flags(e, SMS_ACTOR_FLAG_AIRBORNE, 0);
			return;
		}
		Flight* f = 0;
		for (int i = 0; i < kFlights && !f; i++)
			if (!flights[i].a)
				f = &flights[i];
		if (!f)
			return;
		const float dx = to->x - at.x, dz = to->z - at.z, d = sqrtf(dx * dx + dz * dz);
		f->a  = e;
		f->sx = at.x, f->sy = at.y, f->sz = at.z;
		f->tx = to->x, f->ty = to->y, f->tz = to->z;
		f->length = 0.7f + d / 1500.0f;
		f->length = f->length < 2.5f ? f->length : 2.5f;
		f->at     = 0.0f;
		f->high   = 2.5f * tall + d * 0.1f;
		api->actor->set_flags(e, SMS_ACTOR_FLAG_AIRBORNE, 0);
	}

	void flightsTick(float dt)
	{
		const SmsVec3 still = V3().s();
		for (int i = 0; i < kFlights; i++) {
			Flight& f = flights[i];
			if (!f.a)
				continue;
			if (gone(f.a)) {
				f.a = 0;
				continue;
			}
			f.at += dt;
			const float k = f.at < f.length ? f.at / f.length : 1.0f;
			float y       = f.sy + (f.ty - f.sy) * k + 4.0f * f.high * k * (1.0f - k);
			if (k >= 1.0f) // down, in the fire: held there a little while
				y = f.ty;
			place(f.a, f.sx + (f.tx - f.sx) * k, y, f.sz + (f.tz - f.sz) * k);
			api->actor->set_velocity(f.a, &still);
			if (k >= 1.0f) {
				api->actor->set_flags(f.a, 0, SMS_ACTOR_FLAG_AIRBORNE);
				if (f.at >= f.length + 0.4f)
					f.a = 0;
			}
		}
	}

	// `direct`: hit by the stream itself (a burn or shock counted now); else touched by the goop
	void burn(SmsObject e, int kind, bool launch = false, bool direct = false)
	{
		if (kind == kBlack || !e || gone(e))
			return;
		const bool npc = isNpc(e);
		if (npc && !set.npcs)
			return;
		if (npc)
			direct = false;
		if (launch && kind == kFire)
			launchEnemy(e); // thrown into the nearest fire
		if (!set.defeat && !npc)
			return;
		Zap* free = 0;
		for (int i = 0; i < kZaps; i++) {
			Zap& z = zaps[i];
			if (z.e == e) {
				if (direct && !z.resting) {
					z.start = time - set.of(z.kind).defeat; // the stream on it: counted at once
					return;
				}
				if (!z.resting || time < z.until - (direct ? 0.35f : 0.0f))
					return; // already, or resting a moment
				free = &z;
				break;
			}
			if (!free && !z.e)
				free = &z;
		}
		if (!free)
			for (int i = 0; i < kZaps; i++)
				if (zaps[i].resting && (!free || zaps[i].until < free->until))
					free = &zaps[i];
		if (!free)
			return;
		if (free->e != e)
			free->hits = 0;
		free->resting = false;
		free->e       = e;
		free->npc     = npc;
		free->kind    = kind;
		free->start   = direct ? time - set.of(kind).defeat : time;
		const V3 at   = posOf(e);
		free->bx = at.x, free->by = at.y, free->bz = at.z;
		const SmsVec3 s = at.s();
		api->audio->play_sound_at(kind == kFire ? kSoundFire : kSoundElecDown, &s);
		api->core->log(mod, SMS_LOG_INFO, "%s %s by goop", npc ? "a character" : "an enemy", kind == kFire ? "set burning" : "shocked");
	}

	// each tick: the shocked held trembling with sparks, the burning in flames; defeated at the end
	void zapsTick()
	{
		for (int i = 0; i < kZaps; i++) {
			Zap& z = zaps[i];
			if (!z.e)
				continue;
			if (gone(z.e)) {
				z.e = 0;
				continue;
			}
			if (z.resting)
				continue;
			const Stats& st = set.of(z.kind);
			if (time - z.start >= st.defeat && z.npc) {
				z.resting = true; // a character: let go, never defeated
				z.until   = time + 1.0f;
				z.hits    = 0;
				continue;
			}
			if (time - z.start >= st.defeat && ++z.hits < st.hits) {
				z.resting = true; // let go; again when it touches the goop again
				z.until   = time + 0.6f;
				continue;
			}
			if (time - z.start >= st.defeat) {
				api->actor->effect(z.e, SMS_EFFECT_SMOKE);
				if (isSmall(z.e))
					api->actor->defeat(z.e); // its own defeat, coins and all
				z.e = 0;
				continue;
			}
			if (z.kind == kElectric) { // held where it was shocked, trembling
				float rad = 0, tall = 0;
				api->actor->hit_size(z.e, &rad, &tall);
				const float j = 3.0f * (tall > 20.0f ? tall / 100.0f : 1.0f);
				place(z.e, z.bx + (random01() - 0.5f) * 2.0f * j, z.by, z.bz + (random01() - 0.5f) * 2.0f * j);
			}
			api->actor->effect(z.e, z.kind == kElectric ? SMS_EFFECT_SHOCKED : SMS_EFFECT_BURNING);
		}
	}

	// a Phantamanta hit by fire goop: three hits of water's (half a second till the next)
	void mantaFire(SmsObject m)
	{
		SmsWaterMeter wm = SmsWaterMeter();
		wm.size          = sizeof wm;
		if (!api->actor->water_meter(m, &wm) || !wm.open || wm.wait > 0)
			return;
		api->actor->effect(api->actor->owner(m), SMS_EFFECT_SMOKE);
		const int left = wm.left - 3 > 0 ? wm.left - 3 : 0;
		api->actor->set_water_meter(m, left, 1);
		api->actor->set_water_wait(m, 15);
		api->core->log(mod, SMS_LOG_INFO, "fire goop on a Phantamanta: %d of %d hits left", left, wm.most);
	}

	SmsObject everyone[1024];
	int everyoneCount = 0;
	SmsObject mantas[24];
	int mantaCount = 0;
	float mantasAt = -10;

	// which enemies move by themselves (their own moving added up)
	Mover* moverOf(SmsObject e, bool make)
	{
		Mover* free = 0;
		for (int i = 0; i < kMovers; i++) {
			if (movers[i].e == e)
				return &movers[i];
			if (!free && !movers[i].e)
				free = &movers[i];
		}
		if (free && make) {
			const V3 at = posOf(e);
			*free       = Mover();
			free->e = e, free->x = at.x, free->z = at.z;
			return free;
		}
		return 0;
	}
	void watchMovers()
	{
		for (int i = 0; i < everyoneCount; i++) {
			const SmsObject e = everyone[i];
			if (!isSmall(e))
				continue;
			Mover* m = moverOf(e, true);
			if (!m)
				continue;
			const V3 at    = posOf(e);
			const float dx = at.x - m->x, dz = at.z - m->z, d = sqrtf(dx * dx + dz * dz);
			if (d < 400.0f && !flyingNow(e) && !(m->held && time - m->heldAt < 0.2f))
				m->moved += d;
			m->x = at.x, m->z = at.z;
		}
	}

	// walking enemies drawn to fire goop within 2500: they walk into it (Mario ignored) and stay
	void walkToFire(float dt)
	{
		for (int i = 0; i < everyoneCount; i++) {
			const SmsObject e = everyone[i];
			if (!isSmall(e))
				continue;
			Mover* mv          = moverOf(e, false);
			const bool wasHeld = mv && mv->held && time - mv->heldAt < 0.2f;
			if (mv)
				mv->held = false;
			if (!mv || mv->moved <= 60.0f || (api->actor->flags(e) & SMS_ACTOR_FLAG_AIRBORNE) || flyingNow(e) || zappedNow(e))
				continue;
			V3 at = posOf(e);
			if (wasHeld && time - mv->hitAt > 0.5f) { // its own moving since (chasing him) undone, its chase stopped
				at.x = mv->hx, at.z = mv->hz;
				place(e, at.x, at.y, at.z);
				SmsVec3 v;
				if (api->actor->velocity(e, &v)) {
					v.x = v.z = 0.0f;
					api->actor->set_velocity(e, &v);
				}
			}
			float edge;
			const Puddle* p = nearFire(at.x, at.y, at.z, 2500.0f, &edge);
			if (!p)
				continue;
			float dx = p->x - at.x, dz = p->z - at.z;
			const float d = sqrtf(dx * dx + dz * dz), r = sizeNow(*p);
			if (d < r * 0.4f || d < 1.0f) { // well in it: kept there
				mv->heldAt = time, mv->held = true, mv->hx = at.x, mv->hz = at.z;
				mv->x = at.x, mv->z = at.z;
				continue;
			}
			dx /= d, dz /= d;
			float step = 260.0f * dt;
			if (step > d - r * 0.4f)
				step = d - r * 0.4f;
			SmsVec3 want = V3(at.x + dx * step, at.y + 40.0f, at.z + dz * step).s(), out = want;
			if (GOOP_HAS(api->game, SmsGameApi, out_of_walls))
				api->game->out_of_walls(&want, 40.0f, &out, 0);
			place(e, out.x, at.y, out.z);
			api->actor->set_yaw(e, atan2f(dx, dz) * 57.29578f); // facing the fire
			mv->x = out.x, mv->z = out.z;
			mv->heldAt = time, mv->held = true, mv->hx = out.x, mv->hz = out.z;
		}
	}

	// enemies (and characters, and Phantamantas over fire) standing in a puddle
	void hurtEnemies()
	{
		for (int i = 0; i < everyoneCount; i++) {
			const SmsObject e = everyone[i];
			if (api->actor->kind(e) == SMS_KIND_PHANTAMANTA) {
				float rad = 0, tall = 0;
				api->actor->hit_size(e, &rad, &tall);
				const V3 at = posOf(e);
				for (int k = 0; k < kPuddles; k++) {
					const Puddle& p = puddles[k];
					if (!p.on || p.kind != kFire)
						continue;
					const float r = sizeNow(p) + 120.0f, dx = at.x - p.x, dz = at.z - p.z;
					if (dx * dx + dz * dz <= r * r && fabsf(at.y - p.y) < 200.0f) {
						mantaFire(e);
						break;
					}
				}
				continue;
			}
			if (!isSmall(e) && !(set.npcs && isNpc(e)))
				continue;
			const V3 at = posOf(e);
			for (int k = 0; k < kPuddles; k++) {
				const Puddle& p = puddles[k];
				if (!p.on || p.kind == kBlack)
					continue;
				const float r = sizeNow(p) + 20.0f, dx = at.x - p.x, dz = at.z - p.z;
				if (dx * dx + dz * dz <= r * r && fabsf(at.y - p.y) < 60.0f) {
					burn(e, p.kind, true);
					break;
				}
			}
		}
	}

	// ---------------------------------------------------------------- Mario

	bool onBlack(const SmsPlayer& m) const
	{
		if (m.on_yoshi || m.in_water || m.position.y > m.floor.y + 12.0f)
			return false;
		for (int i = 0; i < kPuddles; i++) {
			const Puddle& p = puddles[i];
			if (!p.on || p.kind != kBlack)
				continue;
			const float r = sizeNow(p) * 0.9f, dx = m.position.x - p.x, dz = m.position.z - p.z;
			if (dx * dx + dz * dz <= r * r && fabsf(m.position.y - p.y) < 40.0f)
				return true;
		}
		return false;
	}

	// Mario moved on from where the game left him to (tx, tz), a little at a time, kept out of walls
	// (at his knees and his chest): what stops him takes that way out of the glide
	void glideTo(Player& g, const SmsPlayer& m, float tx, float tz, float dt)
	{
		if (!GOOP_HAS(api->game, SmsGameApi, out_of_walls))
			return;
		float x = m.position.x, z = m.position.z;
		const float ax = tx - x, az = tz - z, along = sqrtf(ax * ax + az * az);
		const int steps = along > 18.0f ? (int)(along / 18.0f) + 1 : 1;
		for (int k = 0; k < steps; k++) {
			const float wx = x + ax / steps, wz = z + az / steps;
			float px = wx, pz = wz;
			bool hit = false;
			static const float kHeights[2] = {30.0f, 80.0f};
			for (int h = 0; h < 2; h++) {
				const SmsVec3 at = V3(px, m.position.y + kHeights[h], pz).s();
				SmsVec3 out, n;
				if (!api->game->out_of_walls(&at, 50.0f, &out, &n))
					continue;
				px = out.x, pz = out.z;
				const float into = g.glideX * n.x + g.glideZ * n.z;
				const float fast = sqrtf(g.glideX * g.glideX + g.glideZ * g.glideZ);
				// head on and faster than he can run: knocked back onto his bottom
				if (into < 0.0f && -into > 0.7f * fast && -into > 34.0f * dt * 30.0f && !(m.status & 0x20000)) {
					const float hl = sqrtf(n.x * n.x + n.z * n.z);
					if (hl > 0.3f) {
						api->player->set_yaw(atan2f(-n.x / hl, -n.z / hl) * 57.29578f); // facing the wall
						api->player->set_status(kStatusJumpBackDown, 0);
						api->player->set_forward_speed(-18.0f);
						SmsVec3 v = m.velocity;
						v.y       = 30.0f;
						api->player->set_velocity(&v);
						api->audio->play_sound_at(kSoundWallHard, &m.position);
						g.glideX = g.glideZ = 0.0f;
						g.gliding           = false;
						const SmsVec3 to    = V3(px, m.position.y, pz).s();
						api->player->set_position(&to);
						return;
					}
				}
				if (into < 0.0f)
					g.glideX -= n.x * into, g.glideZ -= n.z * into;
				hit = true;
			}
			x = px, z = pz;
			if (hit) {
				const float ox = px - wx, oz = pz - wz;
				if (ox * ox + oz * oz > 4.0f)
					break;
			}
		}
		const SmsVec3 to = V3(x, m.position.y, z).s();
		api->player->set_position(&to);
	}

	// black goop under Mario: slippery as ice and fast (his way kept from tick to tick, turning
	// toward the game's own step times the speed only slowly), carried on after a jump off it
	void blackGoop(Player& g, float dt)
	{
		SmsPlayer m;
		if (!mario(&m)) {
			g.havePrev = g.gliding = false;
			return;
		}
		const float dx = m.position.x - g.prevX, dz = m.position.z - g.prevZ;
		const bool sane = g.havePrev && fabsf(dx) < 300.0f && fabsf(dz) < 300.0f;
		const bool air  = m.position.y > m.floor.y + 12.0f && !m.in_water;
		if (sane && !air && onBlack(m)) {
			if (!g.gliding)
				g.glideX = dx, g.glideZ = dz, g.gliding = true;
			const float k = 1.0f - powf(0.5f, dt / 0.45f);
			g.glideX += (dx * set.blackSpeed - g.glideX) * k;
			g.glideZ += (dz * set.blackSpeed - g.glideZ) * k;
		} else if (sane && g.gliding && !m.in_water) {
			float speed     = sqrtf(g.glideX * g.glideX + g.glideZ * g.glideZ);
			const float own = sqrtf(dx * dx + dz * dz);
			float way       = atan2f(g.glideX, g.glideZ);
			const bool stick = m.size >= offsetof(SmsPlayer, intended_speed) + sizeof(float) && m.intended_speed > 0.03f;
			if (stick) {
				const float want = m.intended_yaw * (3.14159265f / 180.0f);
				float diff       = want - way;
				while (diff > 3.14159265f)
					diff -= 6.2831853f;
				while (diff < -3.14159265f)
					diff += 6.2831853f;
				const float push = m.intended_speed;
				const float turn = air ? (set.airTurn >= 0.999f ? 10.0f : 10.0f * set.airTurn * dt * push) : 7.0f * dt * push;
				way += diff > turn ? turn : diff < -turn ? -turn : diff;
				if (fabsf(diff) > 2.2f)
					speed *= powf(0.5f, dt * push / 0.5f);
			}
			speed += (own - speed) * (1.0f - powf(0.5f, dt / (air ? 3.0f : 1.5f)));
			g.glideX = sinf(way) * speed, g.glideZ = cosf(way) * speed;
			if (!air && speed <= own * 1.05f + 0.3f)
				g.gliding = false;
		} else
			g.gliding = false;
		if (g.gliding) {
			glideTo(g, m, g.prevX + g.glideX, g.prevZ + g.glideZ, dt);
			mario(&m);
		}
		g.prevX = m.position.x, g.prevZ = m.position.z;
		g.havePrev = true;
	}

	// Mario standing in a puddle (or touching wall goop): hurt as by the game's own goop
	void hurtMario(int who, float dt)
	{
		SmsPlayer m;
		if (!mario(&m) || m.on_yoshi || m.health <= 0 || m.in_water)
			return;
		const bool blinking = hasInvincible(m) && m.invincible;
		for (int i = 0; i < kWalls && !blinking; i++) {
			const WallGoop& g = walls[i];
			if (!g.on || g.kind == kBlack)
				continue;
			const float dx = m.position.x - g.x, dz = m.position.z - g.z, dy = m.position.y + 80.0f - g.y;
			const float out = dx * g.nx + dz * g.nz, r = wallSizeNow(g);
			if (out < -10.0f || out > 50.0f || dx * dx + dz * dz - out * out > r * r || fabsf(dy) > r + 80.0f)
				continue;
			if (g.kind == kFire)
				fireHurt(m);
			else if (m.status != kStatusElectric)
				shock(m, false);
			return;
		}
		if (m.position.y > m.floor.y + 12.0f) // in the air
			return;
		for (int i = 0; i < kPuddles; i++) {
			Puddle& p = puddles[i];
			if (!p.on)
				continue;
			const float r = sizeNow(p) * 0.85f, dx = m.position.x - p.x, dz = m.position.z - p.z;
			if (dx * dx + dz * dz > r * r || fabsf(m.position.y - p.y) > 40.0f) {
				p.inside[who] = 0.0f;
				continue;
			}
			p.inside[who] += dt / 30.0f;
			if (p.kind == kBlack || blinking)
				continue;
			if (p.kind == kFire) {
				fireHurt(m);
				return;
			}
			const uint32_t type = m.status & 0x1C0;
			if (p.inside[who] >= 0.15f && m.status != kStatusElectric && (type == 0x000 || type == 0x040)) {
				shock(m, true);
				return;
			}
		}
	}
	void fireHurt(const SmsPlayer& m)
	{
		if (!GOOP_HAS(api->player, SmsPlayerApi, burn) || !api->player->burn())
			return;
		// burning: launched up two and a half times his height, a random way
		const float g = m.gravity > 0.05f ? m.gravity : 2.0f;
		SmsVec3 v     = m.velocity;
		v.y           = sqrtf(2.0f * g * 2.5f * 160.0f);
		api->player->set_velocity(&v);
		api->player->set_yaw(random01() * 360.0f);
		api->player->set_forward_speed(12.0f);
	}
	void shock(const SmsPlayer& m, bool lightning)
	{
		api->player->set_status(kStatusElectric, 0); // shocked, a point of health
		api->audio->play_sound_at(kSoundElecDamage, &m.position);
		if (lightning)
			api->audio->play_sound_at(kSoundLightning, &m.position);
	}

	// the water a goop weapon uses: FLUDD's own (what left the tank this tick) times its setting
	void drain(Player& g)
	{
		SmsPlayer m;
		if (!mario(&m) || !m.has_fludd) {
			g.lastWater = -1;
			return;
		}
		int now = m.water;
		if (g.kind && g.lastWater >= 0 && now < g.lastWater) {
			const float more = (float)(g.lastWater - now) * (set.of(g.kind).water - 1.0f) + g.waterCarry;
			const int whole  = (int)more;
			g.waterCarry     = more - (float)whole;
			now -= whole;
			now = now < 0 ? 0 : now > m.max_water ? m.max_water : now;
			api->player->set_water(now);
		}
		g.lastWater = now;
	}

	// ---------------------------------------------------------------- events

	// a player's FLUDD about to spray: with a goop weapon, its stream made as goop
	void onEmit(SmsEvWaterEmit& e, int kind)
	{
		if (e.player < 0 || e.player > 1)
			return;
		Player& g = players[e.player];
		g.kind    = kind;
		if (!kind)
			return;
		e.drop.tag    = tagOf(e.player, kind);
		e.drop.unseen = 1;
		g.emittedAt   = time;
	}

	// a drop landing: goop leaves goop (and is gone); water washes goop away. `wash` the water's
	// washing: 0 none (the bazooka with the rocket jump keys held), 1 a drop's, 2 a strong splash
	int onLand(const SmsEvWaterLand& e, int wash)
	{
		const int kind = kindOfTag(e.tag);
		if (kind) {
			if (e.on && api->actor->kind(e.on) == SMS_KIND_PHANTAMANTA)
				return SMS_CONTINUE; // on a Phantamanta's back: its hit, no goop there
			if (e.wall) {
				if (set.walls)
					addWallGoop(kind, e.position.x, e.position.y, e.position.z, e.normal);
			} else
				addGoop(kind, e.position.x, e.position.y, e.position.z, e.normal);
			return SMS_CANCEL;
		}
		if (!wash || !anyGoop())
			return SMS_CONTINUE;
		const float reach = wash == 2 ? 40.0f : e.drop_size * 0.7f, amount = wash == 2 ? 28.0f : kWash;
		if (e.wall)
			washWallAt(e.position.x, e.position.y, e.position.z, reach, amount);
		else
			washAt(e.position.x, e.position.y, e.position.z, reach, amount);
		return SMS_CONTINUE;
	}

	// a drop about to hit something: fire on a Phantamanta counts three of water's (the game's own
	// hit left out); electric's push by the water taken back after
	int onHit(const SmsEvHit& ev)
	{
		unpushed = 0;
		if (ev.message != 0xF || !ev.object || !ev.sender) // HIT_MESSAGE_SPRAYED_BY_WATER
			return SMS_CONTINUE;
		const int slot = api->water->drop_of(ev.sender);
		SmsWaterDrop d = SmsWaterDrop();
		d.size         = sizeof d;
		if (slot < 0 || !api->water->get(slot, &d))
			return SMS_CONTINUE;
		const int kind = kindOfTag(d.tag);
		if (kind == kFire && api->actor->kind(ev.object) == SMS_KIND_PHANTAMANTA) {
			mantaFire(ev.object);
			return SMS_CANCEL;
		}
		if (kind == kElectric && api->actor->velocity(ev.object, &unpushedV))
			unpushed = ev.object;
		return SMS_CONTINUE;
	}

	// ...after: electric shocks (never pushes), fire throws it up burning; water on an enemy near
	// fire throws it into the fire
	void onHitDone(const SmsEvHitDone& ev)
	{
		if (ev.message != 0xF || !ev.object || !ev.sender)
			return;
		const int slot = api->water->drop_of(ev.sender);
		SmsWaterDrop d = SmsWaterDrop();
		d.size         = sizeof d;
		if (slot < 0 || !api->water->get(slot, &d))
			return;
		const int kind = kindOfTag(d.tag);
		SmsObject e    = ev.object;
		if (!isSmall(e) && !(set.npcs && isNpc(e)))
			e = 0;
		if (e && e == unpushed)
			api->actor->set_velocity(e, &unpushedV);
		unpushed = 0;
		if (e)
			if (Mover* mv = moverOf(e, false))
				mv->hitAt = time;
		if (!kind && e && isSmall(e) && !gone(e)) {
			const V3 at = posOf(e);
			float edge;
			if (nearFire(at.x, at.y, at.z, 700.0f, &edge) && edge > 0.0f)
				launchEnemy(e);
			return;
		}
		if (kind == kElectric)
			burn(e, kind);
		else if (kind == kFire)
			burn(e, kind, true, true);
	}

	void stageReset()
	{
		for (int i = 0; i < kPuddles; i++)
			puddles[i] = Puddle();
		for (int i = 0; i < kWalls; i++)
			walls[i] = WallGoop();
		for (int i = 0; i < kZaps; i++)
			zaps[i] = Zap();
		for (int i = 0; i < kFlights; i++)
			flights[i] = Flight();
		for (int i = 0; i < kMovers; i++)
			movers[i] = Mover();
		for (int p = 0; p < 2; p++)
			players[p].havePrev = players[p].gliding = false, players[p].lastWater = -1;
		mantaCount = 0, mantasAt = -10, everyoneCount = 0;
	}

	// each tick: `kinds` each player's goop weapon (0 none), `held` FLUDD held in their view
	void tick(float dt, const int kinds[2], const bool held[2])
	{
		time += dt / 30.0f;
		const float secs = dt / 30.0f;
		for (int p = 0; p < 2; p++)
			players[p].kind = kinds[p], players[p].held = held[p];
		flightsTick(secs);
		zapsTick();
		dressTick();
		for (int p = 0; p < playerCount(); p++) {
			For f(hf(), p);
			drain(players[p]);
			if (players[p].gliding || anyGoop())
				blackGoop(players[p], secs);
		}
		bool any = false;
		for (int i = 0; i < kPuddles; i++) {
			Puddle& p = puddles[i];
			if (p.on && time - p.born > set.of(p.kind).life)
				p.on = false; // dried away
			any = any || p.on;
		}
		for (int i = 0; i < kWalls; i++) {
			WallGoop& g = walls[i];
			if (g.on && time - g.born > set.wallLife)
				g.on = false;
			any = any || g.on;
		}
		if (!any && !kinds[0] && !kinds[1])
			return;
		// the enemies and characters in the stage (and the Phantamantas: a second's), for their checks
		const SmsVec3 c = V3().s();
		everyoneCount   = api->actor->near(&c, 1.0e7f, SMS_ACTOR_FLAG_SMALL_ENEMY | SMS_ACTOR_FLAG_CHARACTER, everyone, 1000);
		everyoneCount   = everyoneCount < 1000 ? everyoneCount : 1000;
		if (time - mantasAt > 1.0f) {
			mantasAt   = time;
			mantaCount = 0;
			static SmsObject all[1024];
			int n = api->actor->near(&c, 1.0e7f, 0, all, 1024);
			n     = n < 1024 ? n : 1024;
			for (int i = 0; i < n && mantaCount < 24; i++)
				if (api->actor->kind(all[i]) == SMS_KIND_PHANTAMANTA)
					mantas[mantaCount++] = all[i];
		}
		for (int i = 0; i < mantaCount && everyoneCount < 1024; i++)
			if (!gone(mantas[i]))
				everyone[everyoneCount++] = mantas[i];
		watchMovers();
		if (!any)
			return;
		for (int p = 0; p < playerCount(); p++) {
			For f(hf(), p);
			hurtMario(p, dt);
		}
		walkToFire(secs);
		if (time - enemyCheck >= 0.1f) {
			enemyCheck = time;
			hurtEnemies();
		}
	}

	// ---------------------------------------------------------------- pictures

	static const int kTex = 128;
	SmsTexture texFire = 0, texElectric = 0, texBlack = 0;
	bool madeTextures  = false;
	const float kTile  = 300.0f; // world units a picture covers

	static float lattice(int x, int y, int period, uint32_t salt)
	{
		x = ((x % period) + period) % period;
		y = ((y % period) + period) % period;
		uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + salt * 2246822519u;
		h          = (h ^ (h >> 13)) * 1274126177u;
		return (float)((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
	}
	static float vnoise(float x, float y, int period, uint32_t salt)
	{
		const int ix = (int)floorf(x), iy = (int)floorf(y);
		float fx = x - (float)ix, fy = y - (float)iy;
		fx = fx * fx * (3.0f - 2.0f * fx);
		fy = fy * fy * (3.0f - 2.0f * fy);
		const float a = lattice(ix, iy, period, salt), b = lattice(ix + 1, iy, period, salt);
		const float c = lattice(ix, iy + 1, period, salt), d = lattice(ix + 1, iy + 1, period, salt);
		return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
	}
	static float fbm(float u, float v, uint32_t salt)
	{
		float sum = 0.0f, amp = 0.5f;
		int period = 3;
		for (int o = 0; o < 4; o++) {
			sum += amp * vnoise(u * (float)period, v * (float)period, period, salt + (uint32_t)o * 17u);
			amp *= 0.5f;
			period *= 2;
		}
		return sum / 0.9375f;
	}
	static float fbmp(float u, float v, uint32_t salt, int period, int octaves)
	{
		float sum = 0.0f, amp = 0.5f, total = 0.0f;
		for (int o = 0; o < octaves; o++) {
			sum += amp * vnoise(u * (float)period, v * (float)period, period, salt + (uint32_t)o * 17u);
			total += amp;
			amp *= 0.5f;
			period *= 2;
		}
		return sum / total;
	}
	static float smooth(float a, float b, float x)
	{
		float t = (x - a) / (b - a);
		t       = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
		return t * t * (3.0f - 2.0f * t);
	}
	static void store(std::vector<uint8_t>& out, const std::vector<float>& rgb)
	{
		out.resize((size_t)kTex * kTex * 4);
		for (int i = 0; i < kTex * kTex; i++) {
			for (int ch = 0; ch < 3; ch++) {
				const float c   = rgb[(size_t)i * 3 + ch];
				out[i * 4 + ch] = (uint8_t)(c < 0.0f ? 0 : c > 255.0f ? 255 : (int)c);
			}
			out[i * 4 + 3] = 255;
		}
	}

	// electric goop as the game's: smooth teal paint with yellow marbled into it (black goop: black
	// where electric is teal, grey where it is yellow)
	static void makeElectric(std::vector<uint8_t>& out, bool black)
	{
		std::vector<float> rgb((size_t)kTex * kTex * 3);
		for (int y = 0; y < kTex; y++)
			for (int x = 0; x < kTex; x++) {
				const float u = (float)x / kTex, v = (float)y / kTex;
				const float qx = fbmp(u, v, 11u, 2, 3), qy = fbmp(u + 0.37f, v + 0.11f, 12u, 2, 3);
				const float rx = fbmp(u + 1.2f * qx, v + 1.2f * qy, 13u, 2, 4), ry = fbmp(u + 1.2f * qx + 0.2f, v + 1.2f * qy + 0.6f, 14u, 2, 4);
				const float f      = fbmp(u + rx, v + ry, 15u, 2, 2);
				const float streak = smooth(0.90f, 0.94f, 0.5f + 0.5f * sinf(f * 30.0f));
				const float blob   = smooth(0.63f, 0.66f, fbmp(u + 0.6f * rx, v + 0.6f * ry, 16u, 2, 3));
				const float m      = streak > blob ? streak : blob;
				const float g      = fbmp(u + 0.3f * rx, v, 17u, 2, 3);
				const float tr = black ? 10.0f + 16.0f * g : 25.0f + 20.0f * g, tg = black ? 10.0f + 15.0f * g : 175.0f + 30.0f * g,
				            tb = black ? 12.0f + 18.0f * g : 175.0f + 25.0f * g;
				const float sr = black ? 140.0f : 245.0f, sg = black ? 140.0f : 235.0f, sb = black ? 145.0f : 30.0f;
				const int i = y * kTex + x;
				rgb[i * 3]     = tr + (sr - tr) * m;
				rgb[i * 3 + 1] = tg + (sg - tg) * m;
				rgb[i * 3 + 2] = tb + (sb - tb) * m;
			}
		store(out, rgb);
	}

	// fire goop: orange with darker red swirls, glossy (a light's shine on its ripples)
	static void makeFire(std::vector<uint8_t>& out)
	{
		std::vector<float> h((size_t)kTex * kTex), mix((size_t)kTex * kTex), rgb((size_t)kTex * kTex * 3);
		for (int y = 0; y < kTex; y++)
			for (int x = 0; x < kTex; x++) {
				const float u = (float)x / kTex, v = (float)y / kTex;
				const float qx = fbm(u, v, 31u), qy = fbm(u + 0.37f, v + 0.11f, 32u);
				const float rx = fbm(u + 0.9f * qx, v + 0.9f * qy, 33u);
				const float ry = fbm(u + 0.9f * qx + 0.2f, v + 0.9f * qy + 0.6f, 34u);
				const float f  = fbm(u + 0.8f * rx, v + 0.8f * ry, 35u);
				const float band = 0.5f + 0.5f * sinf(f * 12.0f + qx * 3.0f);
				mix[y * kTex + x] = smooth(0.62f, 0.9f, band);
				h[y * kTex + x]   = f + 0.08f * mix[y * kTex + x];
			}
		const float lx = -0.45f, ly = -0.55f, lz = 0.70f;
		const float ll = sqrtf(lx * lx + ly * ly + lz * lz);
		for (int y = 0; y < kTex; y++)
			for (int x = 0; x < kTex; x++) {
				const int i = y * kTex + x;
				const float dx = h[y * kTex + ((x + 1) & (kTex - 1))] - h[y * kTex + ((x + kTex - 1) & (kTex - 1))];
				const float dy = h[((y + 1) & (kTex - 1)) * kTex + x] - h[((y + kTex - 1) & (kTex - 1)) * kTex + x];
				float nx = -dx * 4.5f, ny = -dy * 4.5f, nz = 1.0f;
				const float nl = sqrtf(nx * nx + ny * ny + nz * nz);
				nx /= nl, ny /= nl, nz /= nl;
				const float diffuse = nx * lx / ll + ny * ly / ll + nz * lz / ll;
				const float hx = lx / ll, hy = ly / ll, hz = lz / ll + 1.0f;
				const float hl = sqrtf(hx * hx + hy * hy + hz * hz);
				float sp       = (nx * hx + ny * hy + nz * hz) / hl;
				sp             = sp > 0.0f ? powf(sp, 50.0f) : 0.0f;
				const float m = mix[i], g = fbm((float)x / kTex + 0.5f, (float)y / kTex, 77u);
				const float lite = smooth(0.55f, 0.8f, g);
				const float or_ = 255.0f, og = 120.0f + 40.0f * lite, ob = 20.0f + 20.0f * lite;
				const float r = or_ + (205.0f - or_) * m, gr = og + (55.0f - og) * m, b = ob + (12.0f - ob) * m;
				const float shade = 0.8f + 0.25f * diffuse, shine = sp * 120.0f;
				rgb[i * 3]     = r * shade + shine;
				rgb[i * 3 + 1] = gr * shade + shine;
				rgb[i * 3 + 2] = b * shade + shine;
			}
		store(out, rgb);
	}

	bool hasPictures() const { return GOOP_HAS(api->models, SmsModelApi, draw_textured); }
	void makeTextures()
	{
		if (madeTextures || !hasPictures())
			return;
		madeTextures = true;
		std::vector<uint8_t> px;
		makeElectric(px, false);
		texElectric = api->models->texture_create(mod, px.data(), kTex, kTex);
		makeElectric(px, true);
		texBlack = api->models->texture_create(mod, px.data(), kTex, kTex);
		makeFire(px);
		texFire = api->models->texture_create(mod, px.data(), kTex, kTex);
	}
	SmsTexture texOf(int kind) const { return kind == kFire ? texFire : kind == kBlack ? texBlack : texElectric; }

	// each player's FLUDD in their goop weapon's look (its own again otherwise)
	int dressed[2] = {0, 0};
	void dressTick()
	{
		const SmsHeldFluddApi* h = hf();
		if (!GOOP_HAS(h, SmsHeldFluddApi, dress))
			return;
		for (int p = 0; p < 2; p++) {
			const int k = p < playerCount() ? players[p].kind : 0;
			if (k == dressed[p])
				continue;
			if (k)
				makeTextures();
			h->dress(p, k ? texOf(k) : 0);
			dressed[p] = k;
		}
	}

	// ---------------------------------------------------------------- drawing (SMS_EV_DRAW_SCENE)

	std::vector<SmsTexCorner> buf;
	float view[3][4];
	V3 eye;
	int viewer = 0;

	void tc(float x, float y, float z, int shade, float u, float v, int a = 255)
	{
		SmsTexCorner c;
		c.x = x, c.y = y, c.z = z, c.u = u, c.v = v;
		const uint8_t s = (uint8_t)(shade < 0 ? 0 : shade > 255 ? 255 : shade);
		c.r = c.g = c.b = s;
		c.a = (uint8_t)(a < 0 ? 0 : a > 255 ? 255 : a);
		buf.push_back(c);
	}
	void ac(float x, float y, float z, int r, int g, int b, int a)
	{
		SmsTexCorner c;
		c.x = x, c.y = y, c.z = z, c.u = c.v = 0.0f;
		c.r = (uint8_t)r, c.g = (uint8_t)g, c.b = (uint8_t)b;
		c.a = (uint8_t)(a < 0 ? 0 : a > 255 ? 255 : a);
		buf.push_back(c);
	}
	void flush(SmsTexture t, int blend)
	{
		if (buf.size() >= 3)
			api->models->draw_textured(buf.data(), (int)buf.size(), t, blend);
		buf.clear();
	}
	float flow(int kind) const { return (float)frame / 60.0f * (kind == kFire ? 10.0f : 5.0f); }

	struct Drop {
		V3 p, v;
		float size, alive;
		int kind;
	};
	std::vector<Drop> drops;

	// the drops of goop flying now (water->get), each player's
	void gatherDrops()
	{
		drops.clear();
		const int n = api->water->count();
		for (int i = 0; i < n; i++) {
			SmsWaterDrop d = SmsWaterDrop();
			d.size         = sizeof d;
			if (!api->water->get(i, &d) || d.landed || d.alive <= 0)
				continue;
			const int k = kindOfTag(d.tag);
			if (!k)
				continue;
			Drop dr;
			dr.p = V3(d.position), dr.v = V3(d.velocity), dr.size = d.drop_size, dr.alive = (float)d.alive, dr.kind = k;
			drops.push_back(dr);
		}
	}

	// how thick the stream is at a point: a thinner stream held in view, thinner still near the eye
	float thick(float size, const V3& o, bool held) const
	{
		float r = size * 0.55f;
		if (held) {
			const float ex = o.x - eye.x, ey = o.y - eye.y, ez = o.z - eye.z;
			const float d  = sqrtf(ex * ex + ey * ey + ez * ez) / 500.0f;
			r *= 0.6f * (d < 0.5f ? 0.5f : d > 1.0f ? 1.0f : d);
		}
		return r;
	}

	// the stream's drops of a kind: round blobs a little longer along their flight, lit from the eye,
	// joined each to the one let out just before it by a tube (one unbroken stream)
	void drawStream(int kind)
	{
		const bool held = players[viewer & 1].held;
		const float vx = view[2][0], vy = view[2][1], vz = view[2][2]; // toward the viewer
		const float fl0 = flow(kind);
		const int kAround = 8, kBands = 4;
		static float ring[kBands + 1][2], ca[kAround], sa[kAround];
		static bool trig = false;
		if (!trig) {
			for (int b = 0; b <= kBands; b++)
				ring[b][0] = cosf(3.14159265f * (float)b / kBands), ring[b][1] = sinf(3.14159265f * (float)b / kBands);
			for (int k = 0; k < kAround; k++)
				ca[k] = cosf(6.2831853f * k / kAround), sa[k] = sinf(6.2831853f * k / kAround);
			trig = true;
		}
		std::vector<int> order;
		for (int i = 0; i < (int)drops.size(); i++)
			if (drops[i].kind == kind)
				order.push_back(i);
		if (order.empty())
			return;
		for (size_t oi = 0; oi < order.size(); oi++) {
			const Drop& d = drops[order[oi]];
			const float r = thick(d.size, d.p, held), len = r * 1.3f;
			float fx = d.v.x, fy = d.v.y, fz = d.v.z, fl = sqrtf(fx * fx + fy * fy + fz * fz);
			if (fl < 1e-3f)
				fx = 0.0f, fy = 1.0f, fz = 0.0f, fl = 1.0f;
			fx /= fl, fy /= fl, fz /= fl;
			float ax = -fz, az = fx, al = sqrtf(ax * ax + az * az);
			if (al < 1e-3f)
				ax = 1.0f, az = 0.0f, al = 1.0f;
			ax /= al, az /= al;
			const float bx = fy * az, by = fz * ax - fx * az, bz = -fy * ax;
			const float u0 = (float)(order[oi] % 7) * 0.13f + fl0 / kTile, v0 = (float)(order[oi] % 5) * 0.17f;
			float pts[kBands + 1][kAround][3], uv[kBands + 1][kAround][2];
			int shade[kBands + 1][kAround];
			for (int b = 0; b <= kBands; b++)
				for (int k = 0; k < kAround; k++) {
					const float along = ring[b][0] * len, across = ring[b][1] * r;
					const float nx = ax * ca[k] + bx * sa[k], ny = by * sa[k], nz = az * ca[k] + bz * sa[k];
					pts[b][k][0] = d.p.x + fx * along + nx * across;
					pts[b][k][1] = d.p.y + fy * along + ny * across;
					pts[b][k][2] = d.p.z + fz * along + nz * across;
					const float ox = nx * ring[b][1] + fx * ring[b][0], oy = ny * ring[b][1] + fy * ring[b][0], oz = nz * ring[b][1] + fz * ring[b][0];
					const float facing = ox * vx + oy * vy + oz * vz;
					shade[b][k] = (int)(150.0f + 105.0f * (facing > 0.0f ? facing : 0.0f));
					uv[b][k][0] = u0 + (float)k / kAround * 0.35f;
					uv[b][k][1] = v0 + (float)b / kBands * 0.25f;
				}
			for (int b = 0; b < kBands; b++)
				for (int k = 0; k < kAround; k++) {
					const int m = (k + 1) % kAround;
					if (b > 0) {
						tc(pts[b][k][0], pts[b][k][1], pts[b][k][2], shade[b][k], uv[b][k][0], uv[b][k][1]);
						tc(pts[b][m][0], pts[b][m][1], pts[b][m][2], shade[b][m], uv[b][m][0], uv[b][m][1]);
						tc(pts[b + 1][k][0], pts[b + 1][k][1], pts[b + 1][k][2], shade[b + 1][k], uv[b + 1][k][0], uv[b + 1][k][1]);
					}
					if (b < kBands - 1) {
						tc(pts[b][m][0], pts[b][m][1], pts[b][m][2], shade[b][m], uv[b][m][0], uv[b][m][1]);
						tc(pts[b + 1][m][0], pts[b + 1][m][1], pts[b + 1][m][2], shade[b + 1][m], uv[b + 1][m][0], uv[b + 1][m][1]);
						tc(pts[b + 1][k][0], pts[b + 1][k][1], pts[b + 1][k][2], shade[b + 1][k], uv[b + 1][k][0], uv[b + 1][k][1]);
					}
				}
		}
		// the tubes: newest first (the most life left), each joined to the next one let out
		struct ByLife {
			const std::vector<Drop>* d;
			bool operator()(int a, int b) const { return (*d)[a].alive > (*d)[b].alive; }
		};
		ByLife cmp;
		cmp.d = &drops;
		std::sort(order.begin(), order.end(), cmp);
		struct Piece {
			V3 a, b;
			float ra, rb;
			int id;
		};
		std::vector<Piece> pieces;
		// held in view and spraying: the stream starts right at the nozzle (joined to the newest drop)
		float tip[3][4];
		if (held && time - players[viewer & 1].emittedAt < 0.15 && GOOP_HAS(api->models, SmsModelApi, frame)
		    && api->models->frame(SMS_FRAME_HELD, viewer & 1, tip)) {
			const Drop& n = drops[order[0]];
			const V3 nz(tip[0][3], tip[1][3], tip[2][3]);
			const float dx = n.p.x - nz.x, dy = n.p.y - nz.y, dz = n.p.z - nz.z;
			if (dx * dx + dy * dy + dz * dz < 900.0f * 900.0f) {
				Piece pc;
				pc.a = nz, pc.b = n.p, pc.rb = thick(n.size, n.p, held) * 0.85f, pc.ra = pc.rb * 0.7f, pc.id = order[0];
				pieces.push_back(pc);
			}
		}
		for (size_t k = 0; k + 1 < order.size(); k++) {
			const Drop &a = drops[order[k]], &b = drops[order[k + 1]];
			if (a.alive - b.alive > 3.5f)
				continue; // not let out one after the other
			const float dx = b.p.x - a.p.x, dy = b.p.y - a.p.y, dz = b.p.z - a.p.z;
			const float gap2 = 4.0f * (a.v.x * a.v.x + a.v.y * a.v.y + a.v.z * a.v.z) + 900.0f;
			if (dx * dx + dy * dy + dz * dz > gap2)
				continue; // too far apart: not the same stream
			Piece pc;
			pc.a = a.p, pc.b = b.p, pc.ra = thick(a.size, a.p, held) * 0.85f, pc.rb = thick(b.size, b.p, held) * 0.85f, pc.id = order[k];
			pieces.push_back(pc);
		}
		const int kSides = 8;
		for (size_t t = 0; t < pieces.size(); t++) {
			const Piece& pc = pieces[t];
			float fx = pc.b.x - pc.a.x, fy = pc.b.y - pc.a.y, fz = pc.b.z - pc.a.z;
			const float fl = sqrtf(fx * fx + fy * fy + fz * fz) + 1e-4f;
			fx /= fl, fy /= fl, fz /= fl;
			float ax = -fz, az = fx, al = sqrtf(ax * ax + az * az);
			if (al < 1e-3f)
				ax = 1.0f, az = 0.0f, al = 1.0f;
			ax /= al, az /= al;
			const float bx = fy * az, by = fz * ax - fx * az, bz = -fy * ax;
			const float u0 = (float)(pc.id % 7) * 0.13f + fl0 / kTile;
			for (int q = 0; q < kSides; q++) {
				const int sides[4] = {q, (q + 1) % kSides, (q + 1) % kSides, q};
				float pt[4][3];
				int sh[4];
				for (int c = 0; c < 4; c++) {
					const float an = 6.2831853f * (float)sides[c] / kSides, cs = cosf(an), sn = sinf(an);
					const float nx = ax * cs + bx * sn, ny = by * sn, nz = az * cs + bz * sn;
					const V3& o    = c < 2 ? pc.a : pc.b;
					const float rr = c < 2 ? pc.ra : pc.rb;
					pt[c][0] = o.x + nx * rr, pt[c][1] = o.y + ny * rr, pt[c][2] = o.z + nz * rr;
					const float facing = nx * vx + ny * vy + nz * vz;
					sh[c] = (int)(150.0f + 105.0f * (facing > 0.0f ? facing : 0.0f));
				}
				const float us[4] = {u0 + (float)q / kSides * 0.35f, u0 + (float)(q + 1) / kSides * 0.35f, u0 + (float)(q + 1) / kSides * 0.35f,
				                     u0 + (float)q / kSides * 0.35f};
				const float vs[4] = {0.0f, 0.0f, 0.2f, 0.2f};
				const int tri[6]  = {0, 1, 2, 0, 2, 3};
				for (int c = 0; c < 6; c++)
					tc(pt[tri[c]][0], pt[tri[c]][1], pt[tri[c]][2], sh[tri[c]], us[tri[c]], vs[tri[c]]);
			}
		}
	}

	static void basis(const Puddle& p, float* t, float* b)
	{
		float tx = p.ny, ty = -p.nx, tz = 0.0f, tl = sqrtf(tx * tx + ty * ty);
		if (tl < 1e-3f)
			tx = 1.0f, ty = 0.0f, tl = 1.0f;
		tx /= tl, ty /= tl;
		t[0] = tx, t[1] = ty, t[2] = tz;
		b[0] = p.ny * tz - p.nz * ty, b[1] = p.nz * tx - p.nx * tz, b[2] = p.nx * ty - p.ny * tx;
	}
	// its wavy edge at angle a: broad lobes and small bumps, as the game's own goop's
	static float edgeAt(const Puddle& p, float a, float r)
	{
		return r * (1.0f + 0.11f * sinf(3.0f * a + p.seed) + 0.06f * sinf(5.0f * a + p.seed * 1.7f) + 0.035f * sinf(9.0f * a + p.seed * 2.3f)
		            + 0.022f * sinf(13.0f * a + p.seed * 3.1f) + 0.014f * sinf(17.0f * a + p.seed * 4.3f));
	}
	static float thickOf(float r) { return r < 60.0f ? r * 0.06f : 3.6f; }
	static float rimOf(float r) { return r < 60.0f ? r * 0.12f : 7.0f; }

	// (x, y, z) well inside another puddle of the same kind: no rim there (overlapping puddles look one)
	bool underOther(int self, int kind, float x, float y, float z) const
	{
		for (int i = 0; i < kPuddles; i++) {
			const Puddle& o = puddles[i];
			if (i == self || !o.on || o.kind != kind)
				continue;
			const float dx = x - o.x, dy = y - o.y, dz = z - o.z;
			if (dy > 80.0f || dy < -80.0f)
				continue;
			const float r = sizeNow(o);
			if (r < 1.0f)
				continue;
			const float d2 = dx * dx + dz * dz;
			if (d2 > r * r * 1.5f)
				continue;
			const float sure = r * 0.7f - rimOf(r) * 1.5f;
			if (sure > 0.0f && d2 < sure * sure)
				return true;
			float t[3], b[3];
			basis(o, t, b);
			const float a  = atan2f(dx * b[0] + dy * b[1] + dz * b[2], dx * t[0] + dy * t[1] + dz * t[2]);
			const float in = edgeAt(o, a, r) - rimOf(r) * 1.5f;
			if (in > 0.0f && d2 < in * in)
				return true;
		}
		return false;
	}

	// a puddle: a round of the goop's picture with a wavy edge, lying a little thick on the ground with
	// a rounded, darker edge, joining other puddles of its kind where they overlap
	bool joined[kPuddles][56];
	float joinedBorn[kPuddles], joinedSize[kPuddles];
	void drawPuddle(const Puddle& p, int index)
	{
		const float r = sizeNow(p);
		if (r < 1.0f)
			return;
		const float vz = view[2][0] * p.x + view[2][1] * p.y + view[2][2] * p.z + view[2][3];
		if (vz > r * 1.4f + 20.0f)
			return; // behind the view
		// far off, fewer corners (very high: twice as far before; low: every other, very low: every fourth)
		const float away = -vz - r * 1.4f, far = set.quality >= 4 ? 2.0f : set.quality == 2 ? 0.6f : 1.0f;
		int every        = away > 6000.0f * far ? 4 : away > 2500.0f * far ? 2 : 1;
		every *= set.quality <= 0 ? 4 : set.quality == 1 ? 2 : 1;
		const int kAround = 56, kRings = 7;
		static float cs[kAround], sn[kAround];
		static bool trig = false;
		if (!trig) {
			for (int k = 0; k < kAround; k++)
				cs[k] = cosf(6.2831853f * k / kAround), sn[k] = sinf(6.2831853f * k / kAround);
			trig = true;
		}
		float t[3], b[3];
		basis(p, t, b);
		const float lift = 1.0f + 0.04f * (float)index;
		const float cx = p.x + p.nx * lift, cy = p.y + p.ny * lift, cz = p.z + p.nz * lift;
		const float tm    = (float)frame / 60.0f;
		const int shadeTop = p.kind == kFire ? (int)(255.0f * (0.93f + 0.07f * sinf(tm * 9.0f + p.seed))) : 255;
		const float fl     = p.kind == kFire ? flow(p.kind) : 0.0f;
		const float thick = thickOf(r), rim = rimOf(r);
		static const float inRim[kRings + 1] = {0, 0, 0, 0, 0, 1.0f, 0.45f, 0.0f};
		static const float outF[kRings + 1]  = {0.0f, 0.3f, 0.55f, 0.75f, 0.9f, 1.0f, 1.0f, 1.0f};
		static const float high[kRings + 1]  = {1, 1, 1, 1, 1, 1.0f, 0.8f, 0.0f};
		static const float lit[kRings + 1]   = {1, 1, 1, 1, 1, 0.97f, 0.78f, 0.5f};
		float rr[kAround];
		for (int k = 0; k < kAround; k++)
			rr[k] = edgeAt(p, 6.2831853f * (float)k / kAround, r);
		const int redo = set.quality >= 4 ? 4 : set.quality >= 3 ? 8 : 16;
		if (joinedBorn[index] != p.born || fabsf(joinedSize[index] - r) > 6.0f || (frame + index) % redo == 0) {
			joinedBorn[index] = p.born, joinedSize[index] = r;
			for (int k = 0; k < kAround; k++) {
				const float ox = cs[k] * rr[k], oz = sn[k] * rr[k];
				joined[index][k] = underOther(index, p.kind, cx + t[0] * ox + b[0] * oz, cy + t[1] * ox + b[1] * oz, cz + t[2] * ox + b[2] * oz);
			}
		}
		static float pos[kRings + 1][kAround][3], uv[kRings + 1][kAround][2];
		static int col[kRings + 1][kAround];
		for (int k = 0; k < kAround; k += every) {
			const bool jn   = joined[index][k] || joined[index][(k + every) % kAround] || joined[index][(k + kAround - every) % kAround];
			const float top = rr[k] - rim > rr[k] * 0.4f ? rr[k] - rim : rr[k] * 0.4f;
			for (int j = 0; j <= kRings; j++) {
				float d  = j <= 4 ? top * outF[j] : top + (rr[k] - top) * (1.0f - inRim[j]);
				float h  = thick * high[j];
				int lt   = (int)(255.0f * lit[j]);
				if (jn && j > 4)
					h = thick, lt = 255, d = rr[k] * 1.04f;
				const float ca = cs[k] * d, sa = sn[k] * d;
				const float x = cx + t[0] * ca + b[0] * sa + p.nx * h, y = cy + t[1] * ca + b[1] * sa + p.ny * h, z = cz + t[2] * ca + b[2] * sa + p.nz * h;
				pos[j][k][0] = x, pos[j][k][1] = y, pos[j][k][2] = z;
				float du = 0.0f, dv = 0.0f;
				if (set.quality >= 2) { // the water's ripples over it
					du = 3.5f * sinf(x * 0.045f + tm * 1.6f) + 2.5f * sinf(z * 0.06f - tm * 1.2f);
					dv = 3.5f * sinf(z * 0.05f + tm * 1.4f) + 2.5f * sinf(x * 0.07f + tm * 1.9f);
				}
				uv[j][k][0] = (x + fl + du) / kTile, uv[j][k][1] = (z + dv) / kTile;
				col[j][k]   = shadeTop * lt / 255;
			}
		}
		const int n = kAround / every;
		for (int t2 = 0; t2 < kRings * n; t2++) {
			const int j = t2 / n, k = (t2 % n) * every, m = (k + every) % kAround;
			const int c6[6][2] = {{j, k}, {j + 1, k}, {j + 1, m}, {j, k}, {j + 1, m}, {j, m}};
			const int corners  = j == 0 ? 3 : 6;
			for (int v = 0; v < corners; v++) {
				const int jj = c6[v][0], q = c6[v][1];
				tc(pos[jj][q][0], pos[jj][q][1], pos[jj][q][2], col[jj][q], uv[jj][q][0], uv[jj][q][1]);
			}
		}
	}

	static float hash01(uint32_t h)
	{
		h = (h ^ 61u) ^ (h >> 16);
		h *= 9u;
		h = h ^ (h >> 4);
		h *= 0x27d4eb2du;
		h = h ^ (h >> 15);
		return (float)(h & 0xFFFF) / 65535.0f;
	}

	// goop on walls: a blob with drips running down from it, growing longer
	void drawWalls(int kind)
	{
		const float tm = (float)frame / 60.0f;
		for (int i = 0; i < kWalls; i++) {
			const WallGoop& g = walls[i];
			if (!g.on || g.kind != kind)
				continue;
			const float r = wallSizeNow(g);
			if (r < 1.0f)
				continue;
			const float tx = -g.nz, tz = g.nx;
			const float lift = 1.2f + 0.03f * (float)i;
			const float cx = g.x + g.nx * lift, cy = g.y, cz = g.z + g.nz * lift;
			const int kAround = 20;
			for (int k = 0; k < kAround; k++) { // (a fan, as triangles)
				float p2[2][3];
				for (int e = 0; e < 2; e++) {
					const float a  = 6.2831853f * (float)((k + e) % kAround) / kAround;
					const float rr = r * (1.0f + 0.14f * sinf(3.0f * a + g.seed) + 0.08f * sinf(5.0f * a + g.seed * 1.3f));
					const float ox = cosf(a) * rr, oy = sinf(a) * rr;
					p2[e][0] = cx + tx * ox, p2[e][1] = cy + oy, p2[e][2] = cz + tz * ox;
				}
				tc(cx, cy, cz, 255, (cx * tx + cz * tz) / kTile, -cy / kTile);
				tc(p2[0][0], p2[0][1], p2[0][2], 235, (p2[0][0] * tx + p2[0][2] * tz) / kTile, -p2[0][1] / kTile);
				tc(p2[1][0], p2[1][1], p2[1][2], 235, (p2[1][0] * tx + p2[1][2] * tz) / kTile, -p2[1][1] / kTile);
			}
			const int drips     = 3 + (int)(r / 25.0f);
			const float running = (time - g.since) / 5.0f;
			const float most    = (r * 1.4f + 40.0f) * (running < 1.0f ? running : 1.0f);
			const int kBulb     = 12;
			for (int d = 0; d < drips; d++) {
				const uint32_t key = (uint32_t)i * 131u + (uint32_t)d * 977u + (uint32_t)(g.seed * 10.0f);
				const float off  = (hash01(key) * 2.0f - 1.0f) * r * 0.65f;
				const float wide = r * (0.08f + 0.08f * hash01(key + 1u));
				const float len  = most * (0.35f + 0.65f * hash01(key + 2u)) * (0.97f + 0.03f * sinf(tm * 2.0f + (float)d));
				const float top = cy - r * 0.2f, bottom = cy - r * 0.55f - len;
				const float bx = cx + tx * off, bz = cz + tz * off;
				const float u0 = ((bx - tx * wide) * tx + (bz - tz * wide) * tz) / kTile, u1 = ((bx + tx * wide) * tx + (bz + tz * wide) * tz) / kTile;
				tc(bx - tx * wide, top, bz - tz * wide, 245, u0, -top / kTile);
				tc(bx + tx * wide, top, bz + tz * wide, 245, u1, -top / kTile);
				tc(bx + tx * wide, bottom, bz + tz * wide, 225, u1, -bottom / kTile);
				tc(bx - tx * wide, top, bz - tz * wide, 245, u0, -top / kTile);
				tc(bx + tx * wide, bottom, bz + tz * wide, 225, u1, -bottom / kTile);
				tc(bx - tx * wide, bottom, bz - tz * wide, 225, u0, -bottom / kTile);
				const float br = wide * 1.25f, by = bottom - br * 0.35f; // its end: a round drop hanging
				for (int k = 0; k < kBulb; k++) {
					const float a0 = 6.2831853f * (float)k / kBulb, a1 = 6.2831853f * (float)(k + 1) / kBulb;
					const float x0 = cosf(a0) * br, y0 = sinf(a0) * br * 1.15f, x1 = cosf(a1) * br, y1 = sinf(a1) * br * 1.15f;
					tc(bx, by, bz, 225, (bx * tx + bz * tz) / kTile, -by / kTile);
					tc(bx + tx * x0, by + y0, bz + tz * x0, 215, ((bx + tx * x0) * tx + (bz + tz * x0) * tz) / kTile, -(by + y0) / kTile);
					tc(bx + tx * x1, by + y1, bz + tz * x1, 215, ((bx + tx * x1) * tx + (bz + tz * x1) * tz) / kTile, -(by + y1) / kTile);
				}
			}
		}
	}

	// a flame: a soft teardrop `h` high now (`tall` at most), turned to face the view
	void flame(float x, float y, float z, float tall, float h, float sway)
	{
		const float rx = view[0][0], rz = view[0][2]; // the view's right
		const float w  = tall * 0.32f;
		const float pts[7][2] = {{0.0f, 0.0f}, {-w, 0.3f * h}, {-0.5f * w, 0.7f * h}, {sway, 1.05f * h}, {0.5f * w, 0.7f * h}, {w, 0.3f * h}, {0.0f, 0.0f}};
		const int al[7] = {120, 110, 70, 0, 70, 110, 120};
		const float my  = 0.35f * h;
		for (int k = 0; k < 6; k++) {
			ac(x, y + my, z, 255, 150, 70, 210);
			ac(x + rx * pts[k][0], y + pts[k][1], z + rz * pts[k][0], 255, 80, 60, al[k]);
			ac(x + rx * pts[k + 1][0], y + pts[k + 1][1], z + rz * pts[k + 1][0], 255, 80, 60, al[k + 1]);
		}
	}

	void drawFlames(const Puddle& p, int index)
	{
		const float r = sizeNow(p);
		if (r < 10.0f)
			return;
		float t[3], b[3];
		basis(p, t, b);
		static const float kPer[5] = {150.0f, 90.0f, 60.0f, 45.0f, 35.0f}; // (a flame for so much of its size)
		const int q     = set.quality < 0 ? 0 : set.quality > 4 ? 4 : set.quality;
		const int count = (q >= 2 ? 2 : 1) + (int)(r / kPer[q]) + (q >= 4 ? 1 : 0);
		const float tm  = (float)frame / 60.0f;
		for (int i = 0; i < count; i++) {
			const float phase = tm * 1.3f + (float)i * 0.618f + p.seed;
			const int cycle   = (int)floorf(phase);
			const float life  = phase - (float)cycle;
			const uint32_t key = (uint32_t)index * 977u + (uint32_t)i * 131u + (uint32_t)cycle * 7919u;
			const float a = hash01(key) * 6.2831853f, d = r * 0.8f * sqrtf(hash01(key + 1u));
			const float ox = cosf(a) * d, oz = sinf(a) * d;
			const float x = p.x + t[0] * ox + b[0] * oz, y = p.y + t[1] * ox + b[1] * oz, z = p.z + t[2] * ox + b[2] * oz;
			const float tall = (45.0f + r * 0.12f) * (0.7f + 0.6f * hash01(key + 2u));
			flame(x, y, z, tall, tall * sinf(3.14159f * life), 0.12f * tall * sinf(tm * 6.0f + (float)i * 2.1f));
		}
	}

	// SMS_EV_DRAW_SCENE: everything of the goop's in this view
	void draw(const SmsEvDrawScene& e)
	{
		const bool anyStream = players[0].kind || players[1].kind;
		if (!anyGoop() && !anyStream && drops.empty())
			return;
		if (!hasPictures())
			return;
		makeTextures();
		if (buf.capacity() < 200000)
			buf.reserve(200000);
		memcpy(view, e.view_mtx, sizeof view);
		eye    = V3(e.eye);
		viewer = e.view;
		if (e.view == 0)
			frame++;
		gatherDrops();
		for (int kind = kFire; kind <= kBlack; kind++) {
			for (int i = 0; i < kPuddles; i++)
				if (puddles[i].on && puddles[i].kind == kind)
					drawPuddle(puddles[i], i);
			drawWalls(kind);
			drawStream(kind);
			flush(texOf(kind), SMS_BLEND_SOLID);
		}
		// the flames last: soft, added to what is behind
		for (int i = 0; i < kPuddles; i++)
			if (puddles[i].on && puddles[i].kind == kFire)
				drawFlames(puddles[i], i);
		const float tm = (float)frame / 60.0f;
		for (int i = 0; i < kWalls; i++) { // fire goop on walls: a flame or two off it
			const WallGoop& g = walls[i];
			const float r     = g.on && g.kind == kFire ? wallSizeNow(g) : 0.0f;
			if (r < 8.0f)
				continue;
			for (int k = 0; k < 2; k++) {
				const float tall = 30.0f + r * 0.3f;
				flame(g.x + g.nx * 8.0f - g.nz * (k ? 0.3f : -0.3f) * r, g.y - r * 0.2f, g.z + g.nz * 8.0f + g.nx * (k ? 0.3f : -0.3f) * r, tall,
				      tall * (0.6f + 0.4f * sinf(tm * 8.0f + g.seed + (float)k * 2.0f)), 0.1f * tall * sinf(tm * 5.0f + (float)k));
			}
		}
		flush(0, SMS_BLEND_ADD);
	}
};

} // namespace goop
