// The weapons' shots, hits and bursts: made of the game's water drops through the mod API's general
// tools (api->water, api->actor, game->ray/particle/splash/clean_goop, audio->play_sound_at,
// held_fludd->aim/kick). Included by fludd_weapons.cpp (FluddWeapons runs it each tick).
#pragma once
#include <sms_mod.hpp>
#include <math.h>
#include <string.h>
#include <stddef.h>

namespace shots {

// the weapons (as the menu's ids: SMS_SPRAY_*'s numbers)
enum { NORMAL = 0, RIFLE = 2, BAZOOKA = 3, SMG = 4, WEAPONS = 5 };
const char* const kNames[WEAPONS] = {"FLUDD", "shotgun", "rifle", "bazooka", "submachine gun"};

// the game's numbers this needs (its sounds, particles, messages)
const uint32_t kSoundNozzle    = 0x0024; // MSD_SE_PO_NORMAL_NOZZLE_IMI: the spray's own sound
const uint32_t kSoundExplosion = 0x081E; // MSD_SE_PO_WFLOWER_EXPLOSION
const uint32_t kParticleHit    = 0xE7;   // PARTICLE_MS_ENM_WATHIT: water hitting an enemy
const uint32_t kSprayed        = 0xF;    // HIT_MESSAGE_SPRAYED_BY_WATER

// a drop's tag: this mod's (its top byte), the player, the weapon, the shot's number
const uint32_t kMine = 0x5A000000u;
inline uint32_t tagOf(int player, int weapon, uint32_t shot) { return kMine | ((uint32_t)(player & 1) << 23) | ((uint32_t)(weapon & 7) << 20) | (shot & 0xFFFFFu); }
inline bool mine(uint32_t tag) { return (tag & 0xFF000000u) == kMine; }
inline int tagPlayer(uint32_t tag) { return (int)((tag >> 23) & 1); }
inline int tagWeapon(uint32_t tag) { return (int)((tag >> 20) & 7); }
// a shot's own number with its player and weapon: the drops of one shot share it
inline uint32_t tagShot(uint32_t tag) { return tag & 0x00FFFFFFu; }

struct V {
	float x, y, z;
	V() : x(0), y(0), z(0) {}
	V(float a, float b, float c) : x(a), y(b), z(c) {}
	V(const SmsVec3& v) : x(v.x), y(v.y), z(v.z) {}
	V operator+(const V& o) const { return V(x + o.x, y + o.y, z + o.z); }
	V operator-(const V& o) const { return V(x - o.x, y - o.y, z - o.z); }
	V operator*(float k) const { return V(x * k, y * k, z * k); }
	float dot(const V& o) const { return x * o.x + y * o.y + z * o.z; }
	float len() const { return sqrtf(x * x + y * y + z * z); }
	V unit() const { const float l = len(); return l > 1e-5f ? V(x / l, y / l, z / l) : V(0, 0, 0); }
	SmsVec3 s() const { SmsVec3 v; v.x = x, v.y = y, v.z = z; return v; }
};

// A weapon's numbers (the game's own before; the menu's settings on top)
struct Weapon {
	int drops;     // at once
	float pow;     // times the stream's speed (reach)
	float alive;   // times the stream's drop life (reach)
	float size;    // times the stream's drop size
	float hit;     // times its reach (hit radius and height)
	float spread;  // radians either way (a random cone)
	float every;   // seconds between shots
	int water;     // the tank's water used per shot, in the stream's drops
	float recoil;  // Mario pushed back
	float shove;   // an enemy pushed, units a 30th of a second (slowing down)
	float lift;    // and thrown up, the same units
	int toDefeat;  // shots to defeat (with defeating on), within kHitWindow
	float burst;   // a burst's radius where it first hits (0 none)
	float kick;    // the held gun's recoil, 0 to 1
	bool straight; // flies dead straight to what the crosshair is on
};
const Weapon kDefaults[WEAPONS] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false},
	{8, 1.0f, 0.7f, 1.25f, 1.2f, 0.12f, 1.0f, 24, 6.0f, 16.0f, 0.0f, 2, 0, 0.7f, false},
	{1, 4.0f, 0.7f, 3.5f, 5.0f, 0.0f, 1.0f, 24, 6.0f, 16.0f, 0.0f, 2, 0, 0.7f, true},       // rifle: one big blob
	{1, 1.8f, 1.4f, 4.0f, 3.0f, 0.0f, 5.0f, 60, 16.0f, 34.0f, 38.0f, 1, 550.0f, 1.0f, true}, // bazooka
	{1, 4.0f, 0.7f, 3.5f, 5.0f, 0.0f, 0.15f, 24, 2.0f, 8.0f, 0.0f, 2, 0, 0.3f, true},       // submachine gun
};

