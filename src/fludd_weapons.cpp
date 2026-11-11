// FLUDD Weapons: FLUDD's spray nozzle fires a Rifle, a Submachine gun or a Bazooka.
//
// D-pad left and right choose the weapon (FLUDD's own spray, Rifle, Submachine gun, Bazooka);
// R fires it. In first person (the First Person mod, FLUDD held in view) the weapon's own model is
// held there; in third person (its option) FLUDD stays on Mario's back, or is drawn as the
// weapon's pack. The Rifle zooms on D-pad up, with a sniper scope; from third person that goes
// into first person and back. The Bazooka's burst throws enemies, and Mario too in first person
// (a rocket jump). Long range: enemies and characters drawn and moving further away, to hit there.
//
// How: the game does the firing (api->held_fludd's set_weapon and set_weapon_stats); this mod
// chooses, keeps R from the game's own spray while a weapon fires, and draws the scope and the
// weapon's name. It and the First Person mod read each other through held_fludd's get_state.
#include <sms_mod.hpp>
#include "recommended_preset.h"
#include "shots.h"

// The weapons made for bosses and the bazooka pushing enemies in the air forward (set_weapon's
// flags): coming in the next game, after SDK 1.7.0; an older game leaves the flags unused
#ifndef SMS_SPRAY_BOSSES
#define SMS_SPRAY_BOSSES 32
#endif
#ifndef SMS_SPRAY_AIR_FORWARD
#define SMS_SPRAY_AIR_FORWARD 64
#endif
#ifndef SMS_SPRAY_SHOT_WATER
#define SMS_SPRAY_SHOT_WATER 128   // the bazooka's shot seen flying: a streak of water
#define SMS_SPRAY_SHOT_BALLOON 256 // ...or a green water balloon that pops where it bursts
#endif
#ifndef SMS_SPRAY_SHOT_BIG
#define SMS_SPRAY_SHOT_BIG 512     // ...a big one
#endif
#ifndef SMS_SPRAY_ROCKET_HURT
#define SMS_SPRAY_ROCKET_HURT 1024 // each rocket jump costs Mario a point of health
#endif
#ifndef SMS_SPRAY_HEARTS
#define SMS_SPRAY_HEARTS 2048      // defeated enemies drop a heart (a point of health)
#define SMS_SPRAY_ROCKET_OUCH 4096 // Mario cries out as a rocket jump hurts him
#endif

#include <math.h>
#include <string>
#include <stddef.h>

namespace {

// the game's FLUDD weapons: api->held_fludd (Allebale's First Person and Remastered builds), taken
// only if its table says so (its magic)
const SmsHeldFluddApi* heldFluddApi(const SmsApi& api)
{
	if (api.size < offsetof(SmsApi, held_fludd) + sizeof(void*) || !api.held_fludd) // SMS_API_HAS
		return nullptr;
	const SmsHeldFluddApi* h = api.held_fludd;
	return h->size >= offsetof(SmsHeldFluddApi, set_weapon) + sizeof(void*) && h->magic == SMS_HELD_FLUDD_MAGIC
	               && h->set_weapon
	           ? h
	           : nullptr;
}
bool has(const SmsHeldFluddApi* h, size_t end) { return h && h->size >= end; }
#define HELD_HAS(h, field) has(h, offsetof(SmsHeldFluddApi, field) + sizeof(void*))

const char* const kWeaponNames[] = {"FLUDD", "Rifle", "Submachine gun", "Bazooka", "Fire goop", "Electric goop", "Black goop"};
const int kWeaponIds[]           = {SMS_SPRAY_NORMAL, SMS_SPRAY_RIFLE, SMS_SPRAY_SMG, SMS_SPRAY_BAZOOKA, SMS_SPRAY_FIRE_GOOP,
                                    SMS_SPRAY_ELECTRIC_GOOP, SMS_SPRAY_BLACK_GOOP};
const int kWeaponCount           = 7;
// the keyboard keys a weapon can be chosen with (the menu's choices after None)
const char* const kNumberKeys[]   = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
const int kKeyDefaults[] = {1, 5, 4, 3, 7, 6, 2}; // each weapon's key (the menu's choice: 1..9, 10 for 0)
const char* const kKeyChoices[]   = {"None", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
// the goop weapons: FLUDD's own stream, as fire or electric goop (R left to the game)
bool isGoop(int w) { return w == SMS_SPRAY_FIRE_GOOP || w == SMS_SPRAY_ELECTRIC_GOOP || w == SMS_SPRAY_BLACK_GOOP; }

// long range: how much further than the game's own distance enemies and characters are drawn and
// move (the "long_range" setting)
const char* const kRanges[] = {"Off", "Low", "Medium", "High", "Very high", "Insane"};
const float kRangeScale[]   = {1.0f, 1.5f, 2.0f, 3.0f, 5.0f, 10.0f};
const int kRangeCount       = 6;

// Shadow Mario: how much of his water meter each weapon hit takes, times the game's own (the
// "shadow_mario" setting)
const char* const kShadowLevels[] = {"Very easy", "Easy", "Normal", "Hard", "Very hard", "Extreme"};
const float kShadowDamage[]       = {2.0f, 1.4f, 1.0f, 0.6f, 0.35f, 0.2f};
const int kShadowCount            = 6;
// Rocket jumps: never, always, or only while the rocket jump key is held (as Team Fortress 2's
// crouch), so shots at enemies close by do not throw Mario
const char* const kRocketModes[] = {"Off", "Always", "While the key is held"};
const int kRocketModeCount       = 3;
const char* const kRocketKeys[]  = {"Left Ctrl", "Left Shift"}; // SDL key names: the keys themselves, whatever they do in the game
const int kRocketKeyCount        = 2;
const char* const kProjectiles[] = {"Water", "Green water balloon", "Big green water balloon"}; // the bazooka's shot seen flying
const int kProjectileCount       = 3;

// Split-screen co-op (held_fludd's select_player, First Person beta 2026.11.10): two players
bool hasPlayers(const SmsHeldFluddApi* h)
{
	return HELD_HAS(h, player_view) && h->players && h->player && h->select_player && h->player_view;
}

} // namespace

struct FluddWeapons : sms::Mod {
	using sms::Mod::Mod;

