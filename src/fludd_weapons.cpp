// FLUDD Weapons: FLUDD's spray nozzle fires a Rifle, a Submachine gun or a Bazooka.
//
// D-pad left and right choose the weapon (FLUDD's own spray, Rifle, Submachine gun, Bazooka);
// R fires it. In first person (the First Person mod, FLUDD held in view) the weapon's own model is
// held there; in third person (its option) FLUDD stays on Mario's back, or is drawn as the
// weapon's pack. The Rifle zooms on D-pad up, with a sniper scope; from third person that goes
// into first person and back. The Bazooka's burst throws enemies, and Mario too in first person
// (a rocket jump). Long range: enemies and characters drawn and moving further away, to hit there.
//
// How: all of it here, on the mod API's general tools (API 1.8): its own water drops (api->water),
// the game's actors (api->actor), its own models (api->models), FLUDD posed as the gun
// (held_fludd->pose). It and the First Person mod read each other through held_fludd's get_state.
#include <sms_mod.hpp>
#include "recommended_preset.h"
#include "shots.h"
#include "models.h"
#include "goop.h"

// the weapons (the menu's): the mod's own numbers
enum { W_NORMAL = 0, W_SHOTGUN = 1, W_RIFLE = 2, W_BAZOOKA = 3, W_SMG = 4, W_FIRE_GOOP = 5, W_ELECTRIC_GOOP = 6, W_BLACK_GOOP = 7 };

#include <math.h>
#include <string>
#include <stddef.h>