// What the mod's menu says of a weapon now (FluddWeapons::shotSettings)
struct Settings {
	float every, push, range, hitSize, burst, lift, dropSize; // push under 1: the game's own push only that much
	int toDefeat, water;
	bool defeat;  // shots may defeat enemies
	bool third;   // fired in third person too (FLUDD on his back)
};

const float kHitWindow   = 4.0f;     // seconds: shots counted together towards a defeat
const float kSlow        = 0.82f;    // a shove kept each 30th of a second
const float kBazookaNow  = 900.0f;   // aimed nearer: it bursts there as it fires
const float kRifleReach  = 60000.0f; // as far as the stage goes
const int kMostLanes     = 8;
const int kMostStraight  = 16;
const int kMostHits      = 32;
const int kMostBursts    = 16;

struct Engine {
	const SmsApi* api = nullptr;
	SmsMod* mod       = nullptr;
	double time       = 0; // seconds played (ticks)
	uint32_t nextShot = 1;
	uint32_t seed     = 12345u;

	// a straight shot in flight (the rifle's, submachine gun's and bazooka's)
	struct Straight {
		uint32_t shot = 0; // tagShot, 0 none
		V from, way;
		V lanes[kMostLanes];
		int laneCount = 0;
		float reachTo = 0, speed = 0, aim = 0;
		SmsObject target = 0; // far away: what it was aimed at, hit as the shot gets there
		int weapon = 0, player = 0;
	};
	Straight flights[kMostStraight];
	int nextFlight = 0;

	// the enemies shoved and hit lately
	struct Hit {
		SmsObject enemy = 0;
		float vx = 0, vz = 0;
		int hits = 0;
		uint32_t lastShot = 0;
		double last = 0;
	};
	Hit hits[kMostHits];

	// each player's weapon
	struct Player {
		double ready = 0;          // when the next shot may go
		double soundUntil = 0;     // the spray's own sound, for a moment after a shot
		uint32_t bazookaShot = 0;  // its drop in flight, to burst where it lands
		V bazookaAt;
		bool bazookaSeen = false;
		int told = -1;             // the weapon last logged as fired
	};
	Player players[2];
	uint32_t bursts[kMostBursts] = {}; // the shots that burst (once each)
	int nextBurst = 0;

	// a hit with the game's push of a weaker weapon taken down after (SMS_EV_OBJECT_HIT .. _DONE)
	SmsObject softened = 0;
	V softenedV;
	float softenBy = 1;

	// ---------------------------------------------------------------- small tools

	float random01()
	{
		seed = seed * 1664525u + 1013904223u;
		return (float)((seed >> 8) & 0xffff) / 65535.0f;
	}

	// a direction turned a little at random, within `spread` radians
	V scatter(V d, float spread)
	{
		if (spread <= 0.0f)
			return d;
		V side(d.z, 0.0f, -d.x);
		if (side.len() < 1e-4f)
			side = V(1, 0, 0);
		side = side.unit();
		V upw(d.y * side.z, d.z * side.x - d.x * side.z, -d.y * side.x);
		const float a = (random01() * 2.0f - 1.0f) * spread, b = (random01() * 2.0f - 1.0f) * spread;
		return (d + side * a + upw * b).unit();
	}

	bool hasWater() const { return api && api->size >= offsetof(SmsApi, water) + sizeof(void*) && api->water; }
	bool hasActor() const { return api && api->size >= offsetof(SmsApi, actor) + sizeof(void*) && api->actor; }
	bool ready() const { return hasWater() && hasActor(); }

	bool burstDone(uint32_t shot) const
	{
		for (int i = 0; i < kMostBursts; i++)
			if (bursts[i] == shot)
				return true;
		return false;
	}
	void markBurst(uint32_t shot) { bursts[nextBurst++ % kMostBursts] = shot; }

	Hit* hitOf(SmsObject e)
	{
		Hit* slot = nullptr;
		for (int i = 0; i < kMostHits; i++) {
			if (hits[i].enemy == e)
				return &hits[i];
			if (!slot && (!hits[i].enemy || time - hits[i].last > 10.0))
				slot = &hits[i];
		}
		if (!slot)
			return nullptr;
		*slot       = Hit();
		slot->enemy = e;
		return slot;
	}

