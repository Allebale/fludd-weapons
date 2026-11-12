// The weapons' shots, hits and bursts: made of the game's water drops through the mod API's general
// tools (api->water, api->actor, game->ray/particle/splash/clean_goop, audio->play_sound_at,
// held_fludd->aim/kick). Included by fludd_weapons.cpp (FluddWeapons runs it each tick).
#pragma once
#include <sms_mod.hpp>
#include <math.h>
#include <string.h>
#include <stddef.h>

namespace shots {

#include "rocket_voices.inc"

// a table of the mod API has this function (an older game's may be shorter)
#define SHOTS_HAS(t, T, f) ((t) && (t)->size >= offsetof(T, f) + sizeof(void*) && (t)->f)

// the weapons (as the menu's ids: SMS_SPRAY_*'s numbers)
enum { NORMAL = 0, RIFLE = 2, BAZOOKA = 3, SMG = 4, WEAPONS = 5 };
const int PIECES = 7; // a drop's tag: the pieces of a popped enemy (they hit nothing)
const char* const kNames[WEAPONS] = {"FLUDD", "shotgun", "rifle", "bazooka", "submachine gun"};

// the game's numbers this needs (its sounds, particles, messages)
const uint32_t kSoundNozzle    = 0x0024; // MSD_SE_PO_NORMAL_NOZZLE_IMI: the spray's own sound
const uint32_t kSoundExplosion = 0x081E; // MSD_SE_PO_WFLOWER_EXPLOSION
const uint32_t kParticleHit    = 0xE7;   // PARTICLE_MS_ENM_WATHIT: water hitting an enemy
const uint32_t kSprayed        = 0xF;    // HIT_MESSAGE_SPRAYED_BY_WATER
const uint32_t kSoundWaterHit  = 0x6802; // MSD_SE_EN_COMMON_W_HIT_OK: water hitting an enemy
const uint32_t kStatusJump     = 0x02000880; // MARIO_STATUS_JUMP
const uint32_t kStatusWallSlide = 0x000008A7; // MARIO_STATUS_WALL_SLIDE
const uint32_t kStatusJumping  = 0x800;  // MARIO_STATUS_FLAG_JUMPING: any jump or fall

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
	bool inView;  // the player has FLUDD held in view (first person)
	float goop;   // how much goop it washes off, times the game's own
	float goopArea; // the bazooka's burst: how far round it washes goop, times 860 (0: the burst's own)
	bool noClean; // the bazooka's rocket jump keys held: it washes no goop (a black goop run kept)
	// the bazooka's rocket jumps (first person): how hard its burst throws Mario (0 not), the
	// push forward off his feet, a point of health each and a cry of his (its volume 0 to 1)
	float selfJump, feetForward, cryVolume;
	bool hurt, cry;
	int keyLaunch;   // 1 the rocket jump key held (hits throw enemies up), 2 hits in the air pop them
	float keyLift;   // how hard the key's hits throw them up (0: 30)
	bool airForward; // enemies already in the air pushed on forward, not up
	bool bosses;     // the weapons made for bosses (Shadow Mario, Petey Piranha, Big Chain Chomp...)
	float shadowDamage; // how much harder (above 1) or softer each hit is on Shadow Mario
	int look;        // the bazooka's shot seen flying: 0 its water, 1 a green water balloon, 2 a big one
	bool hearts;     // the bazooka's defeats drop a heart (first person)
	bool enemySafe;  // the rocket jump key held: a burst that hits an enemy costs Mario no health
	float far;       // long range: how much further enemies are seen (1 the game's own)
};