namespace {

// FLUDD held in view and posed as a gun: api->held_fludd with its pose (API 1.8), taken only if
// its table says so (its magic)
const SmsHeldFluddApi* heldFluddApi(const SmsApi& api)
{
	if (api.size < offsetof(SmsApi, held_fludd) + sizeof(void*) || !api.held_fludd) // SMS_API_HAS
		return nullptr;
	const SmsHeldFluddApi* h = api.held_fludd;
	return h->size >= offsetof(SmsHeldFluddApi, pose) + sizeof(void*) && h->magic == SMS_HELD_FLUDD_MAGIC && h->pose ? h
	                                                                                                              : nullptr;
}
bool has(const SmsHeldFluddApi* h, size_t end) { return h && h->size >= end; }
#define HELD_HAS(h, field) has(h, offsetof(SmsHeldFluddApi, field) + sizeof(void*))

const char* const kWeaponNames[] = {"FLUDD", "Rifle", "Submachine gun", "Bazooka", "Fire goop", "Electric goop", "Black goop"};
const int kWeaponIds[]           = {W_NORMAL, W_RIFLE, W_SMG, W_BAZOOKA, W_FIRE_GOOP,
                                    W_ELECTRIC_GOOP, W_BLACK_GOOP};
const int kWeaponCount           = 7;
// the keyboard keys a weapon can be chosen with (the menu's choices after None)
const char* const kNumberKeys[]   = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
const int kKeyDefaults[] = {1, 5, 4, 3, 7, 6, 2}; // each weapon's key (the menu's choice: 1..9, 10 for 0)
const char* const kKeyChoices[]   = {"None", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
// the goop weapons: FLUDD's own stream, as fire or electric goop (R left to the game)
bool isGoop(int w) { return w == W_FIRE_GOOP || w == W_ELECTRIC_GOOP || w == W_BLACK_GOOP; }

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
	goop::Goop gp;     // the goop weapons (the mod API's tagged drops, landings, pictures)
	SmsMesh held[3] = {}, pack[3] = {}, packGun[3] = {}; // the weapons' models (api->models)
	bool modelsMade = false;
	float muzzleOf[2][3] = {};  // each player's pack's gun's muzzle, as last placed (third person)
	SmsVec3 muzzleMario[2] = {}; // where Mario was then
	double muzzleAt[2]     = {-10.0, -10.0};
	float downT[2]         = {};          // each player's pack's gun turned down (third person rocket jump), 0 to 1
	double downTime[2]     = {-1.0, -1.0}; // when it was last turned
	double underAt  = -10.0; // the last SMS_EV_DRAW_UNDER_HUD (the game draws mods under its HUD)
	float viewFar = 1.0f; // the long range given the game (game->set_view_distance)
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
	static bool shotWeapon(int w) { return w == W_RIFLE || w == W_SMG || w == W_BAZOOKA; }

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
	};
	Player ps[2];
	int cur = 0; // the player being worked on
	Player& P() { return ps[cur]; }
	const Player& P() const { return ps[cur]; }
	bool told      = false, toldNone = false;
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
			m.eng.selected = p;
		}
		~For()
		{
			m.cur = saved;
			m.select(-1);
			m.eng.selected = -1;
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

	int chosen() const { return P().weapon >= 0 && P().weapon < kWeaponCount && allowed(P().weapon) ? kWeaponIds[P().weapon] : W_NORMAL; }
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
		if (!weaponsHere() || chosen() == W_NORMAL || holding() || onCoaster)
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
		return setting("weapons", 1) && chosen() == W_RIFLE && s.first_person && s.held;
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
		if (!fp && chosen() == W_RIFLE && thirdPersonWeapons() && weaponNow() == W_RIFLE) {
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

	// The bazooka's shots on walking enemies: in the air they defeat them at once (2, the
	// "rocket_air_pop" setting), and while the rocket jump key is held those on the ground are thrown
	// up (1, "rocket_key_launch"). The keyboard's key is player 1's; a controller's L anyone's.
	int keyLaunchNow() const
	{
		const int pop = setting("rocket_air_pop", 1) ? 2 : 0;
		if (!setting("rocket_key_launch", 1))
			return pop;
		const SmsPad p = pad(cur);
		if ((p.held & SMS_PAD_L) || p.trigger_l >= 0.5f)
			return pop | 1;
		if (cur != 0)
			return pop;
		const int k  = setting("rocket_jump_key", 1);
		const int sc = input().key_from_name(kRocketKeys[k >= 0 && k < kRocketKeyCount ? k : 0]);
		return sc > 0 && input().key_down(sc) ? pop | 1 : pop;
	}

	// The chosen weapon's own models (api->models): held in FLUDD's place in first person (FLUDD left
	// out), and in third person (the setting) FLUDD drawn as the weapon's pack with the weapon on it,
	// kicking back as it fires. Hidden while Mario climbs (his hands busy; FLUDD as itself on his
	// back) and held in view once he is dead.
	// How far back the pack's gun goes, turned down, to hang behind the pack: its turned barrel just
	// behind the pack's back (in the level frame: y ahead), from the pack's corners as it sits now
	float downShift(int which, const float B[3][4], const float L[3][4], const float off[3]) const
	{
		const wmodels::Source& pk = wmodels::kPack[which];
		float packBack = 1e30f; // the pack's furthest back, the level frame's y, from the pivot's place
		for (int i = 0; i < pk.corners; i++) {
			const float* p = pk.pos + 3 * i;
			float w[3];
			for (int r = 0; r < 3; r++)
				w[r] = B[r][0] * p[0] + B[r][1] * p[1] + B[r][2] * p[2];
			const float y = L[0][1] * w[0] + L[1][1] * w[1] + L[2][1] * w[2];
			packBack      = y < packBack ? y : packBack;
		}
		const wmodels::Source& g = wmodels::kPackGun[which];
		float top = -1e30f; // the gun's highest (turned down, its top is furthest back)
		for (int i = 0; i < g.corners; i++)
			top = g.pos[3 * i + 2] > top ? g.pos[3 * i + 2] : top;
		// turned down about the pivot, a corner's y is py + (z - pz): the gun's back is py + top - pz
		const float gunBack = off[1] + (top - wmodels::kPivotZ) + wmodels::kPivotY - wmodels::kPivotY;
		const float shift   = gunBack - (packBack - 1.5f);
		return shift > 0.0f ? shift : 0.0f;
	}

	void drawModels(int pl, int w, const SmsHeldFluddState& s)
	{
		const SmsApi& a = api();
		if (a.size < offsetof(SmsApi, models) + sizeof(void*) || !a.models)
			return;
		if (!modelsMade) {
			modelsMade = true;
			for (int i = 0; i < 3; i++) {
				held[i]    = wmodels::make(a.models, handle(), wmodels::kHeld[i]);
				pack[i]    = wmodels::make(a.models, handle(), wmodels::kPack[i]);
				packGun[i] = wmodels::make(a.models, handle(), wmodels::kPackGun[i]);
			}
		}
		const int which = w == W_RIFLE ? 0 : w == W_BAZOOKA ? 1 : w == W_SMG ? 2 : -1;
		if (which < 0)
			return;
		if (!canFire()) // a stage's opening or a cutscene: the game's own FLUDD, as it poses him there
			return;
		SmsPlayer m = SmsPlayer();
		m.size      = sizeof m;
		if (!a.player->get(&m) || !m.valid)
			return;
		const uint32_t st   = m.status;
		const uint32_t i    = st & 0x3F;
		const bool climbing = (st & 0x1C0) == 0x140 && (i <= 0x1C || (i >= 0x28 && i <= 0x2F)); // MARIO_STATUS_TYPE_SPECIAL
		const bool dead     = m.health <= 0;
		int hide            = 0;
		if (s.first_person && s.held) { // held in view: the weapon in FLUDD's place
			hide |= 1 << SMS_FRAME_HELD;
			if (!climbing && !dead) {
				const float k = wmodels::kHeldScale[which];
				const float place[3][4] = {{k, 0, 0, 0}, {0, k, 0, wmodels::kHeldAhead[which] - wmodels::kHeldMuzzle[which] * k}, {0, 0, k, 0}};
				a.models->draw(held[which], SMS_FRAME_HELD, pl, place);
			}
		}
		if (setting("third_person_models", 1) && !climbing) { // on his back (co-op: the other's view of one held too)
			hide |= 1 << SMS_FRAME_BACK;
			const float id[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
			a.models->draw(pack[which], SMS_FRAME_BACK, pl, id);
			// its gun level on top, turned about where it sits on the neck, kicking back and up as it fires
			float B[3][4], L[3][4];
			if (a.models->frame(SMS_FRAME_BACK, pl, B) && a.models->frame(SMS_FRAME_BACK_LEVEL, pl, L)) {
				const float py = wmodels::kPivotY, pz = wmodels::kPivotZ;
				float off[3]; // the pivot (the pack's 0, py, pz) in the level frame's units
				const float pw[3] = {B[0][1] * py + B[0][2] * pz, B[1][1] * py + B[1][2] * pz, B[2][1] * py + B[2][2] * pz};
				for (int c = 0; c < 3; c++)
					off[c] = L[0][c] * pw[0] + L[1][c] * pw[1] + L[2][c] * pw[2];
				const shots::Engine::Player& ep = eng.players[pl & 1];
				const float len  = ep.shotWeapon == shots::BAZOOKA ? 0.5f : ep.shotWeapon == shots::SMG ? 0.12f : 0.35f;
				const float t    = (float)(eng.time - ep.shotAt);
				const float ph   = t >= 0.0f && t < len ? t / len : -1.0f;
				const float kick = ph >= 0.0f ? (1.0f - ph) * (1.0f - ph) : 0.0f;
				// third person rocket jump: turned down behind him, bending back from where it sits (its
				// barrel swung over behind the pack, the muzzle down at his heels), and back again
				const bool wantDown = which == 1 && !(s.first_person && s.held) && thirdRocketDown();
				const double now    = eng.time;
				const float dt      = downTime[pl & 1] >= 0.0 && now > downTime[pl & 1] ? (float)(now - downTime[pl & 1]) : 0.0f;
				downTime[pl & 1]    = now;
				float& dn           = downT[pl & 1];
				dn += (wantDown ? 1.0f : -1.0f) * dt * 7.0f; // about 0.15 s either way
				dn = dn < 0.0f ? 0.0f : dn > 1.0f ? 1.0f : dn;
				const float ease  = dn * dn * (3.0f - 2.0f * dn);
				const float early = dn < 0.5f ? dn * 2.0f : 1.0f; // back first, so it turns behind the pack
				const float slide = dn > 0.0f ? early * early * (3.0f - 2.0f * early) * downShift(which, B, L, off) : 0.0f;
				const float back  = kick * wmodels::kKickBack[which] + slide,
				            tilt  = kick * wmodels::kKickTilt[which] - ease * 1.5707963f;
				const float c = cosf(tilt), sn = sinf(tilt);
				const float place[3][4] = {{1, 0, 0, off[0]},
				                           {0, c, -sn, off[1] + py - (c * py - sn * pz) - back - py},
				                           {0, sn, c, off[2] + pz - (sn * py + c * pz) - pz}};
				a.models->draw(packGun[which], SMS_FRAME_BACK_LEVEL, pl, place);
				// its muzzle in the world (the shots start there), and where Mario was then
				const float* mz = wmodels::kPackGunMuzzleAt[which];
				float g[3];
				for (int r = 0; r < 3; r++)
					g[r] = place[r][0] * mz[0] + place[r][1] * mz[1] + place[r][2] * mz[2] + place[r][3];
				for (int r = 0; r < 3; r++)
					muzzleOf[pl & 1][r] = L[r][0] * g[0] + L[r][1] * g[1] + L[r][2] * g[2] + L[r][3];
				muzzleMario[pl & 1] = m.position;
				muzzleAt[pl & 1]    = eng.time;
			}
		}
		if (hide)
			a.models->hide_fludd(pl, hide);
	}

	// the goop weapons the mod's own (this game's API has their tools): FLUDD's stream made goop here
	bool ownGoop() const
	{
		const SmsApi& a = api();
		const SmsHeldFluddApi* h = a.held_fludd;
		return ownShots() && gp.hasPictures() && h && h->size >= offsetof(SmsHeldFluddApi, dress) + sizeof(void*) && h->dress;
	}

	// the goop weapons' numbers (the menu)
	void goopSettings()
	{
		static const int kDefeat[3] = {5, 12, 12}, kLife[3] = {20, 20, 90}, kSize[3] = {130, 130, 160};
		static const int kHits[3] = {3, 1, 1}, kWater[3] = {250, 200, 200};
		for (int g = 0; g < 3; g++) {
			const std::string key(g == 2 ? "black_goop" : g ? "electric_goop" : "fire_goop");
			goop::Stats& st = gp.set.kinds[g];
			st.defeat       = (float)setting((key + "_defeat").c_str(), kDefeat[g]) / 10.0f;
			st.life         = (float)setting((key + "_life").c_str(), kLife[g]);
			st.size         = (float)setting((key + "_size").c_str(), kSize[g]) / 100.0f;
			st.hits         = setting((key + "_hits").c_str(), kHits[g]);
			st.water        = (float)setting((key + "_water").c_str(), kWater[g]) / 100.0f;
		}
		gp.set.blackSpeed = (float)setting("black_goop_speed", 500) / 100.0f;
		gp.set.airTurn    = (float)setting("black_goop_air_turn", 30) / 100.0f;
		gp.set.wallLife   = (float)setting("goop_wall_life", 15);
		gp.set.walls      = setting("goop_walls", 1) != 0;
		gp.set.npcs       = setting("goop_characters", 1) != 0;
		gp.set.defeat     = setting("weapons_defeat", 1) != 0;
		gp.set.quality    = setting("goop_quality", 2);
	}

	// long range: how many times further than the game's own enemies are seen and move
	float farNow() const
	{
		const int r = setting("long_range", 5);
		return kRangeScale[r >= 0 && r < kRangeCount ? r : 0];
	}

	// the rocket jump key held now (the keyboard's: player 1's) or a controller's L
	bool rocketKeyHeld() const
	{
		const SmsPad p = pad(cur);
		if ((p.held & SMS_PAD_L) || p.trigger_l >= 0.5f)
			return true;
		if (cur != 0)
			return false;
		const int k  = setting("rocket_jump_key", 1);
		const int sc = input().key_from_name(kRocketKeys[k >= 0 && k < kRocketKeyCount ? k : 0]);
		return sc > 0 && input().key_down(sc);
	}

	// third person: rocket jumps on there and the rocket jump key (or a controller's L) held, the
	// bazooka on his back turned down behind him
	bool thirdRocketDown() const
	{
		return setting("rocket_jump_third", 0) && setting("rocket_jump", 2) != 0 && rocketKeyHeld();
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

	// A weapon's numbers from the menu for this mod's shots
	shots::Settings shotSettings(int w) const
	{
		shots::Settings s = shots::Settings();
		const SmsHeldFluddState seen = state();
		const bool inView = seen.first_person && seen.held;
		s.defeat = setting("weapons_defeat", 1) != 0;
		s.third  = setting("third_person_weapons", 1) != 0;
		s.inView = inView;
		const char* goopKey = w == W_RIFLE ? "rifle_goop" : w == W_SMG ? "smg_goop" : "bazooka_goop";
		s.goop     = (float)setting(goopKey, w == W_BAZOOKA ? 400 : 100) / 100.0f;
		s.goopArea = w == W_BAZOOKA ? (float)setting("bazooka_goop_area", 400) / 100.0f : 0.0f;
		s.noClean  = w == W_BAZOOKA && muteKeysHeld();
		s.hitSize = 1.0f;
		s.dropSize = 1.0f;
		s.range = 1.0f;
		s.lift = 0.0f;
		s.burst = 1.0f;
		if (w == W_RIFLE) {
			s.every    = (float)setting("rifle_rate", 13) / 10.0f;
			s.push     = (float)setting("rifle_push", 300) / 100.0f;
			s.toDefeat = setting("rifle_defeat", 2);
			s.water    = setting("rifle_water", 16);
			s.hitSize  = (float)setting("rifle_hit", 100) / 100.0f;
			s.dropSize = (float)setting(P().zoomed && inView ? "rifle_blob_scope" : inView ? "rifle_blob" : "rifle_blob_third", P().zoomed && inView ? 20 : 80) / 100.0f;
		} else if (w == W_SMG) {
			s.every    = (float)setting("smg_rate", 14) / 100.0f;
			s.push     = (float)setting("smg_push", 90) / 100.0f;
			s.toDefeat = setting("smg_defeat", 6);
			s.water    = setting("smg_water", 16);
			s.hitSize  = (float)setting("smg_hit", 100) / 100.0f;
			s.dropSize = (float)setting("smg_blob", 20) / 100.0f;
		} else if (w == W_BAZOOKA) {
			const bool down = !inView && thirdRocketDown();
			const bool jump = inView ? rocketJumpOn() : down;
			s.every    = (float)setting("bazooka_rate", 8) / 10.0f;
			s.range    = (float)setting("bazooka_range", 100) / 100.0f;
			s.toDefeat = setting("bazooka_defeat", 3);
			s.water    = setting("bazooka_water", 60);
			s.burst    = (float)setting(jump ? "bazooka_burst_jump" : inView ? "bazooka_burst" : "bazooka_burst_third", inView || jump ? 130 : 400) / 100.0f;
			s.aimDown  = down;
			s.push     = (float)setting(inView ? "bazooka_push" : "bazooka_push_third", inView ? 200 : 800) / 100.0f;
			s.lift     = (float)setting("bazooka_lift", 40) / 100.0f;
			s.selfJump    = jump ? (float)setting("rocket_jump_power", 130) / 100.0f : 0.0f;
			s.feetForward = (float)setting("rocket_jump_feet", 50) / 100.0f;
			s.hurt        = setting("rocket_jump_hurt", 1) != 0;
			s.cry         = setting("rocket_jump_ouch", 1) != 0;
			s.cryVolume   = (float)setting("rocket_jump_cry_volume", 40) / 100.0f;
			s.keyLaunch   = keyLaunchNow();
			s.enemySafe   = setting("rocket_jump_enemy_safe", 1) && rocketKeyHeld();
			s.keepPictures = setting("bazooka_keeps_symbols", 1) != 0 && !inView; // third person only
			s.keyLift     = 30.0f * (float)setting("rocket_key_launch_height", 80) / 100.0f;
			s.airForward  = setting("bazooka_air_forward", 1) != 0;
		}
		s.viewRange = farNow();
		s.look   = setting("bazooka_projectile_look", 2);
		s.hearts = setting("rocket_jump", 2) != 0 && setting("enemy_hearts", 1) != 0;
		s.bosses = setting("boss_balance", 1) != 0;
		const int sh   = setting("shadow_mario", 5);
		s.shadowDamage = kShadowDamage[sh >= 0 && sh < kShadowCount ? sh : 2];
		return s;
	}

	// Each frame: FLUDD's pose for the weapon (or none), and the zoom for the First Person mod.
	void update()
	{
		const SmsHeldFluddApi* h = heldFluddApi(api());
		if (!h) {
			if (setting("weapons", 1) && !toldNone) {
				warn("this game cannot fire FLUDD weapons (it needs the mod API 1.8 tools: a newer game)");
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
		// FLUDD posed as the chosen gun (each tick): held still in view, its nozzle out on his back
		// while it fires (and under the pack), one pump as each shot goes
		int flags = 0;
		float pump = -1.0f, hold = 0.0f;
		if (shotWeapon(w)) {
			const bool inView = s.first_person && s.held;
			if (inView) {
				flags = SMS_POSE_STILL;
				hold  = (float)setting(w == W_RIFLE ? "rifle_distance" : w == W_SMG ? "smg_distance" : "bazooka_distance",
				                       w == W_RIFLE ? 50 : w == W_SMG ? 45 : 55);
			} else if (P().rHeld || setting("third_person_models", 1))
				flags = SMS_POSE_NOZZLE_OUT;
			const shots::Engine::Player& ep = eng.players[cur & 1];
			const float len = ep.shotWeapon == shots::BAZOOKA ? 0.5f : ep.shotWeapon == shots::SMG ? 0.12f : 0.35f;
			const float t   = (float)(eng.time - ep.shotAt);
			if (t >= 0.0f && t < len && !(!inView && setting("third_person_models", 1)))
				pump = t / len;
		}
		h->pose(cur, flags, pump, hold);
		if (HELD_HAS(h, set_zoom) && h->set_zoom)
			h->set_zoom(P().zoomed ? (float)setting("rifle_zoom", 25) : 0.0f,
			            P().zoomed ? (P().zoomFromThird ? SMS_ZOOM_FIRST_PERSON : 0) | (scopeShown() ? SMS_ZOOM_SCOPE : 0) : 0);
	}

	bool scopeShown() const
	{
		const SmsHeldFluddState s = state();
		return P().zoomed && setting("rifle_scope", 1) && s.first_person && s.held && chosen() == W_RIFLE;
	}

	// The Rifle's sniper scope while it is zoomed: black round a round sight, thin cross hairs with
	// marks along them and a red dot in the middle, coming in with the zoom
	// the scope of every player zoomed in, each in their part of the screen (co-op) (an older game's
	// SMS_EV_DRAW, over the HUD; this game's own draws it itself, under its HUD)
	void drawScope()
	{
		if (state().game_scope || core().time() - underAt < 0.5)
			return; // drawn under the HUD (SMS_EV_DRAW_UNDER_HUD) or by the game itself
		float cw = 0, ch = 0;
		draw().canvas(&cw, &ch);
		for (int pl = 0; pl < players(); pl++) {
			float vx = 0, vy = 0, vw = 1, vh = 1;
			const SmsHeldFluddApi* hf = api().held_fludd;
			if (players() > 1 && hf && hf->size >= offsetof(SmsHeldFluddApi, player_view) + sizeof(void*) && hf->player_view)
				hf->player_view(pl, &vx, &vy, &vw, &vh);
			draw().clip_push(cw * vx, ch * vy, cw * vw, ch * vh);
			scopeIn(pl, cw * vx, ch * vy, cw * vw, ch * vh);
			draw().clip_pop();
		}
	}

	// a player's scope in a part of the canvas: black round a round sight, thin cross hairs with
	// marks along them, a red dot in the middle, coming in with the zoom
	void scopeIn(int pl, float vx, float vy, float w, float h)
	{
		For f(*this, pl);
		if (!scopeShown())
			return;
		float in = (float)((core().time() - P().zoomedAt) / 0.3);
		in       = in < 0.0f ? 0.0f : in > 1.0f ? 1.0f : in;
		if (in < 0.05f)
			return;
		const float x = vx + w * 0.5f, y = vy + h * 0.5f, rad = h * 0.46f;
		const uint8_t a = (uint8_t)(255.0f * in);
		const float out = sqrtf(w * w + h * h) * 0.5f + 4.0f; // past the corners
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
			const float d = rad * 0.1f * (float)i, mk = h * 0.008f;
			draw().line(x - d, y - mk, x - d, y + mk, t, hair);
			draw().line(x + d, y - mk, x + d, y + mk, t, hair);
			draw().line(x - mk, y + d, x + mk, y + d, t, hair);
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
		preset.optionBool("bazooka_keeps_symbols", "Bazooka leaves graffiti symbols for FLUDD in third person", true);
		preset.optionBool("bazooka_air_forward", "Bazooka pushes enemies in the air forward, not up", true);
		preset.optionChoice("rocket_jump", "Bazooka rocket jumps in first person (shoot near your feet)",
		             std::vector<const char*>(kRocketModes, kRocketModes + kRocketModeCount), 2);
		preset.optionBool("rocket_jump_third", "Bazooka rocket jumps in third person (hold the rocket jump key)", false);
		preset.optionChoice("rocket_jump_key", "Rocket jump key",
		             std::vector<const char*>(kRocketKeys, kRocketKeys + kRocketKeyCount), 1);
		preset.optionBool("rocket_jump_hurt", "Rocket jumps cost Mario 1 health", true);
		preset.optionBool("rocket_jump_enemy_safe", "No rocket jump damage when a key shot hits an enemy", true);
		preset.optionBool("rocket_jump_ouch", "Mario cries out when a rocket jump hurts", true);
		preset.optionNumber("rocket_jump_cry_volume", "Mario's cry volume (percent)", 40, 1, 100, 1);
		preset.optionBool("enemy_hearts", "Bazooka defeats drop a heart with rocket jumps on", true);
		preset.optionNumber("rocket_jump_power", "Bazooka rocket jump strength (percent)", 130, 10, 300, 10);
		preset.optionBool("rocket_key_launch", "Bazooka rocket jump key shots launch enemies up", true);
		preset.optionBool("rocket_air_pop", "Bazooka shots defeat enemies in the air at once", true);
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
		static const char* const kQualities[] = {"Very low", "Low", "Medium", "High", "Very high"};
		preset.optionChoice("goop_quality", "Goop quality (lower for slower PCs)", std::vector<const char*>(kQualities, kQualities + 5), 2);
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
		gp.api  = &api();
		gp.mod  = handle();
		eng.muzzleCtx = this;
		eng.muzzle    = [](void* ctx, int pl, float* out) -> bool {
			FluddWeapons* self = static_cast<FluddWeapons*>(ctx);
			pl &= 1;
			if (self->eng.time - self->muzzleAt[pl] > 0.2 || !self->setting("third_person_models", 1))
				return false;
			SmsPlayer m = SmsPlayer(); // moved on with him since it was placed
			m.size      = sizeof m;
			if (!self->api().player->get(&m) || !m.valid)
				return false;
			out[0] = self->muzzleOf[pl][0] + m.position.x - self->muzzleMario[pl].x;
			out[1] = self->muzzleOf[pl][1] + m.position.y - self->muzzleMario[pl].y;
			out[2] = self->muzzleOf[pl][2] + m.position.z - self->muzzleMario[pl].z;
			return true;
		};
		eng.washCtx  = &gp;
		eng.washGoop = [](void* g, float x, float y, float z, float r) { static_cast<goop::Goop*>(g)->washBurst(x, y, z, r); };
		if (ownShots())
			info("the weapons' shots made by this mod (the game's water drops and actors)");
		on(SMS_EV_STAGE_LOADED, [this](SmsEvent& ev) {
			const SmsEvStage* st = ev.data && ev.size >= offsetof(SmsEvStage, reason) + sizeof(int) ? (const SmsEvStage*)ev.data : nullptr;
			playing = st && st->reason == SMS_LEAVE_STATE_LOAD; // (a save state's load: straight into play)
			eng.stageReset();
			gp.stageReset();
		});
		on(SMS_EV_STAGE_START, [this](SmsEvent&) { playing = true; });
		// the bazooka with the rocket jump keys held: Mario's own voice (MSD_SE_MA_VO_* to MSD_SE_MV_*)
		// left out, so his rocket jump's cries play alone
		on<SmsEvSound>(SMS_EV_SOUND, [this](SmsEvSound& e) -> int {
			if (!ownShots() || e.id < 0x7800u || e.id > 0x792Fu)
				return SMS_CONTINUE;
			For f(*this, 0);
			return weaponNow() == W_BAZOOKA && muteKeysHeld() ? SMS_CANCEL : SMS_CONTINUE;
		});
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
			eng.popTick();
			eng.chompTick();
			{
				For f(*this, 0); // (rocket jumps: player 1's)
				eng.rocketTick(dt, weaponNow() == W_BAZOOKA && muteKeysHeld());
			}
			const float distance = farNow();
			if (distance != viewFar && SHOTS_HAS(api().game, SmsGameApi, set_view_distance)) {
				api().game->set_view_distance(distance);
				viewFar = distance;
			}
			for (int pl = 0; pl < players(); pl++) {
				For f(*this, pl);
				const int w = weaponNow();
				const SmsHeldFluddState s = state();
				if (shotWeapon(w))
					eng.setStats(w, shotSettings(w)); // (this player's view: first or third person's numbers)
				eng.playerTurn(pl, shotWeapon(w) ? w : 0, P().rHeld && fireNow, s.first_person && s.held,
				               eng.cur(w), eng.curSettings(w));
				drawModels(pl, w, s);
			}
			eng.normalDefeat = setting("normal_defeat", 0) ? (float)setting("normal_defeat_time", 4) / 10.0f : 0.0f;
			eng.looksTick(dt);
			eng.looksDraw();
			if (ownGoop()) {
				goopSettings();
				int kinds[2]  = {0, 0};
				bool held[2]  = {false, false};
				for (int pl = 0; pl < players() && pl < 2; pl++) {
					For f(*this, pl);
					const int w = weaponNow();
					kinds[pl]   = isGoop(w) ? w : 0;
					held[pl]    = state().first_person && state().held;
				}
				gp.tick(dt, kinds, held);
			}
		});
		// a player's FLUDD about to spray while a weapon of this mod's fires: none of its own stream
		on<SmsEvWaterEmit>(SMS_EV_WATER_EMIT, [this](SmsEvWaterEmit& e) -> int {
			if (!ownShots() || e.player < 0 || e.player >= players())
				return SMS_CONTINUE;
			For f(*this, e.player);
			const int w = weaponNow();
			if (ownGoop()) // a goop weapon: FLUDD's own stream made goop (none: its water)
				gp.onEmit(e, isGoop(w) && !shotWeapon(w) ? w : 0);
			return shotWeapon(w) ? SMS_CANCEL : SMS_CONTINUE;
		});
		// a drop landing: goop leaves goop, water washes it (the SMG's shot a good splash, the
		// bazooka's none with the rocket jump keys held)
		on<SmsEvWaterLand>(SMS_EV_WATER_LAND, [this](SmsEvWaterLand& e) -> int {
			if (!ownGoop())
				return SMS_CONTINUE;
			int wash = 1;
			if ((e.tag & 0xFF000000u) == 0x5A000000u) {
				const int weapon = (int)((e.tag >> 20) & 7);
				if (weapon == shots::SMG)
					wash = 2;
				if (weapon == shots::BAZOOKA && muteKeysHeld())
					wash = 0;
				if (weapon == shots::PIECES)
					wash = 0;
			}
			return gp.onLand(e, wash);
		});
		// each view as its scene is drawn: the goop there
		on<SmsEvDrawScene>(SMS_EV_DRAW_SCENE, [this](SmsEvDrawScene& e) {
			if (ownGoop())
				gp.draw(e);
		});
		on<SmsEvHit>(SMS_EV_OBJECT_HIT, [this](SmsEvHit& e) -> int {
			if (ownGoop() && gp.onHit(e) == SMS_CANCEL)
				return SMS_CANCEL;
			return ownShots() ? eng.onHit(e) : SMS_CONTINUE;
		});
		on<SmsEvHitDone>(SMS_EV_OBJECT_HIT_DONE, [this](SmsEvHitDone& e) {
			if (ownShots())
				eng.onHitDone(e);
			if (ownGoop())
				gp.onHitDone(e);
		});

		on<SmsEvCamera>(SMS_EV_CAMERA, [this](SmsEvCamera&) {
			cur = playerNow(); // each player's camera (co-op: the game runs as them)
			update();
			cur = 0;
		});

		// each view as the game draws it, before its HUD: that player's scope there (under the HUD)
		on<SmsEvDrawUnder>(SMS_EV_DRAW_UNDER_HUD, [this](SmsEvDrawUnder& d) {
			underAt = core().time();
			if (d.player >= 0 && d.player < players())
				scopeIn(d.player, 0.0f, 0.0f, d.canvas_w, d.canvas_h);
		});
		on<SmsEvDraw>(SMS_EV_DRAW, [this](SmsEvDraw& d) {
			if (d.mode != SMS_MODE_STAGE)
				return;
			drawScope(); // (co-op: each player's in their half)
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
