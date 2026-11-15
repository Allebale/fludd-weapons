// Shoulder aim: FLUDD and the weapons aimed like a modern third-person shooter. While the aim is
// held (L, or a key of the player's), the camera moves over Mario's right shoulder, a crosshair
// shows, the mouse (and the C-stick) aim, and Mario faces where it points while the stick walks him
// any way (sideways, backwards) and A still jumps. FLUDD's stream and the weapons go to the
// crosshair. Single player, third person (not with the First Person mod's view on).
//
// Only mod API tools: SMS_EV_CAMERA (where the picture is drawn from), held_fludd->take_mouse,
// player->set_aim (he faces one way and walks another, FLUDD tilted), SMS_EV_PAD_READ (the stick
// turned to the aim's way, L and the C-stick kept from the game, R kept short of its click so he
// walks while spraying), game->wall_at and game->ground (the camera out of walls, the crosshair's
// point on the stage).
#pragma once
#include "shots.h"

namespace aim {

using shots::V;

const float kDeg = 3.14159265f / 180.0f;

// the crosshair's colors, as the First Person mod's (its "crosshair_color" setting)
const char* const kCrossNames[] = {"White", "Red", "Green", "Blue", "Yellow", "Cyan", "Pink", "Orange", "Grey", "Black"};
const SmsColor kCrossColors[]   = {SMS_RGB(255, 255, 255), SMS_RGB(255, 60, 60),  SMS_RGB(70, 230, 90),
                                   SMS_RGB(70, 150, 255),  SMS_RGB(255, 225, 40), SMS_RGB(60, 235, 235),
                                   SMS_RGB(255, 110, 200), SMS_RGB(255, 150, 40), SMS_RGB(150, 150, 150),
                                   SMS_RGB(20, 20, 20)};
const int kCrossCount = 10;
// "shoulder_aim_invert": which of the aim's ways are turned round
const char* const kInvertNames[] = {"Off", "Up and down", "Left and right", "Both"};

struct Shoulder {
	const SmsApi* api = nullptr;

	bool held      = false; // the aim button, this frame
	bool active    = false; // aiming (held, in a stage, third person, single player)
	float blend    = 0.0f;  // 0 the game's camera, 1 over the shoulder
	float yaw      = 0.0f;  // the aim, degrees (SmsPlayer.yaw's: 0 faces +Z, 90 faces +X)
	float pitch    = 0.0f;  // degrees up
	float gameYaw  = 0.0f;  // the game camera's way, last frame (the stick is read against it)
	bool aimSet    = false; // player->set_aim is on
	float cx = 0, cy = 0;   // the C-stick, taken from the game while aiming (-1..1)
	V point;                // where the crosshair is on the stage
	bool pointOk   = false;
	V camAt;                // the camera, as placed this frame
	int invert     = 0;     // kInvertNames
	float fov      = 0.0f;  // the aim's field of view (degrees; 0 the game's)

	static V forward(float yawDeg, float pitchDeg)
	{
		const float y = yawDeg * kDeg, p = pitchDeg * kDeg;
		return V(sinf(y) * cosf(p), sinf(p), cosf(y) * cosf(p));
	}
	static V right(float yawDeg) { return V(-cosf(yawDeg * kDeg), 0.0f, sinf(yawDeg * kDeg)); }

	bool marioAt(V* at, SmsPlayer* out = nullptr) const
	{
		SmsPlayer m = SmsPlayer();
		m.size      = sizeof m;
		if (!api->player->get(&m) || !m.valid)
			return false;
		*at = V(m.position);
		if (out)
			*out = m;
		return true;
	}

