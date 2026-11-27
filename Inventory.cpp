// Inventory.cpp
// Plain array code: no allocation, no game objects. Slots past m_SlotCount are never used.

#include "Inventory.h"
#include <algorithm>

void Inventory::Clear()
{
    for (Slot& s : m_Slots)
        s = Slot();
}

void Inventory::SetSlotCount(int count)
{
    const int maxSlots = kMaxSlots;         // a copy: std::min takes references
    count = std::min(std::max(count, (int)kBagSlotsStart), maxSlots);
    // Shrinking (debug panel only) must not lose items: keep enough slots for the used ones
    int lastUsed = -1;
    for (int i = 0; i < kMaxSlots; i++)
        if (m_Slots[i].used)
            lastUsed = i;
    m_SlotCount = std::max(count, lastUsed + 1);
}

int Inventory::UsedSlots() const
{
    int used = 0;
    for (int i = 0; i < m_SlotCount; i++)
        if (m_Slots[i].used)
            used++;
    return used;
}

int Inventory::Count(MatId id) const
{
    int total = 0;
    for (int i = 0; i < m_SlotCount; i++)
        if (m_Slots[i].used && m_Slots[i].id == id)
            total += m_Slots[i].n;
    return total;
}

int Inventory::Room(MatId id) const
{
    const int stack = kMat[(int)id].stack;
    int room = 0;
    for (int i = 0; i < m_SlotCount; i++)
    {
        if (!m_Slots[i].used)
            room += stack;
        else if (m_Slots[i].id == id)
            room += stack - m_Slots[i].n;
    }
    return room;
}

int Inventory::Add(MatId id, int n)
{
    if (n <= 0)
        return 0;
    const int stack = kMat[(int)id].stack;
    int left = n;

    // 1. Top up stacks of the same material
    for (int i = 0; i < m_SlotCount && left > 0; i++)
    {
        Slot& s = m_Slots[i];
        if (s.used && s.id == id && s.n < stack)
        {
            const int put = std::min(left, stack - (int)s.n);
            s.n = (uint8_t)(s.n + put);
            left -= put;
        }
    }
    // 2. Start new stacks in empty slots
    for (int i = 0; i < m_SlotCount && left > 0; i++)
    {
        Slot& s = m_Slots[i];
        if (!s.used)
        {
            const int put = std::min(left, stack);
            s.used = true;
            s.id = id;
            s.n = (uint8_t)put;
            left -= put;
        }
    }
    return n - left;
}

void Inventory::Take(MatId id, int n)
{
    for (int i = m_SlotCount - 1; i >= 0 && n > 0; i--)
    {
        Slot& s = m_Slots[i];
        if (!s.used || s.id != id)
            continue;
        const int take = std::min(n, (int)s.n);
        s.n = (uint8_t)(s.n - take);
        n -= take;
        if (s.n == 0)
            s = Slot();
    }
}

bool Inventory::CanPay(const Cost& cost) const
{
    for (int k = 0; k < cost.count; k++)
        if (Count(cost.item[k].id) < cost.item[k].n)
            return false;
    return true;
}

void Inventory::Pay(const Cost& cost)
{
    for (int k = 0; k < cost.count; k++)
        Take(cost.item[k].id, cost.item[k].n);
}

int Inventory::RoomAfterPay(const Cost& cost, MatId result) const
{
    Inventory copy = *this;                 // 20 small slots: cheap to copy
    copy.Pay(cost);
    return copy.Room(result);
}

bool Inventory::RemoveSlot(int i, MatId& id, int& n)
{
    if (i < 0 || i >= m_SlotCount || !m_Slots[i].used)
        return false;
    id = m_Slots[i].id;
    n = m_Slots[i].n;
    m_Slots[i] = Slot();
    return true;
}
