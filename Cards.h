#pragma once
// Cards: the level-up card pick and the player's deck.
// Each mining level-up queues a pick tagged with the ore grade that caused it; while a
// pick is open the game is paused and 3 cards (Miner 4) are offered with odds set by
// that ore, shifted rarer by mining level and Prospector.

#include "GameObject.h"
#include "CardData.h"
#include <vector>
#include <random>

struct PendingPick
{
    int oreGrade;
    int level;
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

    void QueueLevelUp(int oreGrade, int level) { m_Pending.push_back({ oreGrade, level }); }
    void Grant(CardId id);              // apply a card and add it to the deck (also used by the debug panel)

    int  Owned(CardId id) const { return m_Deck[(int)id]; }
    bool IsPicking() const { return !m_Pending.empty(); }

    // Rarity odds (common, rare, epic) for a level-up caused by this ore grade.
    void OddsFor(int oreGrade, float out[3]) const;
};