	// The controller as read this frame (before the game): the aim's button, and while aiming the
	// stick turned to the aim's way, L and the C-stick kept, R short of its click.
	void padRead(SmsPadStatus& s, bool keyHeld)
	{
		held = keyHeld || (s.buttons & SMS_PAD_L) || s.trigger_l >= 100;
		if (!active)
			return;
		s.buttons &= (uint16_t)~SMS_PAD_L;
		s.trigger_l = 0;
		cx = s.cstick_x / 80.0f, cy = s.cstick_y / 80.0f;
		s.cstick_x = s.cstick_y = 0;
		// the stick: up walks where the aim points (the game reads it against its own camera)
		const float sx = s.stick_x, sy = s.stick_y;
		const V fa = forward(yaw, 0), ra = right(yaw), fg = forward(gameYaw, 0), rg = right(gameYaw);
		const V w    = fa * sy + ra * sx;
		float nx     = w.dot(rg), ny = w.dot(fg);
		const float c = 127.0f;
		nx = nx > c ? c : nx < -c ? -c : nx;
		ny = ny > c ? c : ny < -c ? -c : ny;
		s.stick_x = (int8_t)lrintf(nx);
		s.stick_y = (int8_t)lrintf(ny);
		// R short of its click: a full press would stand him still to aim
		s.buttons &= (uint16_t)~SMS_PAD_R;
		if (s.trigger_r > 200)
			s.trigger_r = 200;
	}

	// The crosshair's point: along the camera's line until a wall, the ground or a roof
	bool trace(const V& from, const V& way, V* hit) const
	{
		const float kStep = 60.0f, kFar = 9000.0f;
		V p = from;
		for (float d = kStep; d <= kFar; d += kStep) {
			const V q = from + way * d;
			SmsVec3 sq = q.s();
			if (SHOTS_HAS(api->game, SmsGameApi, wall_at) && api->game->wall_at(&sq, 15.0f, nullptr)) {
				*hit = p;
				return true;
			}
			SmsGround g = SmsGround();
			g.size      = sizeof g;
			if (SHOTS_HAS(api->game, SmsGameApi, ground) && api->game->ground(&sq, &g)) {
				if (q.y <= g.floor + 2.0f || q.y >= g.roof - 2.0f) {
					*hit = q;
					return true;
				}
			}
			p = q;
		}
		*hit = from + way * kFar;
		return false;
	}

