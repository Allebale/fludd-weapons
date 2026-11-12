// The weapons' own models (made in Blender from simple shapes, low-poly, their light baked into
// their colours as the GameCube's games looked): held in FLUDD's place in first person, and in third
// person FLUDD drawn as the weapon's pack with the weapon on it, through api->models. Each triangle's
// corners: their place (x right, y forward to the muzzle, z up) and colour.
#pragma once
#include "sms_mod.h"

#include <vector>

namespace wmodels {

#include "models/rifle_model.inc"
#include "models/bazooka_model.inc"
#include "models/smg_model.inc"
#include "models/rifle_pack.inc"
#include "models/bazooka_pack.inc"
#include "models/smg_pack.inc"
#include "models/rifle_pack_gun.inc"
#include "models/bazooka_pack_gun.inc"
#include "models/smg_pack_gun.inc"

struct Source {
	const float* pos;
	const unsigned char* col;
	int corners;
};
// 0 the rifle, 1 the bazooka, 2 the submachine gun: held, the pack, the pack's gun
const Source kHeld[3] = {{kRiflePos, kRifleCol, kRifleCorners}, {kBazookaPos, kBazookaCol, kBazookaCorners}, {kSmgPos, kSmgCol, kSmgCorners}};
const Source kPack[3] = {{kRiflePackPos, kRiflePackCol, kRiflePackCorners},
                         {kBazookaPackPos, kBazookaPackCol, kBazookaPackCorners},
                         {kSmgPackPos, kSmgPackCol, kSmgPackCorners}};
const Source kPackGun[3] = {{kRiflePackGunPos, kRiflePackGunCol, kRiflePackGunCorners},
                            {kBazookaPackGunPos, kBazookaPackGunCol, kBazookaPackGunCorners},
                            {kSmgPackGunPos, kSmgPackGunCol, kSmgPackGunCorners}};
const float kHeldMuzzle[3] = {kRifleMuzzle, kBazookaMuzzle, kSmgMuzzle}; // their fronts, forward (own units)
const float kHeldScale[3]  = {12.5f, 15.0f, 15.0f};                    // to about 50 and 47 of the game's long
const float kHeldAhead[3]  = {4.0f, 6.0f, 4.0f};                        // the muzzle past the water's start
const float kKickBack[3]   = {6.0f, 9.0f, 3.5f};                        // the pack's gun's kick: back, and its tilt
const float kKickTilt[3]   = {0.25f, 0.2f, 0.12f};
const float kPivotY = 2.0f, kPivotZ = 21.0f; // where the pack's gun sits on its neck (the pack's units)

inline SmsMesh make(const SmsModelApi* m, SmsMod* mod, const Source& s)
{
	std::vector<SmsCorner> c((size_t)s.corners);
	for (int i = 0; i < s.corners; i++) {
		c[i].x = s.pos[i * 3], c[i].y = s.pos[i * 3 + 1], c[i].z = s.pos[i * 3 + 2];
		c[i].r = s.col[i * 4], c[i].g = s.col[i * 4 + 1], c[i].b = s.col[i * 4 + 2], c[i].a = s.col[i * 4 + 3];
	}
	return m->create(mod, c.data(), s.corners);
}

} // namespace wmodels
