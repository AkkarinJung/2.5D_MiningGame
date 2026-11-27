#pragma once
// Inventory (C1): the player's bag. A fixed array of slots (no heap): 12 at the start,
// up to 20 with Bigger Bag upgrades. Each slot holds one material type, stacked up to
// that material's stack size (MaterialData.h). The Player owns one Inventory.

#include "MaterialData.h"

class Inventory
{
public:
    struct Slot
    {
        MatId   id = MatId::Stone;
        uint8_t n = 0;
        bool    used = false;
    };

    static const int kMaxSlots = kBagSlotsMax;

private:
    Slot m_Slots[kMaxSlots];
    int  m_SlotCount = kBagSlotsStart;

public:
    void Clear();                           // empty every slot (slot count unchanged)
    int  SlotCount() const { return m_SlotCount; }
    void SetSlotCount(int count);           // clamped to kBagSlotsStart..kMaxSlots; never deletes items
    const Slot& GetSlot(int i) const { return m_Slots[i]; }
    int  UsedSlots() const;

    int  Count(MatId id) const;             // how many of this material the bag holds
    int  Room(MatId id) const;              // how many more would fit (existing stacks + empty slots)
    int  Add(MatId id, int n);              // fills existing stacks, then empty slots; returns how many fit
    void Take(MatId id, int n);             // removes up to n, from the last stacks first

    bool CanPay(const Cost& cost) const;
    void Pay(const Cost& cost);
    // Room for `result` after paying `cost` (crafting): works on a copy, the bag is unchanged.
    int  RoomAfterPay(const Cost& cost, MatId result) const;

    // Empty slot i; writes what was in it. False if the slot was empty or out of range.
    bool RemoveSlot(int i, MatId& id, int& n);
};