const float kHitWindow   = 4.0f;     // seconds: shots counted together towards a defeat
const float kSlow        = 0.82f;    // a shove kept each 30th of a second
const float kBazookaNow  = 900.0f;   // aimed nearer: it bursts there as it fires
const float kRifleReach  = 60000.0f; // as far as the stage goes
const float kSmgReach    = 3000.0f;  // the submachine gun's: a middle distance
const float kShadowHitbox = 3.0f;    // the weapons' aim and bursts: Shadow Mario this many times as big
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
		double until = 0; // kept straight till then (seven shots' time: the submachine gun's reach)
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
		double launchUntil = 0; // thrown up by the rocket jump key's hit: kept going up till then
		float launchUp = 0;
	};
	Hit hits[kMostHits];

	// Mario thrown by a rocket jump: steered in the air (as Team Fortress 2's) till he lands
	bool rocketAir = false;
	float rocketVX = 0, rocketVZ = 0, rocketTop = 0;

	// enemies popped in the air: their model swells and pops to nothing
	struct Pop {
		SmsObject actor = 0;
		double start = 0;
	};
	Pop pops[8];

	// each player's weapon
	struct Player {
		double ready = 0;          // when the next shot may go
		double soundUntil = 0;     // the spray's own sound, for a moment after a shot
		uint32_t bazookaShot = 0;  // its drop in flight, to burst where it lands
		V bazookaAt, bazookaWay;
		bool bazookaSeen = false;
		int told = -1;             // the weapon last logged as fired
		double shotAt = -10;       // its last shot (the pack's gun kicks back)
		int shotWeapon = 0;
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
		SmsWaterMeter wm;
		const int kd = kindOf(a);
		if ((kd == SMS_KIND_SHADOW_MARIO || kd == SMS_KIND_PROTO_PIRANHA) && meter(a, &wm)) { // where water must go
			if (mid)
				*mid = V(wm.spot);
			if (radius)
				*radius = wm.spot_radius;
			if (height)
				*height = wm.spot_radius * 2.0f;
			return true;
		}
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

	// Shadow Mario and Proto Piranha in the stage (looked for once a second among all its actors:
	// a stage has more than any short list round the view holds)
	SmsObject specials[8];
	int specialCount  = 0;
	double specialsAt = -10.0;
	SmsObject everyone[1024];
	void refreshSpecials(const V& eye)
	{
		if (time - specialsAt < 1.0)
			return;
		specialsAt      = time;
		specialCount    = 0;
		const SmsVec3 e = eye.s();
		int n           = api->actor->near(&e, 1.0e7f, 0, everyone, 1024);
		n               = n < 1024 ? n : 1024;
		for (int i = 0; i < n && specialCount < 8; i++) {
			const int kd = api->actor->kind(everyone[i]);
			if (kd == SMS_KIND_SHADOW_MARIO || kd == SMS_KIND_PROTO_PIRANHA)
				specials[specialCount++] = everyone[i];
		}
	}

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
		// Shadow Mario (his hitbox three times as big) and Proto Piranha
		if (hasKinds()) {
			refreshSpecials(eye);
			for (int i = 0; i < specialCount; i++) {
				SmsObject* all = specials;
				const int kd = api->actor->kind(all[i]);
				SmsWaterMeter wm;
				if ((kd != SMS_KIND_SHADOW_MARIO && kd != SMS_KIND_PROTO_PIRANHA) || !meter(all[i], &wm) || !wm.open)
					continue;
				// (Proto Piranha: its open mouth only itself; asleep, its goop pile two and a half times as big)
				const float rad = kd == SMS_KIND_SHADOW_MARIO ? wm.spot_radius * kShadowHitbox
				                  : wm.most == 1             ? wm.spot_radius
				                                             : wm.spot_radius * 2.5f;
				const V c       = V(wm.spot) - eye;
				const float t   = c.dot(f);
				const V off     = c - f * t;
				if (t > 0.0f && t < bestT && off.dot(off) <= rad * rad)
					bestT = t, best = all[i];
			}
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
			const float reachNow = (weapon == SMG ? kSmgReach : kRifleReach) * (s.range < 1.0f ? s.range : 1.0f);
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
			// what it was aimed at, hit (the bazooka: burst at) as the shot gets there, so it is not missed
			// (Shadow Mario, Proto Piranha: always, their drops' hit areas are small)
			st->target  = e && (l > 3000.0f || weapon == RIFLE || weapon == BAZOOKA || kindOf(e) != SMS_KIND_SMALL_ENEMY) ? e : 0;
			st->weapon  = weapon;
			st->player  = player;
			st->until   = time + 7.0 * (k.every > 0.1f ? k.every : 0.1f);
			if (weapon == BAZOOKA)
				pl.bazookaAt = at;
			api->core->log(mod, SMS_LOG_INFO, "[diag] the %s (%s) aimed at %s %.0f away", kNames[weapon], inView ? "first person" : "third person",
			               e ? "an enemy" : "nothing", l);
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
		pl.shotAt     = time;
		pl.shotWeapon = weapon;
		if (weapon == BAZOOKA) {
			pl.bazookaShot = shot;
			pl.bazookaSeen = false;
			if (st && s.look >= 1 && hasModels())
				startBalloon(player, from, way, st->aim, s.look == 2);
			if (st && st->aim < kBazookaNow && !burstDone(shot)) { // near: at once, not as its drop lands
				markBurst(shot);
				burst(player, weapon, shot, pl.bazookaAt, k, s, way);
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
			if (!api->water->get(i, &d) || !mine(d.tag))
				continue;
			if (d.landed) { // on the ground or a wall: a rifle's or submachine gun's washes more goop off, once
				const int w = tagWeapon(d.tag);
				if ((w == RIFLE || w == SMG) && curSettings(w).goop > 1.0f) {
					api->game->clean_goop(&d.position, d.clean * curSettings(w).goop);
					d.tag = 0; // (no longer this mod's: it has hit what it will)
					api->water->set(i, &d);
				}
				continue;
			}
			const uint32_t shot = tagShot(d.tag);
			Straight* st = nullptr;
			for (int q = 0; q < kMostStraight && !st; q++)
				if (flights[q].shot == shot)
					st = &flights[q];
			if (!st || time > st->until)
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
				if (st->weapon == BAZOOKA) { // the bazooka bursts on it
					V at;
					if (!burstDone(shot) && middle(st->target, &at, nullptr, nullptr)) {
						markBurst(shot);
						players[st->player].bazookaShot = 0;
						d.alive = 0; // (its drop is done)
						burst(st->player, BAZOOKA, shot, at, cur(BAZOOKA), curSettings(BAZOOKA), way);
					}
				} else if (kindOf(st->target) != SMS_KIND_SMALL_ENEMY) {
					hitTarget(st->target, st->player, st->weapon, shot); // Shadow Mario, Proto Piranha
				} else {
					// very near (the drop set off inside it) or far (that fast it went through): the
					// drop never reached it as the game sees it, so the game's own water hit (its own
					// push, a Strollin' Stu's roll) given here too, once a shot
					Hit* h = hitOf(st->target);
					if (h && h->lastShot != shot)
						api->actor->send(st->target, 0, kSprayed);
					hitEnemy(st->target, st->player, st->weapon, shot, hl > 1e-3f ? way.x / hl : 0.0f, hl > 1e-3f ? way.z / hl : 0.0f);
				}
				st->target = 0;
			}
			if (d.alive > 0 && d.alive < 30)
				d.alive = 30; // no end to its reach (0: just burst, done)
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
			if (!d.landed)
				pl.bazookaWay = V(d.velocity);
			pl.bazookaSeen = true;
			if (d.landed == 1)
				break; // landed
			flying = true;
		}
		if (flying)
			return;
		if (pl.bazookaSeen && !burstDone(pl.bazookaShot)) {
			markBurst(pl.bazookaShot);
			burst(player, BAZOOKA, pl.bazookaShot, pl.bazookaAt, k, s, pl.bazookaWay);
		}
		if (pl.bazookaSeen || time > pl.ready)
			pl.bazookaShot = 0;
	}

	// The shoves: moved across the ground a little each tick, slowing down
	void followShoves(float dt)
	{
		const float keep = powf(kSlow, dt);
		for (int i = 0; i < kMostHits; i++) { // thrown up by the rocket jump key's hit: off the ground, whatever its own move did
			Hit& h = hits[i];
			if (!h.enemy || time >= h.launchUntil || (api->actor->flags(h.enemy) & SMS_ACTOR_FLAG_DEAD))
				continue;
			SmsVec3 v;
			if (api->actor->velocity(h.enemy, &v)) {
				if (v.y < h.launchUp * 0.8f)
					v.y = h.launchUp;
				v.x *= 0.4f, v.z *= 0.4f; // (going up, hardly across)
				api->actor->set_velocity(h.enemy, &v);
			}
			api->actor->set_flags(h.enemy, SMS_ACTOR_FLAG_AIRBORNE, 0);
		}
		for (int i = 0; i < kMostHits; i++) {
			Hit& h = hits[i];
			if (!h.enemy || h.vx * h.vx + h.vz * h.vz < 0.09f)
				continue;
			if (api->actor->flags(h.enemy) & SMS_ACTOR_FLAG_DEAD) {
				h.vx = h.vz = 0.0f;
				continue;
			}
			const int moved = api->actor->push(h.enemy, h.vx * dt, h.vz * dt);
			if (moved != 1)
				api->core->log(mod, SMS_LOG_INFO, "[diag] a shove of %.0f %s", sqrtf(h.vx * h.vx + h.vz * h.vz),
				               moved == 2 ? "against a wall" : "stopped (the stage's edge)");
			if (!moved) {
				h.vx = h.vz = 0.0f; // the stage's edge
				continue;
			}
			if (moved == 2) { // against a wall: those the game defeats so (Strollin' Stus) are, hard enough
				if (h.vx * h.vx + h.vz * h.vz > 6.0f * 6.0f && api->actor->wall_defeat(h.enemy)) {
					h.vx = h.vz = 0.0f;
					continue;
				}
				h.vx *= 0.5f; // the rest slide on, slower
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

	// ---------------------------------------------------------------- bosses, characters, things

	bool hasKinds() const { return SHOTS_HAS(api->actor, SmsActorApi, knock); }
	int kindOf(SmsObject o) const
	{
		if (hasKinds())
			return api->actor->kind(o);
		return (api->actor->flags(o) & SMS_ACTOR_FLAG_SMALL_ENEMY) ? SMS_KIND_SMALL_ENEMY : SMS_KIND_OTHER;
	}
	bool meter(SmsObject o, SmsWaterMeter* m)
	{
		*m      = SmsWaterMeter();
		m->size = sizeof *m;
		return hasKinds() && api->actor->water_meter(o, m);
	}
	void setMeter(SmsObject o, int left, bool effects) { api->actor->set_water_meter(o, left > 0 ? left : 0, effects ? 1 : 0); }

	// what a shot has already done to each (once a shot)
	struct Once {
		SmsObject object = 0;
		uint32_t shot = 0;
		double at = 0;
	};
	Once onces[32];
	int nextOnce = 0;
	bool firstTime(SmsObject o, uint32_t shot)
	{
		for (int i = 0; i < 32; i++)
			if (onces[i].object == o && onces[i].shot == shot && time - onces[i].at < 3.0)
				return false;
		Once& k  = onces[nextOnce++ % 32];
		k.object = o, k.shot = shot, k.at = time;
		return true;
	}
	// parts of a point carried to the next shot (a boss's share by shots that do not divide it)
	struct Owed {
		SmsObject object = 0;
		float owed = 0;
	};
	Owed oweds[8];
	int nextOwed = 0;
	float& owedOf(SmsObject o)
	{
		for (int i = 0; i < 8; i++)
			if (oweds[i].object == o)
				return oweds[i].owed;
		Owed& k  = oweds[nextOwed++ % 8];
		k.object = o, k.owed = 0.0f;
		return k.owed;
	}
	// a share of `most` taken by one of `shots` shots
	int share(SmsObject o, int most, float shots)
	{
		float& owed     = owedOf(o);
		const float sh  = (float)most / (shots > 0.01f ? shots : 1.0f) + owed;
		const int whole = (int)(sh + 0.001f);
		owed            = sh - (float)whole;
		return whole;
	}

	// a character's or Petey Piranha's water just before the game's own drop reaches it
	SmsObject before = 0;
	int beforeLeft   = 0;
	// Big Chain Chomp being cooled: its heat not coming back while it is shot (2 seconds)
	SmsObject chomp = 0;
	int chompLeft   = 0;
	double chompAt  = 0;
	uint32_t chompShot = 0;

	// shots to beat each: a tooth (the submachine gun's: a second of firing), Big Chain Chomp
	float toothShots(int weapon) const
	{
		if (weapon == SMG) {
			const float every = cur(SMG).every > 0.01f ? cur(SMG).every : 0.14f;
			const float n     = floorf(1.0f / every + 0.5f);
			return n > 1.0f ? n : 1.0f;
		}
		return weapon == RIFLE || weapon == BAZOOKA ? 1.0f : 0.0f;
	}
	static float chompShots(int weapon) { return weapon == RIFLE ? 3.0f : weapon == BAZOOKA ? 1.0f : weapon == SMG ? 42.0f : 0.0f; }
	// a character's goop each shot washes (thousandths): FLUDD's own bursts' worth
	static int washOf(int weapon) { return weapon == RIFLE ? 500 : weapon == BAZOOKA ? 1000 : weapon == SMG ? 24 : 0; }
	static float shadowShare(int weapon) { return weapon == RIFLE ? 0.34f : weapon == BAZOOKA ? 0.5f : weapon == SMG ? 0.1f : 0.0f; }

	// Shadow Mario wetted by a shot: the weapon's share of his water (the game took one itself)
	void shadowHit(SmsObject o, int weapon, uint32_t shot, bool gameCounted)
	{
		SmsWaterMeter m;
		const Settings& s = curSettings(weapon);
		if (!meter(o, &m) || !m.open || !firstTime(api->actor->owner(o), shot))
			return;
		const int take = (int)((float)m.most * shadowShare(weapon) * s.shadowDamage + 0.5f) - (gameCounted ? 1 : 0);
		if (take < 1)
			return;
		setMeter(o, m.left - take, true);
		api->core->log(mod, SMS_LOG_INFO, "Shadow Mario hit by the %s: %d of his %d water left", kNames[weapon],
		               m.left - take > 0 ? m.left - take : 0, m.most);
	}

	// a tooth's share of cleaning for a shot; `now`: no drop of the game's follows (a burst)
	void toothHit(SmsObject o, int weapon, uint32_t shot, bool now)
	{
		SmsWaterMeter m;
		if (toothShots(weapon) <= 0.0f || !meter(o, &m) || !m.open || !firstTime(o, shot))
			return;
		int take       = share(o, m.most, toothShots(weapon));
		take           = take > 1 ? take : 1;
		const int left = m.left - take > 0 ? m.left - take : 0;
		setMeter(o, now ? left : left + 1, now); // (else the game's own hit takes the last)
		api->core->log(mod, SMS_LOG_INFO, "Eely-Mouth's tooth hit by the %s: %d of %d left", kNames[weapon], left, m.most);
	}

	// Big Chain Chomp's share of cooling for a shot. `gameNext`: the game's own drop takes one off itself next
	void chompHit(SmsObject o, int weapon, uint32_t shot, bool gameNext)
	{
		SmsWaterMeter m;
		const SmsObject c = api->actor->owner(o);
		if (chompShots(weapon) <= 0.0f || !meter(c, &m) || !m.open)
			return;
		if (c == chomp && shot == chompShot && time - chompAt < 3.0) { // its other drops: the game's own taken back
			if (gameNext)
				setMeter(c, m.left + 1, false);
			return;
		}
		int whole = share(c, m.most, chompShots(weapon));
		if (whole < 1 && weapon == BAZOOKA)
			whole = m.most;
		const int left = m.left - whole > 0 ? m.left - whole : 0;
		chomp = c, chompLeft = left, chompAt = time, chompShot = shot;
		if (left == 0)
			owedOf(c) = 0.0f;
		setMeter(c, gameNext ? left + 1 : left, !gameNext);
		api->core->log(mod, SMS_LOG_INFO, "Big Chain Chomp hit by the %s: %d of its %d heat left", kNames[weapon], left, m.most);
	}

	// each tick: Chomp's heat not coming back while it is being shot
	void chompTick()
	{
		SmsWaterMeter m;
		if (!chomp || time - chompAt > 2.0 || !meter(chomp, &m))
			return;
		if (m.left > chompLeft && chompLeft > 0)
			setMeter(chomp, chompLeft, false);
		else if (m.left < chompLeft)
			chompLeft = m.left; // (the normal spray too)
	}

	// A shot got to what it was aimed at, a boss part no drop meets (far away, or Shadow Mario, who
	// runs too fast): Shadow Mario wetted (the game's own count, then the weapon's share), Proto
	// Piranha's mouth or pile hit
	void hitTarget(SmsObject o, int player, int weapon, uint32_t shot)
	{
		(void)player;
		const int kd = kindOf(o);
		if (kd == SMS_KIND_SHADOW_MARIO) {
			SmsWaterMeter m;
			if (!meter(o, &m) || !m.open)
				return;
			for (int i = 0; i < 32; i++)
				if (onces[i].object == api->actor->owner(o) && onces[i].shot == shot && time - onces[i].at < 3.0)
					return;
			api->actor->send(o, 0, kSprayed);
			shadowHit(o, weapon, shot, true);
		} else if (kd == SMS_KIND_PROTO_PIRANHA && curSettings(weapon).bosses)
			gatekeeperHit(o, weapon);
	}

	// Proto Piranha: its open mouth takes the hit; asleep in its goop pile, a bazooka shot wakes it
	// at once, two rifle shots do
	SmsObject pileOf = 0;
	double pileAt    = 0;
	int pileHits     = 0;
	void gatekeeperHit(SmsObject o, int weapon)
	{
		SmsWaterMeter m;
		if (!meter(o, &m) || !m.open)
			return;
		if (m.most == 1) {
			setMeter(o, 0, true);
			api->core->log(mod, SMS_LOG_INFO, "Proto Piranha hit in the mouth by the %s", kNames[weapon]);
			return;
		}
		bool wake = weapon == BAZOOKA;
		if (weapon == RIFLE) {
			if (pileOf != o || time - pileAt > 5.0)
				pileHits = 0;
			pileOf = o, pileAt = time;
			wake   = ++pileHits >= 2;
		}
		if (wake)
			pileHits = 0;
		setMeter(o, wake ? 0 : m.left, true);
		api->core->log(mod, SMS_LOG_INFO, "Proto Piranha's goop pile hit by the %s%s", kNames[weapon], wake ? ": it comes out" : "");
	}

	// SMS_EV_OBJECT_HIT on what is no small enemy: before the game's own hit
	void otherHitBefore(SmsObject o, int kd, int weapon, uint32_t shot, const Settings& s)
	{
		before = 0;
		SmsWaterMeter m;
		if ((kd == SMS_KIND_CHARACTER || kd == SMS_KIND_PETEY_PIRANHA) && meter(o, &m)) {
			before     = api->actor->owner(o);
			beforeLeft = m.left;
		}
		if (!s.bosses)
			return;
		if (kd == SMS_KIND_EELY_MOUTH_TOOTH)
			toothHit(o, weapon, shot, false);
		else if (kd == SMS_KIND_PHANTAMANTA && (weapon == RIFLE || weapon == BAZOOKA)) {
			if (meter(o, &m) && m.open && m.left > 1) { // the game's own hit (now) its last
				setMeter(o, 1, false);
				api->core->log(mod, SMS_LOG_INFO, "Phantamanta finished by the %s", kNames[weapon]);
			}
		} else if (kd == SMS_KIND_BIG_CHAIN_CHOMP)
			chompHit(o, weapon, shot, true);
	}

	// SMS_EV_OBJECT_HIT_DONE on what is no small enemy: after the game's own hit
	void otherHitDone(SmsObject o, int kd, int weapon, uint32_t shot, int result, const Settings& s)
	{
		SmsWaterMeter m;
		const SmsObject owner = api->actor->owner(o);
		if (kd == SMS_KIND_CHARACTER && meter(o, &m)) { // its goop: the game's own washing taken back, the weapon's share
			if (before == owner && m.left > 0 && m.left < beforeLeft)
				setMeter(o, beforeLeft, false), m.left = beforeLeft;
			if (m.open && firstTime(owner, shot)) {
				setMeter(o, m.left - washOf(weapon), true);
				if (m.left - washOf(weapon) <= 0)
					api->core->log(mod, SMS_LOG_INFO, "a character washed clean by the %s", kNames[weapon]);
			}
		} else if (kd == SMS_KIND_HIDING_PICTURE && result && meter(o, &m) && m.open) {
			// a weapon's few drops each wash as much as a moment of FLUDD's stream (the bazooka's at once)
			setMeter(o, weapon == BAZOOKA ? 0 : m.left - (weapon == RIFLE ? 120 : 15), false);
		} else if (kd == SMS_KIND_SHADOW_MARIO && cur(weapon).burst <= 0.0f) {
			shadowHit(o, weapon, shot, true);
		} else if (s.bosses && kd == SMS_KIND_PETEY_PIRANHA && meter(o, &m)) {
			if (before == owner && m.left < beforeLeft) { // the drop counted in his mouth fills him more
				const int add = weapon == BAZOOKA ? m.most : weapon == RIFLE ? m.most / 4 : weapon == SMG ? m.most / 24 : 0;
				setMeter(o, m.left - add, false);
				api->core->log(mod, SMS_LOG_INFO, "Petey Piranha filled by the %s: %d of %d", kNames[weapon],
				               m.most - (m.left - add > 0 ? m.left - add : 0), m.most);
			}
		} else if (s.bosses && kd == SMS_KIND_PHANTAMANTA && weapon == SMG && meter(o, &m) && m.wait == 30)
			api->actor->set_water_wait(o, 20); // its next hit sooner
		before = 0;
	}

	// The bazooka's burst on all but small enemies: Shadow Mario (his hitbox bigger), Petey's open
	// mouth, Proto Piranha, Eely-Mouth's teeth, Phantamantas, Big Chain Chomp, characters washed,
	// hiding pictures washed off, balls and fruit thrown
	void burstOthers(int player, int weapon, uint32_t shot, const V& at, float r, const Settings& s)
	{
		if (!hasKinds())
			return;
		SmsObject all[160];
		const SmsVec3 c = at.s();
		int n           = api->actor->near(&c, r * kShadowHitbox + 1500.0f, 0, all, 160);
		n               = n < 160 ? n : 160;
		int things = 0, others = 0;
		const float lift = cur(BAZOOKA).lift > 1.0f ? cur(BAZOOKA).lift : 20.0f;
		for (int i = 0; i < n; i++) {
			const SmsObject o = all[i];
			const int kd      = api->actor->kind(o);
			SmsWaterMeter m;
			if (kd == SMS_KIND_SMALL_ENEMY || kd == SMS_KIND_OTHER)
				continue;
			if (kd == SMS_KIND_BALL || kd == SMS_KIND_HIDING_PICTURE) {
				SmsVec3 p;
				if (!api->game->object_position(o, &p))
					continue;
				const float x = p.x - at.x, y = p.y - at.y, z = p.z - at.z, d = sqrtf(x * x + z * z);
				if (kd == SMS_KIND_HIDING_PICTURE) {
					if (d <= r * 1.5f && fabsf(y) <= r * 1.5f && meter(o, &m) && m.open) {
						setMeter(o, 0, true);
						api->core->log(mod, SMS_LOG_INFO, "a picture washed off by the bazooka");
					}
					continue;
				}
				if (d > r * 1.3f || fabsf(y) > r)
					continue;
				const float near1 = 1.0f - d / (r * 1.3f); // 1 at the middle, 0 at the edge
				SmsVec3 push;
				const float out = lift * 0.55f * (0.35f + 0.65f * near1);
				push.x = (d > 1.0f ? x / d : 1.0f) * out, push.z = (d > 1.0f ? z / d : 0.0f) * out;
				push.y = lift * 0.5f * (0.4f + 0.6f * near1);
				things += api->actor->knock(o, &push);
				continue;
			}
			if (kd == SMS_KIND_CHARACTER) {
				if (meter(o, &m) && m.open && (V(m.spot) - at).len() <= r && firstTime(o, shot)) {
					setMeter(o, m.left - washOf(BAZOOKA), true);
					others++;
				}
				continue;
			}
			if (kd == SMS_KIND_SHADOW_MARIO) {
				if (!meter(o, &m))
					continue;
				const V d = V(m.spot) - at;
				const float big = r * kShadowHitbox;
				if (d.x * d.x + d.z * d.z <= big * big && fabsf(d.y) <= 400.0f * kShadowHitbox) {
					hitTarget(o, player, weapon, shot);
					others++;
				}
				continue;
			}
			if (!s.bosses)
				continue;
			if (kd == SMS_KIND_EELY_MOUTH) { // its teeth in it: each its share
				SmsObject teeth[8];
				int t = api->actor->parts(o, teeth, 8);
				t     = t < 8 ? t : 8;
				for (int k = 0; k < t; k++)
					if (meter(teeth[k], &m) && (V(m.spot) - at).len() <= r + m.spot_radius) {
						toothHit(teeth[k], BAZOOKA, shot, true);
						others++;
					}
				continue;
			}
			if (!meter(o, &m) || !m.open)
				continue;
			const float d = (V(m.spot) - at).len();
			if (kd == SMS_KIND_PETEY_PIRANHA && d <= m.spot_radius + 150.0f) { // a burst at his open mouth: full
				setMeter(o, 0, true);
				api->core->log(mod, SMS_LOG_INFO, "Petey Piranha filled by the bazooka");
				others++;
			} else if (kd == SMS_KIND_PROTO_PIRANHA && d <= (m.most == 1 ? m.spot_radius + 40.0f : r * 2.5f)) {
				gatekeeperHit(o, BAZOOKA);
				others++;
			} else if (kd == SMS_KIND_PHANTAMANTA && d <= r + m.spot_radius) {
				setMeter(o, 0, true);
				api->core->log(mod, SMS_LOG_INFO, "Phantamanta finished by the bazooka");
				others++;
			} else if (kd == SMS_KIND_BIG_CHAIN_CHOMP && d <= r + m.spot_radius) {
				chompHit(o, BAZOOKA, shot, false);
				others++;
			}
		}
		if (things || others)
			api->core->log(mod, SMS_LOG_INFO, "the bazooka's burst: %d bosses or characters, %d things", others, things);
	}

	// ---------------------------------------------------------------- looks: the bazooka's balloon, hearts

	// the bazooka's green water balloon (its shot's look 1, 2 the big one): flown straight to where it
	// bursts in as many frames as its own drop, then it pops there (its rubber in pieces, a splash)
	struct Balloon {
		bool on = false, big = false;
		V from, way;
		float aim = 0, along = 0, frames = 6;
	};
	Balloon balloons[2];
	// its rubber, burst into pieces: thrown out, falling and tumbling, gone after about half a second
	struct Piece {
		V at, v, axis;
		float spin = 0, turn = 0, size = 0, life = 0; // life: ticks left
	};
	Piece pieces[12];
	// hearts dropped where the bazooka defeats an enemy (first person, rocket jumps on): a point of
	// health each as Mario touches one, gone after 15 seconds (blinking the last 3)
	struct Heart {
		V at;
		float vy = 0, ground = 0, spin = 0, life = 0;
		bool landed = false;
	};
	Heart heartList[8];
	SmsCorner looks[12000];
	int lookCount   = 0;
	int balloonTick = 0;

	bool hasModels() const { return api->size >= offsetof(SmsApi, models) + sizeof(void*) && api->models; }

	void startBalloon(int player, const V& from, const V& way, float aim, bool big)
	{
		Balloon& b = balloons[player & 1];
		b.on       = true;
		b.big      = big;
		b.from     = from;
		b.way      = way;
		b.aim      = aim;
		// from the muzzle (the small one comes out of it), the big one a little ahead (not over the camera)
		b.along  = !big ? 0.0f : aim * 0.3f < 140.0f ? aim * 0.3f : 140.0f;
		b.frames = aim < kBazookaNow ? 5.0f : 6.0f;
	}

	void dropHeart(SmsObject e)
	{
		SmsVec3 p;
		if (!api->game->object_position(e, &p))
			return;
		Heart* h = &heartList[0];
		for (int i = 0; i < 8; i++) { // a free one, or the oldest
			if (heartList[i].life <= 0.0f) {
				h = &heartList[i];
				break;
			}
			if (heartList[i].life < h->life)
				h = &heartList[i];
		}
		const float a = random01() * 6.2831853f; // beside it, its coin in the middle
		h->at         = V(p.x + sinf(a) * 45.0f, p.y + 40.0f, p.z + cosf(a) * 45.0f);
		const SmsVec3 top = V(h->at.x, h->at.y + 50.0f, h->at.z).s(), bottom = V(h->at.x, h->at.y - 3000.0f, h->at.z).s();
		SmsVec3 hit;
		h->ground = api->game->ray(&top, &bottom, &hit) ? hit.y : p.y;
		h->vy     = 14.0f; // popped up
		h->spin   = a;
		h->life   = 450.0f;
		h->landed = false;
		api->core->log(mod, SMS_LOG_INFO, "a heart dropped");
	}

	// each tick: the balloons flying and popping, the pieces, the hearts falling and picked up
	void looksTick(float dt)
	{
		for (int i = 0; i < 12; i++) {
			Piece& q = pieces[i];
			if (q.life <= 0.0f)
				continue;
			q.life -= dt;
			q.v.y -= 1.6f * dt; // falling
			q.v.x *= powf(0.92f, dt), q.v.z *= powf(0.92f, dt);
			q.at = q.at + q.v * dt;
			q.spin += q.turn * dt;
		}
		for (int p = 0; p < 2; p++) {
			Balloon& b = balloons[p];
			if (!b.on)
				continue;
			b.along += b.aim / b.frames * dt;
			const float end = b.aim - (b.big ? 40.0f : 6.0f);
			if (b.along < end)
				continue;
			b.on        = false; // there: it pops, its rubber in pieces, with a splash
			const V at  = b.from + b.way * end;
			const SmsVec3 sa = at.s();
			api->game->particle(kParticleHit, &sa);
			for (int i = 0; i < 12; i++) {
				Piece& q = pieces[i];
				// out every way, more back towards Mario and up than into what it hit
				V d = V(random01() * 2.0f - 1.0f, random01() * 1.4f - 0.2f, random01() * 2.0f - 1.0f) - b.way * 0.8f;
				d   = d.unit();
				const float sp = (7.0f + 9.0f * random01()) * (b.big ? 1.6f : 1.0f);
				q.at   = at;
				q.v    = V(d.x * sp, d.y * sp + 4.0f, d.z * sp);
				q.axis = V(random01() - 0.5f, random01() - 0.5f, random01() - 0.5f).unit();
				q.spin = random01() * 6.28f;
				q.turn = 0.3f + 0.5f * random01();
				q.size = (2.5f + 2.5f * random01()) * (b.big ? 5.0f : 1.0f);
				q.life = 12.0f + 6.0f * random01();
			}
		}
		const SmsHeldFluddApi* hf = api->held_fludd;
		const bool sel = SHOTS_HAS(hf, SmsHeldFluddApi, select_player);
		const int np   = sel && hf->players ? hf->players() : 1;
		for (int i = 0; i < 8; i++) {
			Heart& h = heartList[i];
			if (h.life <= 0.0f)
				continue;
			h.life -= dt;
			h.spin += 0.08f * dt;
			if (!h.landed) {
				h.vy -= 1.5f * dt;
				h.at.y += h.vy * dt;
				if (h.at.y <= h.ground + 40.0f && h.vy < 0.0f)
					h.at.y = h.ground + 40.0f, h.landed = true;
			}
			for (int p = 0; p < np && h.life > 0.0f; p++) { // either player (co-op) touching it
				if (sel)
					hf->select_player(p);
				SmsPlayer m = SmsPlayer();
				m.size      = sizeof m;
				if (api->player->get(&m) && m.valid && !m.on_yoshi) {
					const float dx = m.position.x - h.at.x, dz = m.position.z - h.at.z, dy = h.at.y - m.position.y;
					if (dx * dx + dz * dz < 80.0f * 80.0f && dy > -60.0f && dy < 190.0f) {
						h.life = 0.0f;
						api->player->set_health(m.health + 1);
						api->audio->play_system_sound(0x4801); // MSD_SE_SY_HP_RECOVER
						api->core->log(mod, SMS_LOG_INFO, "a heart picked up: a point of health");
					}
				}
				if (sel)
					hf->select_player(-1);
			}
		}
	}

	void corner(const V& p, int r, int g, int b)
	{
		if (lookCount >= 12000)
			return;
		SmsCorner& c = looks[lookCount++];
		c.x = p.x, c.y = p.y, c.z = p.z;
		c.r = (uint8_t)(r < 0 ? 0 : r > 255 ? 255 : r);
		c.g = (uint8_t)(g < 0 ? 0 : g > 255 ? 255 : g);
		c.b = (uint8_t)(b < 0 ? 0 : b > 255 ? 255 : b);
		c.a = 255;
	}

	// a balloon's corner: lat from -1 (its back, the knot) to 1 (its front); round, a little longer
	// along the way, its skin wobbling, its light baked (from above), a shine on top
	void balloonCorner(const V& c, const V& fwd, const V& rt, const V& up, float r, float len, float lat, float lon, float wob)
	{
		const float a = lat * 1.5707963f, ring = cosf(a), along = sinf(a);
		const float sx = ring * cosf(lon), sy = ring * sinf(lon);
		const float rr = r * (1.0f + 0.06f * sinf(wob + 3.0f * lon));
		const V n      = fwd * along + rt * sx + up * sy;
		const V p      = c + fwd * (along * len) + (rt * sx + up * sy) * rr;
		const float l  = n.y * 0.8f + 0.35f;
		const float d  = l > 0.0f ? (l < 1.0f ? l : 1.0f) : 0.0f;
		const float shine = n.y > 0.88f ? (n.y - 0.88f) * 6.0f : 0.0f;
		corner(p, (int)(20.0f + 50.0f * d + 200.0f * shine), (int)(70.0f + 170.0f * d + 120.0f * shine), (int)(25.0f + 45.0f * d + 200.0f * shine));
	}

	void drawBalloon(const Balloon& b)
	{
		const V c     = b.from + b.way * b.along;
		const V fwd   = b.way;
		V upw         = fabsf(fwd.y) > 0.95f ? V(1.0f, 0.0f, 0.0f) : V(0.0f, 1.0f, 0.0f);
		V rt          = V(fwd.y * upw.z - fwd.z * upw.y, fwd.z * upw.x - fwd.x * upw.z, fwd.x * upw.y - fwd.y * upw.x);
		if (rt.len() < 1e-4f)
			return;
		rt          = rt.unit();
		const V up  = V(rt.y * fwd.z - rt.z * fwd.y, rt.z * fwd.x - rt.x * fwd.z, rt.x * fwd.y - rt.y * fwd.x);
		const float scale = b.big ? 6.5f : 1.0f; // the big one: about 42 round
		const float r = 6.5f * scale, len = 8.0f * scale;
		const float wob = (float)(balloonTick)*0.9f;
		const int kRings = 10, kAround = 16;
		for (int i = 0; i < kRings; i++) {
			const float a0 = -1.0f + 2.0f * (float)i / kRings, a1 = -1.0f + 2.0f * (float)(i + 1) / kRings;
			for (int j = 0; j < kAround; j++) {
				const float l0 = 6.2831853f * (float)j / kAround, l1 = 6.2831853f * (float)(j + 1) / kAround;
				balloonCorner(c, fwd, rt, up, r, len, a0, l0, wob);
				balloonCorner(c, fwd, rt, up, r, len, a1, l0, wob);
				balloonCorner(c, fwd, rt, up, r, len, a1, l1, wob);
				balloonCorner(c, fwd, rt, up, r, len, a0, l0, wob);
				balloonCorner(c, fwd, rt, up, r, len, a1, l1, wob);
				balloonCorner(c, fwd, rt, up, r, len, a0, l1, wob);
			}
		}
		// its knot: a small dark green cone at its back
		const V back = c - fwd * len, tip = back - fwd * (2.2f * scale);
		for (int j = 0; j < 8; j++) {
			const float l0 = 6.2831853f * (float)j / 8.0f, l1 = 6.2831853f * (float)(j + 1) / 8.0f, k = 1.4f * scale;
			corner(tip, 20, 90, 30);
			corner(back + rt * (cosf(l0) * k) + up * (sinf(l0) * k), 30, 120, 40);
			corner(back + rt * (cosf(l1) * k) + up * (sinf(l1) * k), 30, 120, 40);
		}
	}

	void drawPieces()
	{
		for (int i = 0; i < 12; i++) {
			const Piece& q = pieces[i];
			if (q.life <= 0.0f)
				continue;
			const float sz = q.size * (q.life < 4.0f ? q.life / 4.0f : 1.0f); // shrinking away at the end
			const float cs = cosf(q.spin), sn = sinf(q.spin);
			const V base[2] = {V(1.0f, 0.0f, 0.0f), V(0.0f, 0.0f, 1.0f)};
			V e[2];
			for (int k = 0; k < 2; k++) { // two edges across it, turned about its axis by its spin (Rodrigues)
				const V& v    = base[k];
				const float d = q.axis.dot(v);
				const V cr    = V(q.axis.y * v.z - q.axis.z * v.y, q.axis.z * v.x - q.axis.x * v.z, q.axis.x * v.y - q.axis.y * v.x);
				e[k]          = (v * cs + cr * sn + q.axis * (d * (1.0f - cs))) * sz;
			}
			const V bend = q.axis * (sz * 0.5f); // its fold
			const V p0 = q.at - e[0], p1 = q.at + e[0], p2 = q.at + e[1] + bend, p3 = q.at - e[1] + bend;
			const int lit = 150 + (int)(60.0f * sn); // catching the light as it turns
			corner(p0, 30, lit, 45), corner(p1, 30, lit, 45), corner(p2, 40, lit - 40, 50);
			corner(p0, 30, lit, 45), corner(p1, 30, lit, 45), corner(p3, 25, lit - 60, 35);
		}
	}

	// a heart: a puffy red heart standing up, turning about its upright, about 50 tall, lit from above
	// (baked, by the turn), a shine on its upper left lobe; from its outline (the heart curve) and two
	// rings inside it pushed out to the front and back
	void heartCorner(const V& c, float cs, float sn, float x, float y, float z, float nx, float ny, float nz)
	{
		const float k  = 1.7f;
		const float wx = x * cs + z * sn, wz = -x * sn + z * cs;
		const float mx = nx * cs + nz * sn, mz = -nx * sn + nz * cs;
		const float lt = ny * 0.7f + mz * 0.45f + mx * 0.2f;
		const float l  = 0.25f + 0.55f * (lt > 0.0f ? lt : 0.0f);
		const float shine = (ny > 0.55f && nz * nz < 0.6f) ? 0.35f : 0.0f;
		corner(V(c.x + wx * k, c.y + y * k, c.z + wz * k), (int)(255.0f * (l + 0.35f) + 255.0f * shine), (int)(50.0f * l + 230.0f * shine),
		       (int)(60.0f * l + 230.0f * shine));
	}
	static void heartOutline(float t, float* x, float* y, float* nx, float* ny)
	{
		const float st = sinf(t);
		*x = 16.0f * st * st * st;
		*y = 13.0f * cosf(t) - 5.0f * cosf(2.0f * t) - 2.0f * cosf(3.0f * t) - cosf(4.0f * t);
		const float dx = 48.0f * st * st * cosf(t), dy = -13.0f * st + 10.0f * sinf(2.0f * t) + 6.0f * sinf(3.0f * t) + 4.0f * sinf(4.0f * t);
		const float l  = sqrtf(dx * dx + dy * dy) + 1e-4f;
		*nx = dy / l, *ny = -dx / l;
	}
	void drawHeart(const V& c, float turn)
	{
		const float cs = cosf(turn), sn = sinf(turn);
		const int kAround = 28;
		const float ring[4] = {1.0f, 0.72f, 0.38f, 0.0f}, deep[4] = {0.0f, 0.62f, 0.9f, 1.0f};
		for (int side = -1; side <= 1; side += 2)
			for (int band = 0; band < 3; band++)
				for (int j = 0; j < kAround; j++) {
					float px[2][2], py[2][2], pz[2][2], qx[2][2], qy[2][2], qz[2][2];
					for (int a = 0; a < 2; a++) {
						const float t = 6.2831853f * (float)(j + a) / kAround;
						float x, y, nx, ny;
						heartOutline(t, &x, &y, &nx, &ny);
						for (int k = 0; k < 2; k++) {
							const int b2     = band + k;
							const float f    = ring[b2], flat = 1.0f - deep[b2];
							px[a][k] = x * f, py[a][k] = y * f + 2.0f * (1.0f - f), pz[a][k] = deep[b2] * 9.0f * side;
							qx[a][k] = nx * flat, qy[a][k] = ny * flat, qz[a][k] = deep[b2] * side;
						}
					}
					const int order[6][2] = {{0, 0}, {0, 1}, {1, 1}, {0, 0}, {1, 1}, {1, 0}};
					for (int o = 0; o < 6; o++) {
						const int a = order[o][0], k = order[o][1];
						heartCorner(c, cs, sn, px[a][k], py[a][k], pz[a][k], qx[a][k], qy[a][k], qz[a][k]);
					}
				}
	}

	// each tick: the balloons, their pieces and the hearts drawn (api->models)
	void looksDraw()
	{
		if (!hasModels())
			return;
		lookCount = 0;
		balloonTick++;
		for (int p = 0; p < 2; p++)
			if (balloons[p].on)
				drawBalloon(balloons[p]);
		drawPieces();
		for (int i = 0; i < 8; i++) {
			const Heart& h = heartList[i];
			if (h.life <= 0.0f || (h.life < 90.0f && ((int)(h.life / 3.0f) & 1))) // its last 3 seconds: blinking
				continue;
			const float bob = h.landed ? 6.0f * sinf(h.spin * 2.5f) : 0.0f;
			drawHeart(V(h.at.x, h.at.y + bob, h.at.z), h.spin);
		}
		if (lookCount >= 3)
			api->models->draw_triangles(looks, lookCount);
	}

	// ---------------------------------------------------------------- rocket jumps, pops

	// The bazooka's burst near Mario (first person, rocket jumps on) throws him, as in Team Fortress 2:
	// straight away from it (a burst behind him throws him forward), harder nearer; a burst at the
	// wall beside his feet throws him up it; one at his feet forward faster, the way he goes. He steers
	// in the air (rocketTick) and the landing never stuns. With hurting on, a point of health and a cry.
	// (Player 1's: the game's player table is his.)
	void rocketJump(int player, const V& at, float r, const Settings& s, bool noHurt)
	{
		if (player != 0 || s.selfJump <= 0.0f || !s.inView)
			return;
		SmsPlayer m = SmsPlayer();
		m.size      = sizeof m;
		if (!api->player->get(&m) || !m.valid || m.on_yoshi || m.in_water)
			return;
		const float reach = r > 400.0f ? r : 400.0f; // far enough to reach him from the ground at a jump's top
		const V pos(m.position);
		float dx = pos.x - at.x, dy = pos.y + 60.0f - at.y, dz = pos.z - at.z; // from his middle
		const float dist = sqrtf(dx * dx + dy * dy + dz * dz);
		if (dist > reach)
			return;
		if (dist > 1.0f)
			dx /= dist, dy /= dist, dz /= dist;
		else
			dx = 0.0f, dy = 1.0f, dz = 0.0f;
		const bool hasGravity = m.size >= offsetof(SmsPlayer, gravity) + sizeof(float);
		const float g     = hasGravity && m.gravity > 0.05f ? m.gravity : 0.8f;
		const float strength = s.selfJump < 4.0f ? s.selfJump : 4.0f;
		const float speed = sqrtf(2.0f * g * 1500.0f) * (1.0f - 0.5f * dist / reach) * strength; // the rocket nozzle's height, 1500
		float up          = dy * speed;
		if (up < speed * 0.3f) // a burst beside or above him still lifts him a little
			up = speed * 0.3f;
		// up a wall: firing at its foot (or at the wall below him in the air) throws him straight up it
		bool wallJump = false;
		if (at.y < pos.y + 60.0f && SHOTS_HAS(api->game, SmsGameApi, wall_at)) {
			const float hx = at.x - pos.x, hz = at.z - pos.z;
			SmsVec3 n;
			const SmsVec3 waist = V(pos.x, pos.y + 50.0f, pos.z).s();
			if (hx * hx + hz * hz < 260.0f * 260.0f && api->game->wall_at(&waist, 90.0f, &n)) {
				const float nl = sqrtf(n.x * n.x + n.z * n.z);
				if (nl > 0.5f && (hx * n.x + hz * n.z) / nl < 40.0f) { // the burst at the wall's side of him
					wallJump = true;
					up = speed * 0.95f;
					dx = n.x / nl * 0.12f, dz = n.z / nl * 0.12f; // only a little away from it: up it
				}
			}
		}
		// the way he looks across the ground (the view's)
		const float face = m.yaw * (3.14159265f / 180.0f);
		float fx = sinf(face), fz = cosf(face);
		SmsVec3 eye, look;
		if (SHOTS_HAS(api->held_fludd, SmsHeldFluddApi, aim) && api->held_fludd->aim(&eye, &look)
		    && look.x * look.x + look.z * look.z > 0.01f) {
			const float l = sqrtf(look.x * look.x + look.z * look.z);
			fx = look.x / l, fz = look.z / l;
		}
		const float along = (dx * fx + dz * fz) * speed;                        // its push the way he looks
		const float sx = dx * speed - along * fx, sz = dz * speed - along * fz; // and to the side
		// a burst at his feet (near, not in front of him): thrown forward faster, the way he goes
		float feetX = 0.0f, feetZ = 0.0f;
		if (s.feetForward > 0.0f && !wallJump && at.y < pos.y + 60.0f) {
			float gx = sinf(face), gz = cosf(face);
			if (m.forward_speed < -1.0f) // going backwards: that way
				gx = -gx, gz = -gz;
			const float bx = at.x - pos.x, bz = at.z - pos.z;
			const float hd = sqrtf(bx * bx + bz * bz), kFeet = 220.0f;
			if (hd < kFeet && bx * gx + bz * gz < 40.0f) { // under him or behind, not in front
				const float k = speed * s.feetForward * (1.0f - 0.6f * hd / kFeet);
				feetX = gx * k, feetZ = gz * k;
			}
		}
		const bool inAir = (m.status & kStatusJumping) != 0;
		if (!inAir || (wallJump && m.status == kStatusWallSlide))
			api->player->set_status(kStatusJump, 0); // (off a wall slide too: up, not sliding down)
		// across the ground: the way he goes now and the push
		const float hx = sinf(face) * m.forward_speed + along * fx + sx + feetX;
		const float hz = cosf(face) * m.forward_speed + along * fz + sz + feetZ;
		const float h  = sqrtf(hx * hx + hz * hz);
		SmsVec3 v = V(hx, (inAir && m.velocity.y > 0.0f ? m.velocity.y : 0.0f) + up, hz).s();
		api->player->set_velocity(&v);
		rocketAir = true;
		rocketVX  = hx, rocketVZ = hz;
		rocketTop = h > 30.0f ? h : 30.0f;
		steer(h, hx, hz);
		if (s.hurt && !noHurt) {
			api->player->set_health(m.health - 1);
			if (s.cry && SHOTS_HAS(api->audio, SmsAudioApi, play_clip)) {
				// one of Allebale's picks at random; low on health (3 or less, as the game's own "danger") a weak one
				const bool low       = m.health - 1 <= 3;
				const VoiceClip* set = low ? kLowClips : kHurtClips;
				const int n          = low ? (int)(sizeof kLowClips / sizeof *kLowClips) : (int)(sizeof kHurtClips / sizeof *kHurtClips);
				int pick             = (int)(random01() * n);
				pick                 = pick < n ? pick : n - 1;
				api->audio->play_clip(set[pick].pcm, set[pick].frames, set[pick].rate, s.cryVolume);
			}
		}
		api->core->log(mod, SMS_LOG_INFO, "rocket jump: %.0f away%s%s%s, up %.1f, across %.1f%s", dist, inAir ? " (in the air)" : "",
		               wallJump ? " up a wall" : "", feetX != 0.0f || feetZ != 0.0f ? " off the feet" : "", up, h,
		               s.hurt && !noHurt ? ", 1 health lost" : noHurt ? ", no health lost (an enemy hit)" : "");
	}

	// a new stage: no rocket jump going on, no pops (the game forgets its model sizes too)
	void stageReset()
	{
		for (int i = 0; i < 8; i++)
			heartList[i].life = 0.0f;
		for (int i = 0; i < 12; i++)
			pieces[i].life = 0.0f;
		balloons[0].on = balloons[1].on = false;
		rocketAir    = false;
		specialCount = 0;
		specialsAt   = -10.0;
		for (int i = 0; i < 8; i++)
			pops[i].actor = 0;
		for (int i = 0; i < kMostHits; i++)
			hits[i] = Hit();
	}

	// his facing and forward speed from a speed across the ground
	void steer(float h, float hx, float hz)
	{
		if (h > 0.5f) {
			api->player->set_yaw(atan2f(hx, hz) * (180.0f / 3.14159265f));
			api->player->set_forward_speed(h);
		} else
			api->player->set_forward_speed(0.0f);
	}

	// Each tick: Mario up from a rocket jump steers freely (the stick pushes his speed across the ground
	// its way, never faster than the jump threw him) and his fall counts from where he is. With the
	// rocket jump keys held (`keys`) he never slides down a wall: he keeps flying as from a jump, so a
	// burst at the wall below throws him on up it.
	void rocketTick(float dt, bool keys)
	{
		if (!rocketAir && !keys)
			return;
		SmsPlayer m = SmsPlayer();
		m.size      = sizeof m;
		if (!api->player->get(&m) || !m.valid) {
			rocketAir = false;
			return;
		}
		if (keys && m.status == kStatusWallSlide) {
			api->player->set_status(kStatusJump, 0);
			SmsVec3 v = m.velocity; // (the jump's own push up not added)
			api->player->set_velocity(&v);
		}
		if (!rocketAir)
			return;
		if (!(m.status & kStatusJumping) || m.in_water) {
			rocketAir = false;
			return;
		}
		if (SHOTS_HAS(api->player, SmsPlayerApi, set_fall_start))
			api->player->set_fall_start(m.position.y);
		float vx = rocketVX, vz = rocketVZ;
		const bool hasStick = m.size >= offsetof(SmsPlayer, intended_speed) + sizeof(float);
		const float mag     = hasStick ? m.intended_speed : 0.0f;
		if (mag > 0.05f) {
			const float iy = m.intended_yaw * (3.14159265f / 180.0f);
			const float wx = sinf(iy), wz = cosf(iy);
			const float push = 2.4f * mag * dt;
			vx += wx * push, vz += wz * push;
			const float v = sqrtf(vx * vx + vz * vz); // and the way it already goes turned towards the stick's
			if (v > 0.5f) {
				const float k = 0.12f * mag * dt;
				vx += (wx * v - vx) * k, vz += (wz * v - vz) * k;
			}
		}
		const float v = sqrtf(vx * vx + vz * vz);
		if (v > rocketTop)
			vx *= rocketTop / v, vz *= rocketTop / v;
		rocketVX = vx, rocketVZ = vz;
		steer(sqrtf(vx * vx + vz * vz), vx, vz);
	}

	// An enemy shot in the air pops: water bursts out of it, big blobs fly out like pieces (mostly the
	// shot's way), a splash, and its model swells and pops to nothing (popTick)
	void pop(int player, SmsObject e, float dx, float dz)
	{
		int slot = 0;
		for (int i = 0; i < 8; i++)
			if (pops[i].actor == e || !pops[i].actor) {
				slot = i;
				break;
			}
		pops[slot].actor = e;
		pops[slot].start = time;
		SmsVec3 p;
		if (!api->game->object_position(e, &p))
			return;
		const SmsVec3 mid = V(p.x, p.y + 50.0f, p.z).s();
		for (int i = 0; i < 3; i++)
			api->game->particle(kParticleHit, &mid);
		api->game->splash(&mid, 18.0f);
		if (SHOTS_HAS(api->audio, SmsAudioApi, play_sound_at))
			api->audio->play_sound_at(kSoundWaterHit, &mid);
		SmsWaterDrop base = SmsWaterDrop();
		base.size         = sizeof base;
		if (!api->water->nozzle_drop(player, &base))
			return;
		SmsWaterDrop d = SmsWaterDrop();
		d.size         = sizeof d;
		d.hit_radius   = base.hit_radius;
		d.hit_height   = base.hit_height;
		d.alive        = base.alive;
		d.attack       = 1;
		d.flags        = 0x40;
		d.player       = player;
		d.tag          = tagOf(player, PIECES, nextShot++);
		for (int i = 0; i < 14; i++) {
			const float ang = random01() * 6.2831853f, up = 0.35f + 0.55f * random01();
			const V dir     = V(sinf(ang) * (1.0f - up * 0.5f) + dx * 0.8f, up + 0.3f, cosf(ang) * (1.0f - up * 0.5f) + dz * 0.8f).unit();
			d.drop_size     = base.drop_size * (2.5f + 4.0f * random01());
			d.position      = mid;
			d.velocity      = (dir * (14.0f + 16.0f * random01())).s();
			api->water->emit(&d);
		}
	}

	// the popped enemies' models: swell (1 to 1.35 in 4 30ths of a second), then pop to nothing in 2
	// more, and stay so till it is gone (5 seconds, then its own size again for its next life)
	void popTick()
	{
		if (!SHOTS_HAS(api->actor, SmsActorApi, set_model_scale))
			return;
		for (int i = 0; i < 8; i++) {
			Pop& p = pops[i];
			if (!p.actor)
				continue;
			const float t = (float)(time - p.start) * 30.0f;
			if (t > 150.0f) {
				api->actor->set_model_scale(p.actor, 1.0f);
				p.actor = 0;
				continue;
			}
			const float k = t < 4.0f ? 1.0f + 0.35f * t / 4.0f : t < 6.0f ? 1.35f * (1.0f - (t - 4.0f) / 2.0f) : 0.0f;
			api->actor->set_model_scale(p.actor, k > 0.001f ? k : 0.001f);
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
		const bool inAir  = (f & SMS_ACTOR_FLAG_AIRBORNE) != 0;
		// the rocket jump key's shots (the bazooka's): one in the air pops (first person), and with the
		// key held one on the ground is thrown up
		if ((s.keyLaunch & 3) && walks && weapon == BAZOOKA) {
			if ((s.keyLaunch & 2) && fresh && s.inView && inAir) {
				h->lastShot = shot;
				h->hits     = 0;
				api->core->log(mod, SMS_LOG_INFO, "an enemy in the air popped by the bazooka");
				pop(player, e, dx, dz);
				api->actor->defeat(e);
				if (s.hearts)
					dropHeart(e);
				return;
			}
			if (s.keyLaunch & 1) {
				if (!fresh || inAir)
					return; // the rest of a shot that has done its part: no push
				h->lastShot    = shot;
				h->vx = h->vz  = 0.0f; // up: no push along the ground
				h->launchUntil = time + 0.1;
				h->launchUp    = s.keyLift > 0.0f ? s.keyLift : 30.0f;
				SmsVec3 v;
				if (api->actor->velocity(e, &v)) { // only a little of its own speed across the ground kept
					v.x *= 0.4f, v.z *= 0.4f;
					v.y = h->launchUp;
					api->actor->set_velocity(e, &v);
				}
				api->actor->set_flags(e, SMS_ACTOR_FLAG_AIRBORNE, 0);
				api->core->log(mod, SMS_LOG_INFO, "an enemy thrown up by the bazooka (rocket jump key held)");
				return;
			}
		}
		SmsPlayer m = SmsPlayer();
		m.size      = sizeof m;
		SmsVec3 ep;
		if (s.airForward && weapon == BAZOOKA && fresh && walks && inAir && api->player->get(&m) && m.valid
		    && api->game->object_position(e, &ep)) {
			// already in the air: pushed on forward, away from Mario, not thrown up again
			const float fx = ep.x - m.position.x, fz = ep.z - m.position.z, fl = sqrtf(fx * fx + fz * fz);
			if (fl > 1.0f)
				dx = fx / fl, dz = fz / fl;
			h->vx = dx * k.shove;
			h->vz = dz * k.shove;
		} else {
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
		}
		if (fresh)
			api->core->log(mod, SMS_LOG_INFO, "[diag] %s hit: shove %.0f (walks %d, flags %x)", kNames[weapon], walks ? k.shove : 0.0f, walks ? 1 : 0, f);
		if (!fresh)
			return;
		h->lastShot = shot;
		if (!s.defeat)
			return;
		if (time - h->last > kHitWindow * (s.far > 1.0f ? s.far : 1.0f)) // longer at long range
			h->hits = 0;
		h->hits++;
		h->last = time;
		if (h->hits < k.toDefeat)
			api->core->log(mod, SMS_LOG_DEBUG, "an enemy hit by the %s, %d of %d", kNames[weapon], h->hits, k.toDefeat);
		if (h->hits >= k.toDefeat) {
			h->hits = 0;
			api->core->log(mod, SMS_LOG_INFO, "an enemy defeated by the %s", kNames[weapon]);
			api->actor->defeat(e);
			if (weapon == BAZOOKA && s.hearts && s.inView)
				dropHeart(e);
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

	// Goop washed off round a burst: where it burst (goop on a wall too), and spots on the ground in two
	// rings round it, each bigger the more it cleans
	void cleanGoop(const V& at, float r, float strength, bool none)
	{
		if (none)
			return;
		const SmsVec3 c = at.s();
		api->game->clean_goop(&c, r * 0.6f);
		const float spot = r * 0.35f * (strength > 1.0f ? strength : 1.0f);
		for (int ring = 0; ring < 3; ring++) {
			const int count = ring == 0 ? 1 : ring == 1 ? 7 : 13;
			const float rad = ring == 0 ? 0.0f : ring == 1 ? r * 0.42f : r * 0.8f;
			for (int i = 0; i < count; i++) {
				const float a = 6.2831853f * (float)i / (float)count + (float)ring * 0.4f;
				V p(at.x + cosf(a) * rad, at.y, at.z + sinf(a) * rad);
				SmsVec3 hit;
				const SmsVec3 top = V(p.x, at.y + 300.0f, p.z).s(), low = V(p.x, at.y - 800.0f, p.z).s();
				if (api->game->ray(&top, &low, &hit)) // the ground there
					p.y = hit.y;
				const SmsVec3 sp = p.s();
				api->game->clean_goop(&sp, spot);
			}
		}
	}

	// (way: the shot's own way across the ground, for an enemy right in the middle of it)
	void burst(int player, int weapon, uint32_t shot, const V& at, const Weapon& k, const Settings& s, V way = V())
	{
		const float wl = sqrtf(way.x * way.x + way.z * way.z);
		const float wx = wl > 1e-3f ? way.x / wl : 0.0f, wz = wl > 1e-3f ? way.z / wl : 0.0f;
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
			// out from the middle; one in the middle (a burst right on it) pushed on the shot's way
			hitEnemy(seen[i], player, weapon, shot, l > 40.0f ? x / l : wx, l > 40.0f ? z / l : wz);
			inIt++;
		}
		burstOthers(player, weapon, shot, at, r, s);
		cleanGoop(at, s.goopArea > 0.0f ? 860.0f * s.goopArea : r * 1.2f, s.goop, s.noClean);
		explode(player, at, r);
		rocketJump(player, at, r, s, s.enemySafe && inIt > 0);
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
		if (weapon == PIECES)
			return 1; // a popped enemy's pieces: they push no one
		const Settings& s = curSettings(weapon);
		const uint32_t f = api->actor->flags(ev.object);
		const int kd     = kindOf(ev.object);
		if (kd != SMS_KIND_SMALL_ENEMY && kd != SMS_KIND_OTHER)
			otherHitBefore(ev.object, kd, weapon, tagShot(d.tag), s);
		// the bazooka with the rocket jump key held: the game never hears of the hit on a small enemy (its
		// push and its being knocked back after), only the burst here, which throws them up
		if (weapon == BAZOOKA && (s.keyLaunch & 1) && (f & SMS_ACTOR_FLAG_SMALL_ENEMY)) {
			const uint32_t shot = tagShot(d.tag);
			if (!burstDone(shot)) {
				markBurst(shot);
				burst(tagPlayer(d.tag), weapon, shot, V(d.position), cur(weapon), s, V(d.velocity));
			}
			return 1;
		}
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
		if (weapon == PIECES)
			return;
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
		const Weapon& k = cur(weapon);
		const int kd    = kindOf(ev.object);
		if (kd != SMS_KIND_SMALL_ENEMY && kd != SMS_KIND_OTHER) {
			otherHitDone(ev.object, kd, weapon, shot, ev.result, curSettings(weapon));
			if (kd == SMS_KIND_SHADOW_MARIO && k.burst > 0.0f && !burstDone(shot)) { // the bazooka bursts on him
				markBurst(shot);
				burst(player, weapon, shot, V(d.position), k, curSettings(weapon), V(d.velocity));
			}
			return;
		}
		if (!(f & SMS_ACTOR_FLAG_SMALL_ENEMY))
			return;
		if (k.burst > 0.0f) {
			if (!burstDone(shot)) { // once a shot, where it first hits an enemy
				markBurst(shot);
				burst(player, weapon, shot, V(d.position), k, curSettings(weapon), V(d.velocity));
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
		w.lift     = d.lift * (s.lift >= 0.0f ? s.lift : 0.0f) * (s.inView ? 1.0f : 0.5f); // (third person: mostly pushed)
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