	// an actor's middle, and how far round and high it is hit
	bool middle(SmsObject a, V* mid, float* radius, float* height)
	{
		SmsVec3 p;
		float r = 0, h = 0;
		if (!api->game->object_position(a, &p) || !api->actor->hit_size(a, &r, &h))
			return false;
		if (mid)
			*mid = V(p.x, p.y + 0.5f * (h > 20.0f ? h : 20.0f), p.z);
		if (radius)
			*radius = r;
		if (height)
			*height = h;
		return true;
	}

	// ---------------------------------------------------------------- aiming

	// Where along the view's line an enemy is (-1 not on it): within a good deal more than its own
	// size (the rifle's grows with its hit size), so it is easy to hit
	float alongRay(const V& eye, const V& f, SmsObject a, float most, float hitScale, bool rifle)
	{
		SmsVec3 p;
		float r = 0, hh = 0;
		if (!api->game->object_position(a, &p) || !api->actor->hit_size(a, &r, &hh))
			return -1.0f;
		const float rad0 = (r > 60.0f ? r : 60.0f) + 70.0f * hitScale;
		const V c        = V(p.x, p.y + 0.5f * hh, p.z) - eye;
		const float t    = c.dot(f);
		if (t < 0.0f || t > most)
			return -1.0f;
		const float rad = rad0 + t * 0.03f * hitScale; // kinder far away
		const V off     = c - f * t;
		const float tall = rifle && hitScale > 1.0f ? hitScale : 1.0f;
		const float vy   = fabsf(off.y) - 0.5f * (hh > 60.0f ? hh : 60.0f) - 40.0f * tall;
		const float d2   = off.x * off.x + off.z * off.z + (vy > 0.0f ? vy * vy : 0.0f);
		return d2 <= rad * rad ? t : -1.0f;
	}

	// What the crosshair is on: the map along the view (game->ray), and the first small enemy on the
	// line before it; the rifle aims straight at the enemy nearest the view's middle that is seen.
	SmsObject target(const V& eye, const V& f, float reach, int weapon, float hitScale, V* at)
	{
		SmsVec3 hit;
		const V end   = eye + f * reach;
		const SmsVec3 a = eye.s(), b = end.s();
		float most    = reach;
		if (api->game->ray(&a, &b, &hit))
			most = (V(hit) - eye).len();
		if (most < 1.0f)
			most = 1.0f;
		SmsObject seen[128];
		const SmsVec3 e = eye.s();
		int n = api->actor->near(&e, reach, SMS_ACTOR_FLAG_SMALL_ENEMY, seen, 128);
		n     = n < 128 ? n : 128;
		SmsObject best = 0;
		float bestT    = most;
		for (int i = 0; i < n; i++) {
			const float t = alongRay(eye, f, seen[i], bestT, hitScale, weapon == RIFLE);
			if (t >= 0.0f && t < bestT)
				bestT = t, best = seen[i];
		}
		*at = eye + f * bestT;
		if (weapon != RIFLE)
			return best;
		// the rifle's auto aim: within a cone growing with its hit size, seen (nothing in the way)
		const float sinMax = 0.045f * (hitScale > 0.2f ? hitScale : 0.2f);
		const float cosMin = sqrtf(1.0f - (sinMax < 0.9f ? sinMax * sinMax : 0.81f));
		for (int tries = 0; tries < 4; tries++) {
			int pick     = -1;
			float pickCos = cosMin;
			V pickAt;
			for (int i = 0; i < n; i++) {
				V c;
				if (!seen[i] || !middle(seen[i], &c, nullptr, nullptr))
					continue;
				const V d     = c - eye;
				const float l = d.len();
				if (l < 50.0f || l > reach)
					continue;
				const float cs = d.dot(f) / l;
				if (cs > pickCos)
					pickCos = cs, pick = i, pickAt = c;
			}
			if (pick < 0)
				break;
			// nothing of the map between: up to just short of it
			const V d = pickAt - eye;
			const V stop = eye + d.unit() * (d.len() - 40.0f > 1.0f ? d.len() - 40.0f : 1.0f);
			const SmsVec3 sa = eye.s(), sb = stop.s();
			if (!api->game->ray(&sa, &sb, nullptr)) {
				*at = pickAt;
				return seen[pick];
			}
			seen[pick] = 0; // hidden behind a wall: the next best
		}
		if (best) // on the line but not seen: at its middle all the same
			middle(best, at, nullptr, nullptr);
		return best;
	}

