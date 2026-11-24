// Coin Magnet: an enemy defeated by a Rifle shot through the scope sends the coins it drops over to
// the player who fired, who then picks them up as ever (the game's own touch: the coin's sound, count
// and health).
//
// Only mod API 1.8's general tools. The Rifle's defeat is the mod's own (Engine::onDefeat); the coins
// the enemy drops are found round where it died with actor->near (the stage's item manager is one of
// the live managers it looks through; a coin waiting in its pool is hidden, so it is found only once
// it is out) and told apart by their actor type; they are moved each tick with actor->set_position.
#pragma once
#include "shots.h"

namespace coins {

using shots::V;

// The game's coin actor types (TCoin, TCoinRed, TCoinBlue)
inline bool isCoin(uint32_t type) { return type == 0x2000000Eu || type == 0x2000000Fu || type == 0x20000010u; }

struct Magnet {
	const SmsApi* api = nullptr;
	SmsMod* mod       = nullptr;

	bool hasTools() const
	{
		const SmsActorApi* a = api ? api->actor : nullptr;
		return SHOTS_HAS(a, SmsActorApi, set_position) && SHOTS_HAS(a, SmsActorApi, flags) &&
		       SHOTS_HAS(a, SmsActorApi, near) && SHOTS_HAS(api->game, SmsGameApi, object_position) &&
		       SHOTS_HAS(api->game, SmsGameApi, object_type);
	}

	// coins flying to a player
	static const int kPulled = 64;
	struct Pull {
		SmsObject coin;
		int player;
		double until;
	};
	Pull pulled[kPulled];
	int pulling = 0;

	// the places of the Rifle's kills, watched a while for the coins the enemy drops there
	static const int kWatch = 8, kSeen = 32;
	static constexpr float kDropReach = 600.0f; // how far round it its coins pop out and land
	struct Spot {
		V at;
		int player   = 0;
		double until = -100;
		SmsObject seen[kSeen]; // the coins already out there as it died: not its drops
		int seenCount = 0;
	};
	Spot spots[kWatch];

	// each player's last Rifle shot through the scope (a shot's defeat may come as the zoom ends)
	double scopedAt[2] = {-100, -100};

	void stageLoaded()
	{
		pulling    = 0;
		freshCount = 0;
		for (int w = 0; w < kWatch; w++)
			spots[w].until = -100;
		scopedAt[0] = scopedAt[1] = -100;
	}
	void scopedShot(int player, double now) { scopedAt[player & 1] = now; }

	// coins just made to appear (SMS_EV_ACTOR_SPAWN: an enemy's drops come out of the item manager's
	// pool), judged next tick, when the enemy has put them where it is
	static const int kNew = 32;
	struct Fresh {
		SmsObject coin;
		double until;
	};
	Fresh fresh[kNew];
	int freshCount = 0;
	bool watching(double now) const
	{
		for (int w = 0; w < kWatch; w++)
			if (now <= spots[w].until)
				return true;
		return false;
	}
	void spawned(SmsObject o, uint32_t type, double now)
	{
		if (!isCoin(type) || !watching(now) || freshCount >= kNew)
			return;
		fresh[freshCount].coin  = o;
		fresh[freshCount].until = now + 0.5;
		freshCount++;
		if (api->core)
			api->core->log(mod, SMS_LOG_INFO, "coin magnet: a coin came out while watching a kill");
	}
	// a coin near a watched kill: off to its player
	bool take(SmsObject c, const V& p, double now)
	{
		for (int w = 0; w < kWatch && pulling < kPulled; w++) {
			Spot& sp = spots[w];
			if (now > sp.until || (p - sp.at).len() > kDropReach * 2.0f)
				continue;
			for (int k = 0; k < sp.seenCount; k++)
				if (sp.seen[k] == c)
					return false;
			if (isPulled(c))
				return false;
			if (sp.seenCount < kSeen)
				sp.seen[sp.seenCount++] = c;
			Pull& n  = pulled[pulling++];
			n.coin   = c;
			n.player = sp.player;
			n.until  = now + 4.0;
			if (api->core)
				api->core->log(mod, SMS_LOG_INFO, "coin magnet: a coin on its way to player %d", sp.player + 1);
			return true;
		}
		return false;
	}

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
	// the coins out round `at` into out; returns how many
	int coinsNear(const V& at, SmsObject* out, int most) const
	{
		static SmsObject all[256];
		SmsVec3 p = at.s();
		int n     = api->actor->near(&p, kDropReach, 0, all, 256);
		if (n > 256)
			n = 256;
		int k = 0;
		for (int i = 0; i < n && k < most; i++)
			if (isCoin(api->game->object_type(all[i])))
				out[k++] = all[i];
		return k;
	}

	// The Rifle defeated an enemy at `at` (`zoomed`: its player is looking through the scope now):
	// the coins it drops there fly to that player.
	void defeated(int player, const V& at, bool zoomed, double now)
	{
		player &= 1;
		if (!hasTools() || (!zoomed && now - scopedAt[player] > 2.0))
			return;
		Spot* sp = &spots[0];
		for (int i = 1; i < kWatch; i++)
			if (spots[i].until < sp->until)
				sp = &spots[i];
		sp->at        = at;
		sp->player    = player;
		sp->until     = now + 6.0; // its coins come out as its defeat ends
		sp->seenCount = coinsNear(at, sp->seen, kSeen);
		if (api->core)
			api->core->log(mod, SMS_LOG_INFO, "coin magnet: watching a Rifle kill for its coins (%d coins already there)",
			               sp->seenCount);
	}

	void watch(double now)
	{
		// the coins made to appear lately, now where their enemy put them
		int kept = 0;
		for (int i = 0; i < freshCount; i++) {
			V p;
			if (out(fresh[i].coin) && where(fresh[i].coin, &p) && take(fresh[i].coin, p, now))
				continue;
			if (now <= fresh[i].until)
				fresh[kept++] = fresh[i];
		}
		freshCount = kept;
		// and any coin out round a watched kill
		for (int w = 0; w < kWatch; w++) {
			Spot& sp = spots[w];
			if (now > sp.until)
				continue;
			SmsObject found[32];
			const int n = coinsNear(sp.at, found, 32);
			for (int i = 0; i < n; i++) {
				V p;
				if (where(found[i], &p))
					take(found[i], p, now);
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
		int kept = 0;
		for (int i = 0; i < pulling; i++) {
			Pull pl = pulled[i];
			V p, m;
			if (now > pl.until || !out(pl.coin) || !where(pl.coin, &p) || !playerAt(pl.player, &m))
				continue; // picked up (or gone, or taking too long)
			m.y += 60.0f; // his middle
			const V d     = m - p;
			const float l = d.len();
			const float step = (l * 6.0f + 1500.0f) * secs; // units this tick
			const V to = step >= l ? m : p + d * (step / l);
			SmsVec3 s  = to.s();
			api->actor->set_position(pl.coin, &s);
			SmsVec3 still = {0, 0, 0};
			if (SHOTS_HAS(api->actor, SmsActorApi, set_velocity))
				api->actor->set_velocity(pl.coin, &still); // no bounce of its own fighting the pull
			pulled[kept++] = pl;
		}
		pulling = kept;
	}
};

} // namespace coins