	// Each frame's camera (SMS_EV_CAMERA, single player). `can`: aiming is allowed now (a stage
	// played, third person, not holding on to something, not in a talk).
	void camera(SmsEvCamera& c, bool can, float sensitivity, float dt)
	{
		const V gp(c.position), gt(c.target);
		const V gd = gt - gp;
		const float flat = sqrtf(gd.x * gd.x + gd.z * gd.z);
		const float gYaw = atan2f(gd.x, gd.z) / kDeg;
		const bool want = held && can;
		if (want && !active) { // aim from where the camera looks: no turn at the start
			yaw   = gYaw;
			pitch = flat > 1.0f ? atan2f(gd.y, flat) / kDeg : 0.0f;
			pitch = pitch > 20.0f ? 20.0f : pitch < -20.0f ? -20.0f : pitch;
		}
		active  = want;
		gameYaw = gYaw;

		// the blend in and out (about a fifth of a second)
		const float speed = dt * 6.0f;
		blend = active ? (blend + speed > 1.0f ? 1.0f : blend + speed) : (blend - speed < 0.0f ? 0.0f : blend - speed);

		V m;
		SmsPlayer pl;
		if (!marioAt(&m, &pl)) {
			release();
			return;
		}
		if (active) {
			float dx = 0, dy = 0;
			const SmsHeldFluddApi* h = api->held_fludd;
			if (SHOTS_HAS(h, SmsHeldFluddApi, take_mouse))
				h->take_mouse(&dx, &dy);
			const float s = 0.12f * sensitivity;
			const float ix = (invert & 2) ? -1.0f : 1.0f, iy = (invert & 1) ? -1.0f : 1.0f;
			yaw -= dx * s * ix;
			pitch -= dy * s * iy;
			yaw -= cx * 150.0f * dt * ix; // the C-stick, a controller's aim
			pitch += cy * 100.0f * dt * iy;
			pitch = pitch > 70.0f ? 70.0f : pitch < -60.0f ? -60.0f : pitch;
			while (yaw > 180.0f)
				yaw -= 360.0f;
			while (yaw < -180.0f)
				yaw += 360.0f;
		}
		if (blend <= 0.0f) {
			release();
			return;
		}

		// over the shoulder: behind him, a little right and up, kept out of the walls
		const V f     = forward(yaw, pitch);
		const V r     = right(yaw);
		const V pivot = m + V(0, 150.0f, 0) + r * 85.0f; // Mario a little left of the crosshair
		V want3 = pivot - f * 330.0f;
		const V back = want3 - pivot;
		const float len = back.len();
		V cam = want3;
		for (float d = 30.0f; d <= len; d += 30.0f) {
			const V q   = pivot + back * (d / len);
			SmsVec3 sq  = q.s();
			if (SHOTS_HAS(api->game, SmsGameApi, wall_at) && api->game->wall_at(&sq, 25.0f, nullptr)) {
				cam = pivot + back * ((d - 30.0f) / len);
				break;
			}
		}
		camAt = cam;
		const V tgt = cam + f * 1000.0f;
		const float b = blend * blend * (3.0f - 2.0f * blend); // smooth
		const V np = gp + (cam - gp) * b, nt = gt + (tgt - gt) * b;
		c.position = np.s();
		c.target   = nt.s();
		if (fov > 1.0f) // the aim's own field of view
			c.fovy = c.fovy + (fov - c.fovy) * b;

		if (!active) {
			release();
			return;
		}
		// the crosshair's point, and Mario turned to it: FLUDD's nozzle (about his shoulders) to it
		pointOk = trace(cam, f, &point);
		const V from = m + V(0, 110.0f, 0);
		V to = point - from;
		if (to.len() < 200.0f) // too near (a wall in his face): the aim's way
			to = f;
		const float tf = sqrtf(to.x * to.x + to.z * to.z);
		// Left and right: towards the crosshair's line where his water comes down (about kReach
		// away), not its far end, so the stream lands under the crosshair, not off to his side of it
		const float kReach = 800.0f;
		const float hitDist = (point - cam).len();
		const V landing = cam + f * (hitDist < kReach ? hitDist : kReach);
		const V side    = landing - from;
		const float ay  = (sqrtf(side.x * side.x + side.z * side.z) > 50.0f ? atan2f(side.x, side.z) : atan2f(to.x, to.z)) / kDeg;
		const float ap  = atan2f(to.y, tf) / kDeg;
		if (SHOTS_HAS(api->player, SmsPlayerApi, set_aim)) {
			api->player->set_aim(1, ay, ap);
			aimSet = true;
		}
	}

	void release()
	{
		if (aimSet && SHOTS_HAS(api->player, SmsPlayerApi, set_aim))
			api->player->set_aim(0, 0.0f, 0.0f);
		aimSet = false;
		if (blend <= 0.0f)
			active = false;
	}

	// The crosshair, in the middle of the picture: the First Person mod's (a ring and a dot, with an
	// outline that shows on any ground), in its colors
	void drawCrosshair(const SmsDrawApi& d, int ci) const
	{
		if (!active || blend < 0.6f)
			return;
		float w = 0, h = 0;
		d.canvas(&w, &h);
		const float x = w * 0.5f, y = h * 0.5f, r = h * 0.009f;
		const SmsColor c    = kCrossColors[ci >= 0 && ci < kCrossCount ? ci : 0];
		const SmsColor edge = ci == kCrossCount - 1 ? SMS_RGBA(255, 255, 255, 120) : SMS_RGBA(0, 0, 0, 110);
		d.circle_outline(x, y, r, r * 0.55f, edge);
		d.circle_outline(x, y, r, r * 0.25f, c);
		d.circle(x, y, r * 0.22f, c);
	}
};

} // namespace aim
