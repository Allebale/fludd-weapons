// Coin Magnet: the Rifle's shots pull the coins they pass close to over to the player who fired, who
// then picks them up as ever (the game's own touch: the coin's sound, count and health).
//
// Only mod API 1.8's general tools: the coins are learned as their stage loads (SMS_EV_ITEM_SETUP:
// every coin, red and blue ones and those an enemy or a box will drop), and moved each tick with
// actor->set_position. A coin that is gone (picked up) or not out (a box's, still inside) is left alone.
#pragma once
#include "shots.h"

namespace coins {

using shots::V;

struct Magnet {
	const SmsApi* api = nullptr;

	static const int kCoins = 1024;
	SmsObject coin[kCoins];
	int count  = 0;
	bool fresh = true; // a stage loaded since the last coin: the next coin starts a new list

	// coins flying to a player
	static const int kPulled = 64;
	struct Pull {
		SmsObject coin;
		int player;
		double until;
	};
	Pull pulled[kPulled];
	int pulling = 0;

	bool hasTools() const
	{
		const SmsActorApi* a = api ? api->actor : nullptr;
		return SHOTS_HAS(a, SmsActorApi, set_position) && SHOTS_HAS(a, SmsActorApi, flags) &&
		       SHOTS_HAS(api->game, SmsGameApi, object_position);
	}

	void stageLoaded()
	{
		fresh   = true;
		pulling = 0;
	}
	void itemSetup(const SmsEvItemSetup& e)
	{
		if (fresh) {
			count = 0;
			fresh = false;
		}
		if (e.object && count < kCoins)
			coin[count++] = e.object;
	}

	// out in the stage now (not picked up, not inside its box or enemy)
	bool out(SmsObject c) const
	{
		const uint32_t f = api->actor->flags(c);
		return f && !(f & (SMS_ACTOR_FLAG_DEAD | SMS_ACTOR_FLAG_HIDDEN));
	}
	bool where(SmsObject c, V* at) const
	{
		SmsVec3 p;
		if (!api->game->object_position(c, &p))
			return false;
		*at = V(p);
		return true;
	}
	bool isPulled(SmsObject c) const
	{
		for (int i = 0; i < pulling; i++)
			if (pulled[i].coin == c)
				return true;
		return false;
	}

	// A Rifle shot from `from` along `way` for `len`: the coins within `reach` of its line start
	// flying to `player`.
	void shot(int player, const V& from, const V& way, float len, float reach, double now)
	{
		if (!hasTools() || len <= 0.0f)
			return;
		for (int i = 0; i < count && pulling < kPulled; i++) {
			const SmsObject c = coin[i];
			V p;
			if (isPulled(c) || !out(c) || !where(c, &p))
				continue;
			const float along = (p - from).dot(way);
			if (along < 0.0f || along > len + reach)
				continue;
			const V off = p - (from + way * along);
			if (off.len() > reach)
				continue;
			Pull& n  = pulled[pulling++];
			n.coin   = c;
			n.player = player;
			n.until  = now + 3.0;
		}
	}

	// Each tick: each pulled coin a step nearer its player (`playerAt` gives where that player is),
	// fast at first and slowing as it arrives at his middle, where he picks it up.
	template <typename PlayerAt>
	void tick(double now, float secs, PlayerAt playerAt)
	{
		if (!pulling || !hasTools())
			return;
		int kept = 0;
		for (int i = 0; i < pulling; i++) {
			Pull pl = pulled[i];
			V p, m;
			if (now > pl.until || !out(pl.coin) || !where(pl.coin, &p) || !playerAt(pl.player, &m))
				continue; // picked up (or gone, or taking too long)
			m.y += 60.0f; // his middle
			const V d    = m - p;
			const float l = d.len();
			const float step = (l * 6.0f + 1500.0f) * secs; // units this tick
			const V to = step >= l ? m : p + d * (step / l);
			SmsVec3 s = to.s();
			api->actor->set_position(pl.coin, &s);
			pulled[kept++] = pl;
		}
		pulling = kept;
	}
};

} // namespace coins
