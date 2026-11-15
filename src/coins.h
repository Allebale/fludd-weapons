// Coin Magnet: an enemy defeated by a Rifle shot through the scope sends the coins it drops over to
// the player who fired, who then picks them up as ever (the game's own touch: the coin's sound, count
// and health).
//
// Only mod API 1.8's general tools: the coins are learned as their stage loads (SMS_EV_ITEM_SETUP:
// every coin, red and blue ones and those an enemy or a box will drop), and moved each tick with
// actor->set_position. The kill is known by SMS_EV_ACTOR_DEATH near the line of a scoped Rifle shot
// of the last 2 seconds; its drops are the coins that come out near it after it died.
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
		for (int w = 0; w < kWatch; w++)
			spots[w].until = -100;
		lastShot[0].at = lastShot[1].at = -100;
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

	// Rifle shots through the scope, each player's last: an enemy defeated near one's line soon after
	// is the Rifle's kill
	struct Shot {
		V from, way;
		float len = 0;
		double at = -100;
	};
	Shot lastShot[2];
	void scopedShot(int player, const V& from, const V& way, float len, double now)
	{
		Shot& s = lastShot[player & 1];
		s.from = from, s.way = way, s.len = len, s.at = now;
	}

	// The places of the Rifle's kills, watched a while for the coins the enemy drops there
	static const int kWatch = 8;
	struct Spot {
		V at;
		int player = 0;
		double until = -100;
		uint32_t seen[kCoins / 32]; // the coins already out there as it died: not its drops
	};
	Spot spots[kWatch];
	static constexpr float kDropReach = 350.0f; // how far round it its coins land

	// An enemy defeated at `at`: when it was near a player's scoped Rifle shot of the last 2 seconds,
	// the coins it drops there fly to that player.
	void defeated(const V& at, double now)
	{
		if (!hasTools())
			return;
		int who = -1;
		for (int p = 0; p < 2; p++) {
			const Shot& s = lastShot[p];
			if (now - s.at > 2.0)
				continue;
			const float along = (at - s.from).dot(s.way);
			if (along < -200.0f || along > s.len + 400.0f)
				continue;
			if ((at - (s.from + s.way * along)).len() < 400.0f)
				who = p;
		}
		if (who < 0)
			return;
		Spot* sp = &spots[0];
		for (int i = 1; i < kWatch; i++)
			if (spots[i].until < sp->until)
				sp = &spots[i];
		sp->at     = at;
		sp->player = who;
		sp->until  = now + 2.5;
		for (int i = 0; i < kCoins / 32; i++)
			sp->seen[i] = 0;
		for (int i = 0; i < count; i++) {
			V p;
			if (out(coin[i]) && where(coin[i], &p) && (p - at).len() < kDropReach * 2.0f)
				sp->seen[i >> 5] |= 1u << (i & 31);
		}
	}

	// the coins that have come out near a watched kill since it died: off to its player
	void watch(double now)
	{
		for (int w = 0; w < kWatch; w++) {
			Spot& sp = spots[w];
			if (now > sp.until)
				continue;
			for (int i = 0; i < count && pulling < kPulled; i++) {
				if (sp.seen[i >> 5] & (1u << (i & 31)))
					continue;
				V p;
				const SmsObject c = coin[i];
				if (!out(c) || !where(c, &p) || (p - sp.at).len() > kDropReach * 2.0f)
					continue;
				sp.seen[i >> 5] |= 1u << (i & 31);
				if (isPulled(c))
					continue;
				Pull& n  = pulled[pulling++];
				n.coin   = c;
				n.player = sp.player;
				n.until  = now + 4.0;
			}
		}
	}

	// Each tick: each pulled coin a step nearer its player (`playerAt` gives where that player is),
	// fast at first and slowing as it arrives at his middle, where he picks it up.
	template <typename PlayerAt>
	void tick(double now, float secs, PlayerAt playerAt)
	{
		if (!hasTools())
			return;
		watch(now);
		if (!pulling)
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