	RecommendedPreset preset{*this}; // the "Settings preset" row: Recommended (the defaults) or Custom
	shots::Engine eng; // the shots, hits and bursts (the mod API's water drops and actors)
	bool playing = true; // the stage's opening is over (SMS_EV_STAGE_START): the player has the controls
	                     // (true as the mod loads: loaded in play)
	// shots only while the player plays: not in a stage's opening, a cutscene or a talk
	bool canFire() const
	{
		if (!playing)
			return false;
		const SmsWorldApi* wd = api().size >= offsetof(SmsApi, world) + sizeof(void*) ? api().world : nullptr;
		return !(wd && wd->cutscene_playing && wd->cutscene_playing());
	}
	// the shots this mod fires itself (a game with api->water and api->actor); else the game's own
	bool ownShots() const { return eng.ready(); }
	static bool shotWeapon(int w) { return w == SMS_SPRAY_RIFLE || w == SMS_SPRAY_SMG || w == SMS_SPRAY_BAZOOKA; }

	// Each player's (co-op: two, each with their own controller; alone, the first)
	struct Player {
		int weapon    = 0;     // the weapon chosen (kWeaponNames)
		bool numberWasDown[kWeaponCount] = {};
		bool rHeld    = false; // R, as the controller has it (kept from the game while a weapon fires)
		bool zoomed   = false; // the Rifle's zoom
		bool zoomFromThird = false; // zoomed from third person (into first person for it)
		bool sawFirst = false;      // ...and first person has come on for it
		double zoomedAt  = 0;       // when the zoom came on (the scope comes in with it)
		bool upWasDown = false, rightWasDown = false, leftWasDown = false;
		int lastSet    = -1; // the weapon and flags last set, -1 none
	};
	Player ps[2];
	int cur = 0; // the player being worked on
	Player& P() { return ps[cur]; }
	const Player& P() const { return ps[cur]; }
	bool told      = false, toldNone = false;
	float lastFar  = 1.0f;
	double toastUntil = 0;
	const char* toastText = "";
	int toastPlayer = 0;

	// co-op: the players, and the game's api acting on one (-1: the one the game runs as)
	int players() const
	{
		const SmsHeldFluddApi* h = heldFluddApi(api());
		return hasPlayers(h) && h->players() == 2 ? 2 : 1;
	}
	int playerNow() const
	{
		const SmsHeldFluddApi* h = heldFluddApi(api());
		return hasPlayers(h) && h->player() == 1 ? 1 : 0;
	}
	void select(int player) const
	{
		const SmsHeldFluddApi* h = heldFluddApi(api());
		if (hasPlayers(h))
			h->select_player(player);
	}
	struct For {
		FluddWeapons& m;
		int saved;
		For(FluddWeapons& mod, int p) : m(mod), saved(mod.cur)
		{
			m.cur = p;
			m.select(p);
		}
		~For()
		{
			m.cur = saved;
			m.select(-1);
		}
	};

	// what the First Person mod has set (first person on, FLUDD held in view)
	SmsHeldFluddState state() const
	{
		SmsHeldFluddState s = SmsHeldFluddState();
		s.size              = sizeof s;
		const SmsHeldFluddApi* h = heldFluddApi(api());
		if (!HELD_HAS(h, get_state) || !h->get_state || !h->get_state(&s))
			s = SmsHeldFluddState();
		return s;
	}

	int chosen() const { return P().weapon >= 0 && P().weapon < kWeaponCount && allowed(P().weapon) ? kWeaponIds[P().weapon] : SMS_SPRAY_NORMAL; }
	// a weapon turned off in the menu: not chosen at all (FLUDD always can be)
	bool allowed(int w) const
	{
		if (w <= 0 || w >= kWeaponCount)
			return true;
		char key[32];
		snprintf(key, sizeof key, "weapon_on_%d", w);
		return setting(key, 1) != 0;
	}

	// Holding on to something (a fence, a ladder, a bar, a wire, a ceiling's net): no weapon.
	bool holding() const
	{
		SmsPlayer m = SmsPlayer();
		m.size      = sizeof m;
		if (!api().player->get(&m))
			return false;
		return (m.status & 0x30000000u) != 0 || (m.status & 0x200000u) != 0;
	}

	// first person: FLUDD held in view (the First Person mod); third person: its option
	bool firstPerson() const { return state().first_person != 0; }
	bool thirdPersonWeapons() const { return !firstPerson() && setting("third_person_weapons", 1); }
	bool weaponsHere() const
	{
		if (!setting("weapons", 1))
			return false;
		const SmsHeldFluddState s = state();
		if (!s.first_person)
			return setting("third_person_weapons", 1) != 0;
		if (s.held)
			return true;
		// under the sea with plain FLUDD (its jet pack) it is on his back, not held: the weapons to
		// choose from all the same
		SmsPlayer m = SmsPlayer();
		m.size      = sizeof m;
		return api().player->get(&m) && m.has_fludd && !m.on_yoshi && m.current_nozzle == 2;
	}

	// the weapon that fires now: with FLUDD held (or the third person option) and its spray nozzle out
	bool onCoaster = false; // Pinna Park's roller coaster (Mecha-Bowser's: area 0x3A, stage 0 or 1)

	int weaponNow() const
	{
		if (!weaponsHere() || chosen() == SMS_SPRAY_NORMAL || holding() || onCoaster)
			return 0; // (on Pinna Park's roller coaster: its own rockets, FLUDD's as ever)
		SmsPlayer m = SmsPlayer();
		m.size      = sizeof m;
		if (!api().player->get(&m) || !m.has_fludd || m.on_yoshi || m.current_nozzle != 0)
			return 0;
		return chosen();
	}

	void toast(const char* text)
	{
		toastText   = text;
		toastUntil  = core().time() + 1.5;
		toastPlayer = cur;
	}

