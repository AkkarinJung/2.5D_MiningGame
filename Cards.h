#pragma once
// Cards: the level-up card pick and the player's deck.
// Each level-up queues a pick tagged with what caused it: a monster kill (C4, odds from the
// monster's card tier) or, until C2 removes mining XP, a mining level-up (odds from the ore).
// While a pick is open the game is paused and 3 cards (Miner 4) are offered, shifted rarer
// by level and Prospector.

#include "GameObject.h"
#include "CardData.h"
#include "MonsterData.h"
#include <vector>
#include <random>

struct PendingPick
{
    bool fromKill;          // true: combat level-up (C4); false: mining level-up
    int  grade;             // rarity row: the ore grade, or the monster's card tier
    int  level;             // the level just reached
    MonsterType killer;     // the monster that caused it (fromKill only)
};

class Cards : public GameObject
{
private:
    std::vector<PendingPick> m_Pending;
    int m_Deck[kCardCount] = {};        // copies owned of each card
    std::mt19937 m_Rng;

    bool m_HandOpen = false;            // the current pick has been dealt
    int  m_Hand[4] = {};
    int  m_HandSize = 0;
    float m_Odds[3] = {};

    void DealHand(const PendingPick& pick);
    void Choose(int slot);

public:
    void Init() override;
    void Update() override;

    void QueueLevelUp(int oreGrade, int level) { m_Pending.push_back({ false, oreGrade, level, MonsterType::Slime }); }
    void QueueKillLevelUp(MonsterType killer, int level)
    {
        m_Pending.push_back({ true, kMonster[(int)killer].cardTier, level, killer });
    }
    void Grant(CardId id);              // apply a card and add it to the deck (also used by the debug panel)

    int  Owned(CardId id) const { return m_Deck[(int)id]; }
    int  TotalOwned() const
    {
        int n = 0;
        for (int c : m_Deck)
            n += c;
        return n;
    }
    bool IsPicking() const { return !m_Pending.empty(); }

    // Rarity odds (common, rare, epic) for rarity row `grade` (ore grade or monster card tier,
    // same rows), shifted rarer by 0.03 per level above 1 and by Prospector.
    void OddsFor(int grade, int level, float out[3]) const;
};