	// In third person (FLUDD on his back): the aim from about his shoulders the way he faces, or at
	// the nearest small enemy a little to either side of that
	bool thirdView(int weapon, V* eye, V* f)
	{
		SmsPlayer m = SmsPlayer();
		m.size      = sizeof m;
		if (!api->player->get(&m) || !m.valid)
			return false;
		const float a = m.yaw * (3.14159265f / 180.0f);
		const V fwd(sinf(a), 0.0f, cosf(a));
		*eye = V(m.position.x, m.position.y + 100.0f, m.position.z);
		*f   = fwd;
		const float kNear = weapon == RIFLE ? 4000.0f : 3000.0f, kCone = weapon == RIFLE ? 0.77f : 0.87f;
		SmsObject seen[64];
		const SmsVec3 e = eye->s();
		int n = api->actor->near(&e, kNear + 600.0f, SMS_ACTOR_FLAG_SMALL_ENEMY, seen, 64);
		n     = n < 64 ? n : 64;
		float best = kNear;
		V to;
		bool found = false;
		for (int i = 0; i < n; i++) {
			V c;
			if (!middle(seen[i], &c, nullptr, nullptr))
				continue;
			const V d        = c - *eye;
			const float flat = sqrtf(d.x * d.x + d.z * d.z);
			if (flat < 1.0f || flat > best || (d.x * fwd.x + d.z * fwd.z) / flat < kCone || fabsf(d.y) > 600.0f)
				continue;
			best = flat, to = d, found = true;
		}
		if (found)
			*f = to.unit();
		return true;
	}

	// ---------------------------------------------------------------- firing

	// A shot from a player's FLUDD now (first person: FLUDD held in view, the view's aim; third: on
	// his back). `inView`: FLUDD held in view. Returns 1 when it fired.
	bool fire(int player, int weapon, const Weapon& k, const Settings& s, bool inView)
	{
		Player& pl       = players[player];
		const SmsHeldFluddApi* h = api->held_fludd;
		SmsWaterDrop d   = SmsWaterDrop();
		d.size           = sizeof d;
		if (!api->water->nozzle_drop(player, &d))
			return false;
		V eye, f;
		SmsVec3 se, sf;
		if (inView && h && h->size >= offsetof(SmsHeldFluddApi, aim) + sizeof(void*) && h->aim && h->aim(&se, &sf))
			eye = V(se), f = V(sf);
		else if (!thirdView(weapon, &eye, &f))
			return false;
		pl.ready = time + k.every;
		const uint32_t number = nextShot++;
		const uint32_t tag    = tagOf(player, weapon, number);
		const uint32_t shot   = tagShot(tag);
		V from(d.position), way = V(d.velocity).unit();
		Straight* st = nullptr;
		if (k.straight) {
			V at;
			const float reachNow = kRifleReach * (s.range < 1.0f ? s.range : 1.0f);
			SmsObject e = target(eye, f, reachNow, weapon, s.hitSize, &at);
			const V dv  = at - from;
			const float l = dv.len();
			if (l > 1.0f)
				way = dv * (1.0f / l);
			st = &flights[nextFlight++ % kMostStraight];
			*st         = Straight();
			st->shot    = shot;
			st->from    = from;
			st->way     = way;
			st->reachTo = weapon == BAZOOKA ? l + 200.0f : reachNow;
			st->aim     = l;
			st->target  = (l > 3000.0f || weapon == RIFLE) && weapon != BAZOOKA ? e : 0;
			st->weapon  = weapon;
			st->player  = player;
			if (weapon == BAZOOKA)
				pl.bazookaAt = at;
			api->core->log(mod, SMS_LOG_DEBUG, "the %s aimed at %s %.0f away", kNames[weapon], e ? "an enemy" : "nothing", l);
		}
		const float reach = k.straight ? 1.0f : s.range;
		const float pow   = V(d.velocity).len() * k.pow * (reach > 0.0f ? sqrtf(reach) : 1.0f);
		if (!inView) { // on his back: the drops start a frame's flight ahead of the muzzle
			from = from + way * pow;
			if (st)
				st->from = from;
		}
		SmsWaterDrop out = SmsWaterDrop();
		out.size        = sizeof out;
		out.position    = from.s();
		out.drop_size   = d.drop_size * k.size;
		out.hit_radius  = d.hit_radius * k.hit * s.hitSize;
		out.hit_height  = d.hit_height * k.hit * s.hitSize;
		out.alive       = (int)((float)d.alive * k.alive * (reach > 0.0f ? sqrtf(reach) : 1.0f));
		out.attack      = d.attack > 0 && d.attack < 256 ? d.attack : 1;
		out.tag         = tag;
		out.flags       = 0x40;
		out.player      = player;
		int made        = 0;
		for (int i = 0; i < k.drops; i++) {
			const V dir = scatter(way, k.spread);
			if (st && st->laneCount < kMostLanes)
				st->lanes[st->laneCount++] = dir; // each drop's own way, kept straight
			out.velocity = (dir * (pow * (1.0f - 0.05f * (float)i))).s();
			made += api->water->emit(&out) >= 0 ? 1 : 0;
		}
		if (!made)
			return false;
		if (h && h->size >= offsetof(SmsHeldFluddApi, kick) + sizeof(void*) && h->kick)
			h->kick(k.kick);
		pl.soundUntil = time + (weapon == BAZOOKA ? 0.45 : weapon == SMG ? 0.1 : 0.3);
		if (weapon == BAZOOKA) {
			pl.bazookaShot = shot;
			pl.bazookaSeen = false;
			if (st && st->aim < kBazookaNow && !burstDone(shot)) { // near: at once, not as its drop lands
				markBurst(shot);
				burst(player, weapon, shot, pl.bazookaAt, k, s);
			}
		}
		if (pl.told != weapon) {
			api->core->log(mod, SMS_LOG_INFO, "%s fired (%d drops)", kNames[weapon], made);
			pl.told = weapon;
		}
		api->water->use(player, k.water);
		SmsPlayer m = SmsPlayer(); // the kick, back along the shot
		m.size      = sizeof m;
		if (api->player->get(&m) && m.valid) {
			SmsVec3 v = m.velocity;
			v.x -= way.x * k.recoil;
			v.z -= way.z * k.recoil;
			api->player->set_velocity(&v);
		}
		return true;
	}