	// D-pad left and right: the weapon (each player's own controller; the number keys player 1's)
	void switchWeapon(const SmsPad& p)
	{
		Player& pl = P();
		int& weapon = pl.weapon;
		const bool right = (p.held & SMS_PAD_RIGHT) != 0, left = (p.held & SMS_PAD_LEFT) != 0;
		const bool newRight = right && !pl.rightWasDown, newLeft = left && !pl.leftWasDown;
		pl.rightWasDown = right;
		pl.leftWasDown  = left;
		bool* numberWasDown = pl.numberWasDown;
		// or a number key: each weapon's own (the menu's), as it goes down
		int byKey = -1;
		for (int w = 0; w < kWeaponCount && cur == 0; w++) {
			char key[32];
			snprintf(key, sizeof key, "weapon_key_%d", w);
			const int k  = setting(key, kKeyDefaults[w]); // 0 none, 1..9 the keys 1 to 9, 10 the key 0
			const int sc = k > 0 && k <= 10 ? input().key_from_name(kNumberKeys[k - 1]) : 0;
			const bool down = sc > 0 && input().key_down(sc);
			if (down && !numberWasDown[w] && allowed(w))
				byKey = w;
			numberWasDown[w] = down;
		}
		if (!weaponsHere() || (!newRight && !newLeft && byKey < 0))
			return;
		if (byKey >= 0)
			weapon = byKey;
		else
			for (int n = 0; n < kWeaponCount; n++) { // the next one on (those turned off skipped)
				weapon = (weapon + (newRight ? 1 : kWeaponCount - 1)) % kWeaponCount;
				if (allowed(weapon))
					break;
			}
		if (pl.zoomed && !pl.zoomFromThird)
			pl.zoomed = false;
		toast(kWeaponNames[weapon]);
		info("%sweapon: %s", cur ? "player 2's " : "", kWeaponNames[weapon]);
	}

	// D-pad up: the Rifle's zoom on and off in first person (FLUDD in view); in third person with
	// its weapons, into first person zoomed (the First Person mod switches) and back out again
	bool canZoom() const
	{
		const SmsHeldFluddState s = state();
		return setting("weapons", 1) && chosen() == SMS_SPRAY_RIFLE && s.first_person && s.held;
	}
	void toggleZoom(const SmsPad& p)
	{
		Player& pl = P();
		bool& zoomed = pl.zoomed;
		bool& zoomFromThird = pl.zoomFromThird;
		bool& sawFirst = pl.sawFirst;
		double& zoomedAt = pl.zoomedAt;
		bool& upWasDown = pl.upWasDown;
		const bool fp = firstPerson();
		if (zoomFromThird && fp)
			sawFirst = true;
		if (zoomFromThird && sawFirst && !fp) // left first person some other way
			zoomed = zoomFromThird = sawFirst = false;
		if (zoomed && !zoomFromThird && !canZoom())
			zoomed = false;
		const bool down  = (p.held & SMS_PAD_UP) != 0;
		const bool fresh = down && !upWasDown; // once a press
		upWasDown        = down;
		if (!fresh)
			return;
		if (zoomFromThird) { // back to third person
			zoomed = zoomFromThird = sawFirst = false;
			info("rifle zoom off, back to third person");
			return;
		}
		if (!fp && chosen() == SMS_SPRAY_RIFLE && thirdPersonWeapons() && weaponNow() == SMS_SPRAY_RIFLE) {
			zoomed = zoomFromThird = true; // into first person for it
			sawFirst = false;
			zoomedAt = core().time();
			info("rifle zoom on, from third person");
			return;
		}
		if (!canZoom())
			return;
		zoomed   = !zoomed;
		zoomedAt = core().time();
		info("rifle zoom %s", zoomed ? "on" : "off");
	}

	// Rocket jumps now: the setting, and with "While the key is held" the key (Left Ctrl or Left Shift)
	// Left Ctrl, Left Shift or a controller's L held: Mario's own voice muted (his rocket jump's cries alone)
	bool muteKeysHeld() const
	{
		const SmsPad p = pad(cur);
		if ((p.held & SMS_PAD_L) || p.trigger_l >= 0.5f)
			return true;
		for (int k = 0; k < kRocketKeyCount && cur == 0; k++) { // (the keyboard: player 1's)
			const int sc = input().key_from_name(kRocketKeys[k]);
			if (sc > 0 && input().key_down(sc))
				return true;
		}
		return false;
	}

	// The rocket jump key's shots (a setting, off by default): with it on, shots on walking enemies in
	// the air defeat them (2), and while the key is held shots throw those on the ground up (1). The
	// keyboard's key is player 1's; a controller's L anyone's.
	int keyLaunchNow() const
	{
		if (!setting("rocket_key_launch", 1))
			return 0;
		const SmsPad p = pad(cur);
		if ((p.held & SMS_PAD_L) || p.trigger_l >= 0.5f)
			return 3;
		if (cur != 0)
			return 2;
		const int k  = setting("rocket_jump_key", 1);
		const int sc = input().key_from_name(kRocketKeys[k >= 0 && k < kRocketKeyCount ? k : 0]);
		return sc > 0 && input().key_down(sc) ? 3 : 2;
	}

	bool rocketJumpOn() const
	{
		const int mode = setting("rocket_jump", 2);
		if (mode != 2)
			return mode == 1;
		const int k  = setting("rocket_jump_key", 1);
		const int sc = input().key_from_name(kRocketKeys[k >= 0 && k < kRocketKeyCount ? k : 0]);
		return sc > 0 && input().key_down(sc);
	}

	// A weapon's numbers from the menu for this mod's own shots (the same sendStats gives the game)
	shots::Settings shotSettings(int w) const
	{
		shots::Settings s = shots::Settings();
		const SmsHeldFluddState seen = state();
		const bool inView = seen.first_person && seen.held;
		s.defeat = setting("weapons_defeat", 1) != 0;
		s.third  = setting("third_person_weapons", 1) != 0;
		s.inView = inView;
		const char* goopKey = w == SMS_SPRAY_RIFLE ? "rifle_goop" : w == SMS_SPRAY_SMG ? "smg_goop" : "bazooka_goop";
		s.goop     = (float)setting(goopKey, w == SMS_SPRAY_BAZOOKA ? 400 : 100) / 100.0f;
		s.goopArea = w == SMS_SPRAY_BAZOOKA ? (float)setting("bazooka_goop_area", 400) / 100.0f : 0.0f;
		s.noClean  = w == SMS_SPRAY_BAZOOKA && muteKeysHeld();
		s.hitSize = 1.0f;
		s.dropSize = 1.0f;
		s.range = 1.0f;
		s.lift = 0.0f;
		s.burst = 1.0f;
		if (w == SMS_SPRAY_RIFLE) {
			s.every    = (float)setting("rifle_rate", 13) / 10.0f;
			s.push     = (float)setting("rifle_push", 300) / 100.0f;
			s.toDefeat = setting("rifle_defeat", 2);
			s.water    = setting("rifle_water", 16);
			s.hitSize  = (float)setting("rifle_hit", 100) / 100.0f;
			s.dropSize = (float)setting(P().zoomed && inView ? "rifle_blob_scope" : inView ? "rifle_blob" : "rifle_blob_third", P().zoomed && inView ? 20 : 80) / 100.0f;
		} else if (w == SMS_SPRAY_SMG) {
			s.every    = (float)setting("smg_rate", 14) / 100.0f;
			s.push     = (float)setting("smg_push", 90) / 100.0f;
			s.toDefeat = setting("smg_defeat", 6);
			s.water    = setting("smg_water", 16);
			s.hitSize  = (float)setting("smg_hit", 100) / 100.0f;
			s.dropSize = (float)setting("smg_blob", 20) / 100.0f;
		} else if (w == SMS_SPRAY_BAZOOKA) {
			const bool jump = rocketJumpOn();
			s.every    = (float)setting("bazooka_rate", 8) / 10.0f;
			s.range    = (float)setting("bazooka_range", 100) / 100.0f;
			s.toDefeat = setting("bazooka_defeat", 3);
			s.water    = setting("bazooka_water", 60);
			s.burst    = (float)setting(inView && jump ? "bazooka_burst_jump" : inView ? "bazooka_burst" : "bazooka_burst_third", inView ? 130 : 400) / 100.0f;
			s.push     = (float)setting(inView ? "bazooka_push" : "bazooka_push_third", inView ? 200 : 800) / 100.0f;
			s.lift     = (float)setting("bazooka_lift", 40) / 100.0f;
		}
		return s;
	}

