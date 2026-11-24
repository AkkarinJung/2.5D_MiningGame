#pragma once
// SaveData: settings and records, not runs in progress (dying or quitting ends the run).
// Two small JSON files in %LOCALAPPDATA%\Deepvein\, written to a .tmp file first and then
// swapped in, so a crash mid-write never leaves a broken file. A bad file is renamed to
// .bak and replaced with defaults instead of crashing.

#include "ClassData.h"
#include "Text.h"

namespace SaveData
{
    struct SettingsData
    {
        Language language = Language::English;
    };

    struct RecordsData
    {
        int deepestFloor = 1;
        int deepestByClass[kClassCount] = { 1, 1, 1, 1 };
        int runs = 0;
    };

    void Load();                    // once at startup (picks the system language the first time)
    SettingsData& Settings();
    const RecordsData& Records();
    void SaveSettings();

    void RecordFloor(PlayerClass cls, int floor);   // reached a floor: update deepest records
    void RecordRunEnd(PlayerClass cls, int floor);  // died or restarted: +1 run
}