	// ---------------------------------------------------------------- each tick

	// The straight shots' drops: kept flying dead straight at the speed they set off with (the
	// game's fall taken back out) and alive, until past what they were aimed at.
	void followStraight(float dt)
	{
		const int n = api->water->count();
		for (int i = 0; i < n; i++) {
			SmsWaterDrop d = SmsWaterDrop();
			d.size         = sizeof d;
			if (!api->water->get(i, &d) || !mine(d.tag) || d.landed)
				continue;
			const uint32_t shot = tagShot(d.tag);
			Straight* st = nullptr;
			for (int q = 0; q < kMostStraight && !st; q++)
				if (flights[q].shot == shot)
					st = &flights[q];
			if (!st)
				continue;
			const V p(d.position);
			if ((p - st->from).len() > st->reachTo)
				continue; // past it: falls as any drop
			V v(d.velocity);
			if (st->speed <= 0.0f)
				st->speed = v.len();
			// its own way: the lane nearest the way it is going
			const float vl = v.len() + 1e-4f;
			int lane = 0;
			float best = -2.0f;
			for (int k = 0; k < st->laneCount; k++) {
				const float c = v.dot(st->lanes[k]) / vl;
				if (c > best)
					best = c, lane = k;
			}
			const V way = st->laneCount ? st->lanes[lane] : st->way;
			d.velocity  = (way * st->speed).s();
			// put back exactly on its line; far away faster (there in about half a second)
			const float along = (p - st->from).dot(way);
			const float quick = st->weapon == BAZOOKA ? st->aim / 6.0f : st->aim > 3000.0f ? st->aim / 15.0f : 0.0f;
			const float extra = quick > st->speed ? (quick - st->speed) * dt : 0.0f;
			if (along > 0.0f)
				d.position = (st->from + way * (along + extra)).s();
			if (st->target && along + extra >= st->aim - 100.0f) { // that fast it could pass through: hit as it gets there
				const float hl = sqrtf(way.x * way.x + way.z * way.z);
				hitEnemy(st->target, st->player, st->weapon, shot, hl > 1e-3f ? way.x / hl : 0.0f, hl > 1e-3f ? way.z / hl : 0.0f);
				st->target = 0;
			}
			if (d.alive < 30)
				d.alive = 30; // no end to its reach
			api->water->set(i, &d);
		}
	}