	// The weapons' numbers from the menu, to the game.
	void sendStats(const SmsHeldFluddApi* h)
	{
		if (!HELD_HAS(h, set_weapon_stats) || !h->set_weapon_stats)
			return;
		const int r       = setting("long_range", 5);
		const float far   = kRangeScale[r >= 0 && r < kRangeCount ? r : 0];
		if (far != lastFar) {
			info("long range: %s (%.1f times as far)", kRanges[r >= 0 && r < kRangeCount ? r : 0], far);
			lastFar = far;
		}
		const int launch = keyLaunchNow(); // (air shots and the rocket jump key's shots, for every weapon)
		const int sh      = setting("shadow_mario", 5);
		const float boss  = kShadowDamage[sh >= 0 && sh < kShadowCount ? sh : 2];
		SmsSprayStats ns  = SmsSprayStats(); // FLUDD's own spray: only its time to defeat
		ns.size           = sizeof ns;
		ns.every          = (float)setting("normal_defeat_time", 4) / 10.0f;
		ns.far_scale      = far;
		ns.shadow_damage = boss;
		ns.key_launch    = launch;
		h->set_weapon_stats(SMS_SPRAY_NORMAL, &ns);

		SmsSprayStats rf = SmsSprayStats(); // the Rifle: one big, fast blob of water, dead straight
		rf.size          = sizeof rf;
		rf.every         = (float)setting("rifle_rate", 13) / 10.0f;
		rf.push          = (float)setting("rifle_push", 300) / 100.0f;
		rf.range         = 1.0f;
		rf.to_defeat     = setting("rifle_defeat", 2);
		rf.water         = setting("rifle_water", 16);
		rf.hit_size      = (float)setting("rifle_hit", 100) / 100.0f;
		// its blob's size: zoomed (the scope), else first or third person's
		const SmsHeldFluddState seen = state();
		const bool inView  = seen.first_person && seen.held;
		rf.drop_size     = (float)setting(P().zoomed && inView ? "rifle_blob_scope" : inView ? "rifle_blob" : "rifle_blob_third", P().zoomed && inView ? 20 : 80) / 100.0f;
		rf.burst         = 1.0f;
		rf.view_distance = (float)setting("rifle_distance", 50);
		rf.far_scale     = far;
		rf.shadow_damage = boss;
		rf.key_launch    = launch;
		h->set_weapon_stats(SMS_SPRAY_RIFLE, &rf);

		SmsSprayStats sm = SmsSprayStats(); // the Submachine gun: the Rifle's kind of shot, faster and softer
		sm.size          = sizeof sm;
		sm.every         = (float)setting("smg_rate", 14) / 100.0f;
		sm.push          = (float)setting("smg_push", 90) / 100.0f;
		sm.range         = 1.0f;
		sm.to_defeat     = setting("smg_defeat", 6);
		sm.water         = setting("smg_water", 16);
		sm.hit_size      = (float)setting("smg_hit", 100) / 100.0f;
		sm.drop_size     = (float)setting("smg_blob", 20) / 100.0f;
		sm.burst         = 1.0f;
		sm.view_distance = (float)setting("smg_distance", 45);
		sm.far_scale     = far;
		sm.shadow_damage = boss;
		sm.key_launch    = launch;
		h->set_weapon_stats(SMS_SPRAY_SMG, &sm);

		// the Bazooka: a burst where it lands (an older game reads only the stats it knows)
		SmsSprayStats bz = SmsSprayStats();
		bz.size          = sizeof bz;
		bz.every         = (float)setting("bazooka_rate", 8) / 10.0f;
		bz.range         = (float)setting("bazooka_range", 100) / 100.0f;
		bz.to_defeat     = setting("bazooka_defeat", 3);
		bz.water         = setting("bazooka_water", 60);
		bz.hit_size      = 1.0f;
		bz.drop_size     = 1.0f;
		// its burst's size: rocket jumping (first person, rocket jumps on now), else first or third person's
		const SmsHeldFluddState view = state();
		const bool held  = view.first_person && view.held;
		const bool jump  = rocketJumpOn();
		bz.burst         = (float)setting(held && jump ? "bazooka_burst_jump" : held ? "bazooka_burst" : "bazooka_burst_third", held ? 130 : 400) / 100.0f;
		bz.push          = (float)setting(held ? "bazooka_push" : "bazooka_push_third", held ? 200 : 800) / 100.0f;
		bz.lift          = (float)setting("bazooka_lift", 40) / 100.0f;
		bz.self_jump     = jump ? (float)setting("rocket_jump_power", 130) / 100.0f : 0.0f;
		bz.view_distance = (float)setting("bazooka_distance", 55);
		bz.far_scale     = far;
		bz.shadow_damage = boss;
		bz.goop_area     = (float)setting("bazooka_goop_area", 400) / 100.0f;
		bz.key_launch    = launch;
		bz.feet_forward  = (float)setting("rocket_jump_feet", 50) / 100.0f;
		bz.key_lift      = 30.0f * (float)setting("rocket_key_launch_height", 80) / 100.0f;
		h->set_weapon_stats(SMS_SPRAY_BAZOOKA, &bz);
		// the goop weapons: seconds to defeat an enemy, how long the goop lasts (in minutes), its puddles' size
		for (int g = 0; g < 3; g++) {
			const char* const pre = g == 2 ? "black_goop" : g ? "electric_goop" : "fire_goop";
			std::string key(pre);
			SmsSprayStats gs = SmsSprayStats();
			gs.size          = sizeof gs;
			// (the defaults: fire, electric, black)
			static const int kDefeat[3] = {5, 12, 12}, kLife[3] = {20, 20, 90}, kSize[3] = {130, 130, 160};
			static const int kHits[3] = {3, 1, 1}, kWater[3] = {250, 200, 200};
			gs.every         = (float)setting((key + "_defeat").c_str(), kDefeat[g]) / 10.0f;
			gs.range         = (float)setting((key + "_life").c_str(), kLife[g]) / 60.0f;
			gs.burst         = (float)setting((key + "_size").c_str(), kSize[g]) / 100.0f;
			// black goop: how fast; fire goop: 2 for the goop performance mode (the game's drawing of
			// every goop simpler)
			gs.push          = g == 2 ? (float)setting("black_goop_speed", 500) / 100.0f
			                          : g == 0 && setting("goop_performance", 0) ? 2.0f : 1.0f;
			gs.to_defeat     = setting((key + "_hits").c_str(), kHits[g]);
			gs.hit_size      = (float)setting("goop_wall_life", 15) / 60.0f; // its goop on walls' minutes
			gs.lift          = g == 2 ? (float)setting("black_goop_air_turn", 30) / 100.0f : 1.0f; // black goop: turning in the air
			gs.drop_size     = 1.0f;
			gs.far_scale     = far;  // (the same for every weapon: without it the long range
			gs.shadow_damage = boss; // and Shadow Mario numbers flipped every frame)
			gs.water         = setting((key + "_water").c_str(), kWater[g]); // percent of FLUDD's own
			h->set_weapon_stats(g == 2 ? SMS_SPRAY_BLACK_GOOP : g ? SMS_SPRAY_ELECTRIC_GOOP : SMS_SPRAY_FIRE_GOOP, &gs);
		}
	}