	// A player's bazooka drop: where it lands, it bursts
	void followBazooka(int player, const Weapon& k, const Settings& s)
	{
		Player& pl = players[player];
		if (!pl.bazookaShot)
			return;
		const int n = api->water->count();
		bool flying = false;
		for (int i = 0; i < n; i++) {
			SmsWaterDrop d = SmsWaterDrop();
			d.size         = sizeof d;
			if (!api->water->get(i, &d) || !mine(d.tag) || tagShot(d.tag) != pl.bazookaShot)
				continue;
			pl.bazookaAt   = V(d.position);
			pl.bazookaSeen = true;
			if (d.landed == 1)
				break; // landed
			flying = true;
		}
		if (flying)
			return;
		if (pl.bazookaSeen && !burstDone(pl.bazookaShot)) {
			markBurst(pl.bazookaShot);
			burst(player, BAZOOKA, pl.bazookaShot, pl.bazookaAt, k, s);
		}
		if (pl.bazookaSeen || time > pl.ready)
			pl.bazookaShot = 0;
	}

	// The shoves: moved across the ground a little each tick, slowing down
	void followShoves(float dt)
	{
		const float keep = powf(kSlow, dt);
		for (int i = 0; i < kMostHits; i++) {
			Hit& h = hits[i];
			if (!h.enemy || h.vx * h.vx + h.vz * h.vz < 0.09f)
				continue;
			if (api->actor->flags(h.enemy) & SMS_ACTOR_FLAG_DEAD) {
				h.vx = h.vz = 0.0f;
				continue;
			}
			const int moved = api->actor->push(h.enemy, h.vx * dt, h.vz * dt);
			if (!moved) {
				h.vx = h.vz = 0.0f; // the stage's edge
				continue;
			}
			if (moved == 2) { // against a wall: slides on, slower
				h.vx *= 0.5f;
				h.vz *= 0.5f;
			}
			h.vx *= keep;
			h.vz *= keep;
		}
	}

	// A player's turn each tick: a shot when it is ready and R is held, the bazooka's burst, the
	// spray's sound as it fires
	void playerTurn(int player, int weapon, bool trigger, bool inView, const Weapon& k, const Settings& s)
	{
		Player& pl = players[player];
		if (weapon > NORMAL && weapon < WEAPONS && trigger && time >= pl.ready)
			fire(player, weapon, k, s, inView);
		if (weapon == BAZOOKA || pl.bazookaShot)
			followBazooka(player, k, s);
		if (time < pl.soundUntil) {
			SmsWaterDrop d = SmsWaterDrop();
			d.size         = sizeof d;
			if (api->water->nozzle_drop(player, &d) && api->audio->size >= offsetof(SmsAudioApi, play_sound_at) + sizeof(void*))
				api->audio->play_sound_at(kSoundNozzle, &d.position);
		}
	}

	// ---------------------------------------------------------------- hits

	void hitEnemy(SmsObject e, int player, int weapon, uint32_t shot, float dx, float dz)
	{
		(void)player;
		const uint32_t f = api->actor->flags(e);
		if (!f || (f & SMS_ACTOR_FLAG_DEAD) || !(f & SMS_ACTOR_FLAG_SMALL_ENEMY))
			return;
		Hit* h = hitOf(e);
		if (!h)
			return;
		const Weapon& k   = cur(weapon);
		const Settings& s = curSettings(weapon);
		const bool fresh  = h->lastShot != shot; // this shot's first drop to reach it
		const bool walks  = (f & SMS_ACTOR_FLAG_WALKS) != 0;
		if (walks && h->vx * h->vx + h->vz * h->vz < k.shove * k.shove) { // the strongest shove, not their sum
			h->vx = dx * k.shove;
			h->vz = dz * k.shove;
		}
		if (fresh && k.lift > 0.0f && walks) { // thrown into the air: the game's own fall brings it down
			SmsVec3 v;
			if (api->actor->velocity(e, &v)) {
				v.y = k.lift;
				api->actor->set_velocity(e, &v);
				api->actor->set_flags(e, SMS_ACTOR_FLAG_AIRBORNE, 0);
			}
		}
		if (!fresh)
			return;
		h->lastShot = shot;
		if (!s.defeat)
			return;
		if (time - h->last > kHitWindow)
			h->hits = 0;
		h->hits++;
		h->last = time;
		if (h->hits < k.toDefeat)
			api->core->log(mod, SMS_LOG_DEBUG, "an enemy hit by the %s, %d of %d", kNames[weapon], h->hits, k.toDefeat);
		if (h->hits >= k.toDefeat) {
			h->hits = 0;
			api->core->log(mod, SMS_LOG_INFO, "an enemy defeated by the %s", kNames[weapon]);
			api->actor->defeat(e);
		}
	}

	// The bazooka's water explosion where it bursts: the game's water-hit sprays and splashes round
	// it, a ring of drops thrown out and up, a very big blob in the middle, and a bang
	void explode(int player, const V& at, float r)
	{
		const SmsVec3 c = at.s();
		api->game->particle(kParticleHit, &c);
		for (int i = 0; i < 6; i++) {
			const float a = (float)i * (6.2831853f / 6.0f);
			const SmsVec3 p = V(at.x + sinf(a) * r * 0.35f, at.y + 30.0f, at.z + cosf(a) * r * 0.35f).s();
			api->game->particle(kParticleHit, &p);
		}
		for (int i = 0; i < 8; i++) {
			const float a = (float)i * (6.2831853f / 8.0f);
			const SmsVec3 p = V(at.x + sinf(a) * r * 0.5f, at.y, at.z + cosf(a) * r * 0.5f).s();
			api->game->splash(&p, 12.0f);
		}
		SmsWaterDrop base = SmsWaterDrop();
		base.size         = sizeof base;
		if (!api->water->nozzle_drop(player, &base))
			return;
		SmsWaterDrop d = SmsWaterDrop();
		d.size         = sizeof d;
		d.drop_size    = base.drop_size * 1.5f;
		d.hit_radius   = base.hit_radius;
		d.hit_height   = base.hit_height;
		d.alive        = base.alive;
		d.attack       = 1;
		d.flags        = 0x40;
		d.player       = player;
		d.tag          = 0; // (the game's own water: it wets what it reaches)
		for (int i = 0; i < 24; i++) {
			const float a = (float)i * (6.2831853f / 24.0f), up = 0.55f + 0.35f * random01();
			const V dir = scatter(V(sinf(a) * (1.0f - up * 0.6f), up, cosf(a) * (1.0f - up * 0.6f)).unit(), 0.05f);
			d.position  = V(at.x, at.y + 40.0f, at.z).s();
			d.velocity  = (dir * (30.0f + 25.0f * random01())).s();
			api->water->emit(&d);
		}
		d.drop_size = base.drop_size * 12.0f; // and one very big blob in the middle, thrown a little up
		d.position  = V(at.x, at.y + 60.0f, at.z).s();
		d.velocity  = V(0.0f, 12.0f, 0.0f).s();
		api->water->emit(&d);
		if (api->audio->size >= offsetof(SmsAudioApi, play_sound_at) + sizeof(void*))
			api->audio->play_sound_at(kSoundExplosion, &c);
	}

	void burst(int player, int weapon, uint32_t shot, const V& at, const Weapon& k, const Settings& s)
	{
		(void)s;
		const float r = k.burst;
		SmsObject seen[96];
		const SmsVec3 c = at.s();
		int n = api->actor->near(&c, r + 400.0f, SMS_ACTOR_FLAG_SMALL_ENEMY, seen, 96);
		n     = n < 96 ? n : 96;
		int inIt = 0;
		for (int i = 0; i < n; i++) {
			SmsVec3 p;
			if (!api->game->object_position(seen[i], &p))
				continue;
			const float x = p.x - at.x, y = p.y - at.y, z = p.z - at.z;
			if (x * x + z * z > r * r || fabsf(y) > 400.0f) // across the ground, and not far above or below
				continue;
			const float l = sqrtf(x * x + z * z);
			hitEnemy(seen[i], player, weapon, shot, l > 1.0f ? x / l : 0.0f, l > 1.0f ? z / l : 0.0f);
			inIt++;
		}
		api->game->clean_goop(&c, r * 1.2f);
		explode(player, at, r);
		api->core->log(mod, SMS_LOG_INFO, "the bazooka burst at (%.0f, %.0f, %.0f), %d enemies in it", at.x, at.y, at.z, inIt);
	}