	// Each frame: the weapon (or none) to the game, its numbers, and the zoom for the First Person mod.
	void update()
	{
		const SmsHeldFluddApi* h = heldFluddApi(api());
		if (!h) {
			if (setting("weapons", 1) && !toldNone) {
				warn("this game cannot fire FLUDD weapons (it needs a Remastered or First Person beta build)");
				toldNone = true;
			}
			return;
		}
		if (!told) {
			info("D-pad left and right choose the weapon%s", HELD_HAS(h, get_state) ? "" : " (this game is too old for the First Person mod's view)");
			told = true;
		}
		const SmsHeldFluddState s = state();
		const int w               = weaponNow();
		const bool own            = ownShots() && shotWeapon(w); // this mod fires it (the game only shows it)
		const int defeat          = (setting("weapons_defeat", 1) ? SMS_SPRAY_DEFEAT : 0) | (P().rHeld && !own ? 2 : 0)
		                 | (setting("boss_balance", 1) ? SMS_SPRAY_BOSSES : 0);
		const int normal          = setting("normal_defeat", 0) && !own ? SMS_SPRAY_DEFEAT_NORMAL : 0;
		int flags                 = 0;
		if (w && s.first_person)
			flags = defeat | normal;
		else if (w) // third person: FLUDD on his back, or the weapon's pack
			flags = defeat | normal | SMS_SPRAY_THIRD_PERSON | (setting("third_person_models", 1) ? SMS_SPRAY_THIRD_PERSON_MODEL : 0);
		else if (s.first_person && s.held) // FLUDD's own spray, held in view
			flags = normal;
		if (flags && w == SMS_SPRAY_BAZOOKA && setting("bazooka_air_forward", 1))
			flags |= SMS_SPRAY_AIR_FORWARD; // enemies already in the air pushed on forward, not up
		if (flags && w == SMS_SPRAY_BAZOOKA && setting("rocket_jump_hurt", 1))
			flags |= SMS_SPRAY_ROCKET_HURT | (setting("rocket_jump_ouch", 1) ? SMS_SPRAY_ROCKET_OUCH : 0)
			         | ((setting("rocket_jump_cry_volume", 40) & 127) << 13); // its volume, in percent
		if (flags && setting("rocket_jump", 2) != 0 && setting("enemy_hearts", 1)) // with rocket jumps on
			flags |= SMS_SPRAY_HEARTS;
		if (flags) { // how much goop the chosen one cleans, in tenths of the game's own
			const char* key = w == SMS_SPRAY_RIFLE ? "rifle_goop" : w == SMS_SPRAY_SMG ? "smg_goop" : w == SMS_SPRAY_BAZOOKA ? "bazooka_goop" : 0;
			if (key)
				flags |= ((setting(key, w == SMS_SPRAY_BAZOOKA ? 400 : 100) / 10) & 127) << 20;
		}
		if (flags && w == SMS_SPRAY_BAZOOKA && muteKeysHeld())
			flags |= 1 << 27; // SMS_SPRAY_MUTE_MARIO: his own voice left out while the keys are held
		if (flags && w == SMS_SPRAY_BAZOOKA) { // its shot seen flying
			const int look = setting("bazooka_projectile_look", 2);
			flags |= look == 0 ? SMS_SPRAY_SHOT_WATER : look == 1 ? SMS_SPRAY_SHOT_BALLOON : SMS_SPRAY_SHOT_BALLOON | SMS_SPRAY_SHOT_BIG;
		}
		if (flags && !setting("goop_grass_press", 0))
			flags |= 1 << 28; // electric goop leaves the grass standing
		if (flags && !setting("goop_grass_burn", 0))
			flags |= 1 << 29; // fire goop leaves the grass unburnt
		if (flags && !setting("goop_walls", 1))
			flags |= 1 << 30; // goop off walls
		if (flags && !setting("goop_characters", 1))
			flags |= (int)0x80000000u; // friendly characters left alone by the goop
		const int set = w * 1000 + flags;
		if (w || flags || P().lastSet > 0) {
			h->set_weapon(w, flags);
			if (w || flags)
				sendStats(h);
		}
		P().lastSet = set;
		if (HELD_HAS(h, set_zoom) && h->set_zoom)
			h->set_zoom(P().zoomed ? (float)setting("rifle_zoom", 25) : 0.0f,
			            P().zoomed ? (P().zoomFromThird ? SMS_ZOOM_FIRST_PERSON : 0) | (scopeShown() ? SMS_ZOOM_SCOPE : 0) : 0);
	}