	// SMS_EV_OBJECT_HIT: one of this mod's drops reaching something (before the game hears of it):
	// a weaker weapon's push noted to take the game's down after. Returns 1 to cancel the hit.
	int onHit(const SmsEvHit& ev)
	{
		softened = 0;
		if (ev.message != kSprayed || !ev.object || !ev.sender)
			return 0;
		const int slot = api->water->drop_of(ev.sender);
		SmsWaterDrop d = SmsWaterDrop();
		d.size         = sizeof d;
		if (slot < 0 || !api->water->get(slot, &d) || !mine(d.tag))
			return 0;
		const int weapon = tagWeapon(d.tag);
		const Settings& s = curSettings(weapon);
		const uint32_t f = api->actor->flags(ev.object);
		if (s.push < 1.0f && (f & SMS_ACTOR_FLAG_SMALL_ENEMY)) {
			SmsVec3 v;
			if (api->actor->velocity(ev.object, &v)) {
				softened  = ev.object;
				softenedV = V(v);
				softenBy  = s.push > 0.0f ? s.push : 0.0f;
			}
		}
		return 0; // the game's own hit still
	}

	// SMS_EV_OBJECT_HIT_DONE: the game's own push taken down (a weaker weapon), the shot's hit or burst
	void onHitDone(const SmsEvHitDone& ev)
	{
		if (ev.message != kSprayed || !ev.object || !ev.sender)
			return;
		const int slot = api->water->drop_of(ev.sender);
		SmsWaterDrop d = SmsWaterDrop();
		d.size         = sizeof d;
		if (slot < 0 || !api->water->get(slot, &d) || !mine(d.tag))
			return;
		const int weapon  = tagWeapon(d.tag);
		const int player  = tagPlayer(d.tag);
		const uint32_t shot = tagShot(d.tag);
		if (softened == ev.object) { // the game's push, only that much of it
			SmsVec3 v;
			if (api->actor->velocity(ev.object, &v)) {
				v.x = softenedV.x + (v.x - softenedV.x) * softenBy;
				v.z = softenedV.z + (v.z - softenedV.z) * softenBy;
				if (v.y > softenedV.y)
					v.y = softenedV.y + (v.y - softenedV.y) * softenBy;
				api->actor->set_velocity(ev.object, &v);
			}
		}
		softened = 0;
		const uint32_t f = api->actor->flags(ev.object);
		if (!(f & SMS_ACTOR_FLAG_SMALL_ENEMY))
			return;
		const Weapon& k = cur(weapon);
		if (k.burst > 0.0f) {
			if (!burstDone(shot)) { // once a shot, where it first hits an enemy
				markBurst(shot);
				burst(player, weapon, shot, V(d.position), k, curSettings(weapon));
			}
			return;
		}
		const float l = sqrtf(d.velocity.x * d.velocity.x + d.velocity.z * d.velocity.z);
		hitEnemy(ev.object, player, weapon, shot, l > 1e-3f ? d.velocity.x / l : 0.0f, l > 1e-3f ? d.velocity.z / l : 0.0f);
	}

	// the numbers each weapon has now (set by FluddWeapons each tick, before the turns)
	Weapon now[WEAPONS];
	Settings nowSettings[WEAPONS];
	const Weapon& cur(int weapon) const { return now[weapon > 0 && weapon < WEAPONS ? weapon : 0]; }
	const Settings& curSettings(int weapon) const { return nowSettings[weapon > 0 && weapon < WEAPONS ? weapon : 0]; }

	// a weapon's numbers from its settings, over the game's own
	void setStats(int weapon, const Settings& s)
	{
		if (weapon <= NORMAL || weapon >= WEAPONS)
			return;
		const Weapon& d = kDefaults[weapon];
		Weapon& w       = now[weapon];
		w               = d;
		w.every    = s.every >= 0.05f ? s.every : 0.05f;
		w.shove    = d.shove * (s.push >= 1.0f ? s.push : 0.0f); // under 100 percent: none of its own, the game's softened
		w.lift     = d.lift * (s.lift >= 0.0f ? s.lift : 0.0f);
		w.size     = d.size * (s.dropSize > 0.05f ? s.dropSize : 1.0f);
		w.toDefeat = s.toDefeat >= 1 ? s.toDefeat : 1;
		w.water    = s.water >= 0 ? s.water : 0;
		w.burst    = d.burst * (s.burst > 0.1f ? s.burst : 0.1f);
		nowSettings[weapon] = s;
		if (weapon != RIFLE && weapon != SMG)
			nowSettings[weapon].hitSize = 1.0f;
	}
};

} // namespace shots