	bool scopeShown() const
	{
		const SmsHeldFluddState s = state();
		return P().zoomed && setting("rifle_scope", 1) && s.first_person && s.held && chosen() == SMS_SPRAY_RIFLE;
	}

	// The Rifle's sniper scope while it is zoomed: black round a round sight, thin cross hairs with
	// marks along them and a red dot in the middle, coming in with the zoom
	void drawScope()
	{
		if (!scopeShown() || state().game_scope)
			return; // the game draws it itself, under its HUD
		float in = (float)((core().time() - P().zoomedAt) / 0.3);
		in       = in < 0.0f ? 0.0f : in > 1.0f ? 1.0f : in;
		if (in < 0.05f)
			return;
		float w = 0, h = 0;
		draw().canvas(&w, &h);
		const float x = w * 0.5f, y = h * 0.5f, rad = h * 0.46f;
		const uint8_t a = (uint8_t)(255.0f * in);
		// (co-op: the game draws each player's scope in their part of the screen; this one only
		// for a game that does not draw it)
		const float out = sqrtf(x * x + y * y) + 4.0f; // past the corners
		draw().circle_outline(x, y, (rad + out) * 0.5f, out - rad, SMS_RGBA(0, 0, 0, a)); // all round the sight
		draw().circle_outline(x, y, rad, h * 0.012f, SMS_RGBA(20, 20, 20, a));          // its rim
		const SmsColor hair = SMS_RGBA(0, 0, 0, (uint8_t)(230.0f * in));
		const float t = h * 0.0025f, gap = h * 0.02f;
		draw().line(x - rad, y, x - gap, y, t, hair); // the cross hairs, open in the middle
		draw().line(x + gap, y, x + rad, y, t, hair);
		draw().line(x, y - rad, x, y - gap, t, hair);
		draw().line(x, y + gap, x, y + rad, t, hair);
		draw().line(x - rad, y, x - rad * 0.55f, y, t * 3.0f, hair); // thicker at the edge
		draw().line(x + rad * 0.55f, y, x + rad, y, t * 3.0f, hair);
		draw().line(x, y + rad * 0.55f, x, y + rad, t * 3.0f, hair);
		for (int i = 1; i <= 4; i++) { // the marks
			const float d = rad * 0.1f * (float)i, m = h * 0.008f;
			draw().line(x - d, y - m, x - d, y + m, t, hair);
			draw().line(x + d, y - m, x + d, y + m, t, hair);
			draw().line(x - m, y + d, x + m, y + d, t, hair);
		}
		draw().circle(x, y, h * 0.003f, SMS_RGBA(230, 30, 30, a));
	}

	int load() override
	{
		preset.addRow();
		preset.optionBool("weapons", "FLUDD weapons, D-pad left and right", true);
		preset.optionBool("weapon_on_1", "Rifle on", true);
		preset.optionBool("weapon_on_2", "Submachine gun on", true);
		preset.optionBool("weapon_on_3", "Bazooka on", true);
		preset.optionBool("weapon_on_4", "Fire goop on", true);
		preset.optionBool("weapon_on_5", "Electric goop on", true);
		preset.optionBool("weapon_on_6", "Black goop on", true);
		preset.optionChoice("weapon_key_0", "Keyboard key for FLUDD", std::vector<const char*>(kKeyChoices, kKeyChoices + 11), 1);
		preset.optionChoice("weapon_key_1", "Keyboard key for the Rifle", std::vector<const char*>(kKeyChoices, kKeyChoices + 11), 5);
		preset.optionChoice("weapon_key_2", "Keyboard key for the Submachine gun", std::vector<const char*>(kKeyChoices, kKeyChoices + 11), 4);
		preset.optionChoice("weapon_key_3", "Keyboard key for the Bazooka", std::vector<const char*>(kKeyChoices, kKeyChoices + 11), 3);
		preset.optionChoice("weapon_key_4", "Keyboard key for Fire goop", std::vector<const char*>(kKeyChoices, kKeyChoices + 11), 7);
		preset.optionChoice("weapon_key_5", "Keyboard key for Electric goop", std::vector<const char*>(kKeyChoices, kKeyChoices + 11), 6);
		preset.optionChoice("weapon_key_6", "Keyboard key for Black goop", std::vector<const char*>(kKeyChoices, kKeyChoices + 11), 2);
		preset.optionBool("weapons_defeat", "FLUDD weapons can defeat enemies", true);
		preset.optionBool("third_person_weapons", "FLUDD weapons in third person too", true);
		preset.optionBool("third_person_models", "Weapon packs in place of FLUDD in third person", true);
		preset.optionBool("boss_balance", "Weapons made for bosses, not Shadow Mario", true);
		preset.optionBool("normal_defeat", "FLUDD can defeat enemies", false);
		preset.optionNumber("normal_defeat_time", "FLUDD time to defeat (tenths of a second)", 4, 1, 100, 1);
		preset.optionChoice("long_range", "Long range (enemies and characters seen and hit further away)",
		             std::vector<const char*>(kRanges, kRanges + kRangeCount), 5);
		preset.optionChoice("shadow_mario", "Shadow Mario difficulty (how many hits to take him down)",
		             std::vector<const char*>(kShadowLevels, kShadowLevels + kShadowCount), 5);
		preset.optionNumber("rifle_distance", "Rifle distance from the camera", 50, 30, 120, 5);
		preset.optionNumber("smg_distance", "Submachine gun distance from the camera", 45, 30, 120, 5);
		preset.optionNumber("bazooka_distance", "Bazooka distance from the camera", 55, 30, 120, 5);
		preset.optionNumber("rifle_rate", "Rifle fire rate (tenths of a second)", 13, 1, 100, 1);
		preset.optionNumber("rifle_push", "Rifle push (percent)", 300, 0, 400, 10);
		preset.optionNumber("rifle_defeat", "Rifle shots to defeat", 2, 1, 10, 1);
		preset.optionNumber("rifle_water", "Rifle water per shot", 16, 0, 200, 2);
		preset.optionNumber("rifle_zoom", "Rifle zoom, D-pad up (field of view)", 25, 5, 60, 5);
		preset.optionBool("rifle_scope", "Rifle sniper scope while zoomed", true);
		preset.optionNumber("rifle_hit", "Rifle hit size (percent, how near it must pass an enemy)", 100, 20, 500, 10);
		preset.optionNumber("rifle_blob", "Rifle blob size in first person (percent)", 80, 20, 400, 10);
		preset.optionNumber("rifle_blob_scope", "Rifle blob size while zoomed (percent)", 20, 20, 400, 10);
		preset.optionNumber("rifle_blob_third", "Rifle blob size in third person (percent)", 80, 20, 400, 10);
		preset.optionNumber("smg_rate", "Submachine gun fire rate (hundredths of a second)", 14, 1, 100, 1);
		preset.optionNumber("smg_push", "Submachine gun push (percent, under 100 weaker than FLUDD)", 90, 0, 400, 10);
		preset.optionNumber("smg_defeat", "Submachine gun shots to defeat", 6, 1, 10, 1);
		preset.optionNumber("smg_water", "Submachine gun water per shot", 16, 0, 200, 2);
		preset.optionNumber("smg_hit", "Submachine gun hit size (percent)", 100, 20, 500, 10);
		preset.optionNumber("smg_blob", "Submachine gun blob size (percent)", 20, 20, 400, 10);
		preset.optionNumber("bazooka_rate", "Bazooka fire rate (tenths of a second)", 8, 1, 200, 1);
		preset.optionNumber("bazooka_push", "Bazooka push in first person (percent)", 200, 0, 400, 10);
		preset.optionNumber("bazooka_push_third", "Bazooka push in third person (percent)", 800, 0, 1000, 10);
		preset.optionNumber("bazooka_range", "Bazooka range (percent)", 100, 10, 100, 10);
		preset.optionNumber("bazooka_defeat", "Bazooka shots to defeat", 3, 1, 10, 1);
		preset.optionNumber("bazooka_water", "Bazooka water per shot", 60, 0, 300, 5);
		preset.optionNumber("bazooka_burst", "Bazooka burst size in first person (percent)", 130, 20, 400, 10);
		preset.optionNumber("bazooka_burst_third", "Bazooka burst size in third person (percent)", 400, 20, 1000, 10);
		preset.optionNumber("bazooka_burst_jump", "Bazooka burst size while rocket jumping (percent)", 130, 20, 400, 10);
		preset.optionNumber("bazooka_lift", "Bazooka air height (percent)", 40, 0, 400, 10);
		preset.optionChoice("bazooka_projectile_look", "Bazooka projectile",
		             std::vector<const char*>(kProjectiles, kProjectiles + kProjectileCount), 2);
		preset.optionNumber("rifle_goop", "Goop the Rifle cleans (percent)", 100, 100, 1000, 10);
		preset.optionNumber("smg_goop", "Goop the Submachine gun cleans (percent)", 100, 100, 1000, 10);
		preset.optionNumber("bazooka_goop", "Goop the Bazooka cleans (percent)", 400, 100, 1000, 10);
		preset.optionNumber("bazooka_goop_area", "Bazooka goop area (percent)", 400, 10, 1000, 10);
		preset.optionBool("bazooka_air_forward", "Bazooka pushes enemies in the air forward, not up", true);
		preset.optionChoice("rocket_jump", "Bazooka rocket jumps in first person (shoot near your feet)",
		             std::vector<const char*>(kRocketModes, kRocketModes + kRocketModeCount), 2);
		preset.optionChoice("rocket_jump_key", "Rocket jump key",
		             std::vector<const char*>(kRocketKeys, kRocketKeys + kRocketKeyCount), 1);
		preset.optionBool("rocket_jump_hurt", "Rocket jumps cost Mario 1 health", true);
		preset.optionBool("rocket_jump_ouch", "Mario cries out when a rocket jump hurts", true);
		preset.optionNumber("rocket_jump_cry_volume", "Mario's cry volume (percent)", 40, 1, 100, 1);
		preset.optionBool("enemy_hearts", "Bazooka defeats drop a heart with rocket jumps on", true);
		preset.optionNumber("rocket_jump_power", "Bazooka rocket jump strength (percent)", 130, 10, 300, 10);
		preset.optionBool("rocket_key_launch", "Bazooka air shots defeat enemies, key shots launch them", true);
		preset.optionNumber("rocket_key_launch_height", "Bazooka rocket jump key launch height (percent)", 80, 10, 300, 10);
		preset.optionNumber("rocket_jump_feet", "Rocket jump forward boost off your feet (percent)", 50, 0, 300, 10);
		preset.optionNumber("fire_goop_defeat", "Fire goop burning time before an enemy is defeated (tenths of a second)", 5, 1, 100, 1);
		preset.optionNumber("fire_goop_hits", "Fire goop burns before an enemy is defeated", 3, 1, 20, 1);
		preset.optionNumber("fire_goop_water", "Fire goop water use (percent of FLUDD's)", 250, 10, 1000, 10);
		preset.optionNumber("fire_goop_life", "Fire goop lasts (seconds)", 20, 5, 600, 5);
		preset.optionNumber("fire_goop_size", "Fire goop puddle size (percent)", 130, 20, 300, 10);
		preset.optionNumber("electric_goop_defeat", "Electric goop shock time before an enemy is defeated (tenths of a second)", 12, 1, 100, 1);
		preset.optionNumber("electric_goop_hits", "Electric goop shocks before an enemy is defeated", 1, 1, 20, 1);
		preset.optionNumber("electric_goop_water", "Electric goop water use (percent of FLUDD's)", 200, 10, 1000, 10);
		preset.optionNumber("electric_goop_life", "Electric goop lasts (seconds)", 20, 5, 600, 5);
		preset.optionNumber("electric_goop_size", "Electric goop puddle size (percent)", 130, 20, 300, 10);
		preset.optionNumber("black_goop_speed", "Black goop speed (percent)", 500, 50, 1000, 10);
		preset.optionNumber("black_goop_air_turn", "Turning in the air after a black goop jump (percent, 100 at once)", 30, 0, 100, 1);
		preset.optionNumber("black_goop_water", "Black goop water use (percent of FLUDD's)", 200, 10, 1000, 10);
		preset.optionNumber("black_goop_life", "Black goop lasts (seconds)", 90, 5, 600, 5);
		preset.optionNumber("black_goop_size", "Black goop puddle size (percent)", 160, 20, 300, 10);
		preset.optionBool("goop_characters", "Goop shocks and burns friendly characters (never defeats them)", true);
		preset.optionBool("goop_walls", "Goop sticks to walls and drips down", true);
		preset.optionNumber("goop_wall_life", "Goop on walls lasts (seconds)", 15, 5, 300, 5);
		preset.optionBool("goop_grass_press", "Electric goop presses the grass down (Remastered builds)", false);
		preset.optionBool("goop_grass_burn", "Fire goop burns the grass away (Remastered builds)", false);
		preset.optionBool("goop_performance", "Goop performance mode (simpler puddles, for slower PCs)", false);
		preset.check();

		// the D-pad, once a frame
		on<SmsEvFrame>(SMS_EV_FRAME, [this](SmsEvFrame& f) {
			if (f.mode != SMS_MODE_STAGE)
				return;
			for (int pl = 0; pl < players(); pl++) { // each player's own controller (co-op)
				For w(*this, pl);
				switchWeapon(pad(pl));
				toggleZoom(pad(pl));
			}
		});

		// R: the weapon's while one can fire (no stream, pump or its sound from the game); the game
		// fires it each time it is ready while R is held
		on<SmsEvStage>(SMS_EV_STAGE_LOADED, [this](SmsEvStage& e) {
			onCoaster = e.area == 0x3A && (e.episode == 0 || e.episode == 1);
			if (onCoaster)
				info("on the roller coaster: the weapons off (its own rockets)");
		});
		on<SmsEvPadRead>(SMS_EV_PAD_READ, [this](SmsEvPadRead& r) {
			if (!r.pads)
				return;
			for (int pl = 0; pl < players(); pl++) { // each player's own controller (co-op)
				For w(*this, pl);
				padRead(r.pads[pl]);
			}
		});

		// this mod's own shots (a game with the mod API's water drops and actors)
		eng.api = &api();
		eng.mod = handle();
		if (ownShots())
			info("the weapons' shots made by this mod (the game's water drops and actors)");
		on(SMS_EV_STAGE_LOADED, [this](SmsEvent& ev) {
			const SmsEvStage* st = ev.data && ev.size >= offsetof(SmsEvStage, reason) + sizeof(int) ? (const SmsEvStage*)ev.data : nullptr;
			playing = st && st->reason == SMS_LEAVE_STATE_LOAD; // (a save state's load: straight into play)
		});
		on(SMS_EV_STAGE_START, [this](SmsEvent&) { playing = true; });
		on<SmsEvTick>(SMS_EV_TICK, [this](SmsEvTick& t) {
			if (!ownShots())
				return;
			const bool fireNow = canFire();
			const float dt = t.dt * 30.0f; // 30ths of a second
			eng.time += t.dt;
			for (int w = 1; w < shots::WEAPONS; w++)
				eng.setStats(w, shotSettings(w));
			eng.followStraight(dt);
			eng.followShoves(dt);
			for (int pl = 0; pl < players(); pl++) {
				For f(*this, pl);
				const int w = weaponNow();
				const SmsHeldFluddState s = state();
				if (shotWeapon(w))
					eng.setStats(w, shotSettings(w)); // (this player's view: first or third person's numbers)
				eng.playerTurn(pl, shotWeapon(w) ? w : 0, P().rHeld && fireNow, s.first_person && s.held,
				               eng.cur(w), eng.curSettings(w));
			}
		});
		// a player's FLUDD about to spray while a weapon of this mod's fires: none of its own stream
		on<SmsEvWaterEmit>(SMS_EV_WATER_EMIT, [this](SmsEvWaterEmit& e) -> int {
			if (!ownShots() || e.player < 0 || e.player >= players())
				return SMS_CONTINUE;
			For f(*this, e.player);
			return shotWeapon(weaponNow()) ? SMS_CANCEL : SMS_CONTINUE;
		});
		on<SmsEvHit>(SMS_EV_OBJECT_HIT, [this](SmsEvHit& e) -> int { return ownShots() ? eng.onHit(e) : SMS_CONTINUE; });
		on<SmsEvHitDone>(SMS_EV_OBJECT_HIT_DONE, [this](SmsEvHitDone& e) {
			if (ownShots())
				eng.onHitDone(e);
		});

		on<SmsEvCamera>(SMS_EV_CAMERA, [this](SmsEvCamera&) {
			cur = playerNow(); // each player's camera (co-op: the game runs as them)
			update();
			cur = 0;
		});

		on<SmsEvDraw>(SMS_EV_DRAW, [this](SmsEvDraw& d) {
			if (d.mode != SMS_MODE_STAGE)
				return;
			if (players() == 1)
				drawScope();
			if (core().time() > toastUntil)
				return;
			char text[64];
			snprintf(text, sizeof text, "%s%s", players() < 2 ? "" : toastPlayer ? "Player 2 " : "Player 1 ", toastText);
			const float w = draw().text_width(14, text) + 20;
			SmsRect r;
			if (!draw().hud_slot(handle(), SMS_ANCHOR_TOP, w, 26, &r))
				return;
			draw().rect(r.x, r.y, r.w, r.h, SMS_RGBA(0, 0, 0, 170));
			draw().text(r.x + 10, r.y + 6, 14, SMS_RGB(255, 255, 255), SMS_TEXT_SHADOW, text);
		});

		info("loaded");
		return 0;
	}

	// A player's controller as read (`s`): R the weapon's while one can fire (kept from the game)
	void padRead(SmsPadStatus& s)
	{
		bool& rHeld = P().rHeld;
		{
			rHeld           = (s.buttons & SMS_PAD_R) || s.trigger_r >= 60;
			if (onCoaster && s.trigger_r >= 60)
				s.buttons |= SMS_PAD_R; // on the roller coaster, R pressed part way (a soft R, a mouse button) fires its rocket too
			if (weaponNow() == 0 || isGoop(weaponNow()))
				return; // (a goop weapon: FLUDD's own stream, R the game's)
			// R left to the game as FLUDD's own: with the tank empty (its click), and in water with
			// it not full (it fills up as R is held, as ever); the weapon does not fire then
			SmsPlayer pl = SmsPlayer();
			pl.size = sizeof pl;
			if (player().get(&pl) && pl.has_fludd && !pl.on_yoshi
			    && (pl.water <= 0 || (pl.in_water && pl.water < pl.max_water))) {
				rHeld = false;
				return;
			}
			s.buttons &= (uint16_t)~SMS_PAD_R;
			s.trigger_r = 0;
		}
	}
};

SMS_MOD_CLASS(FluddWeapons)
