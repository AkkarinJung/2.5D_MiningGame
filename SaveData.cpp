// SaveData.cpp
// %LOCALAPPDATA%\Deepvein\settings.json and records.json. The format is tiny and flat,
// so it is written and read by hand here instead of pulling in a JSON library.

#include "main.h"
#include "SaveData.h"
#include <shlobj.h>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace
{
    SaveData::SettingsData g_Settings;
    SaveData::RecordsData  g_Records;
    std::wstring g_Folder;
    char g_Status[160] = "not loaded yet";

    void SetStatus(const char* text)
    {
        strncpy_s(g_Status, text, _TRUNCATE);
        OutputDebugStringA("Deepvein save: ");
        OutputDebugStringA(text);
        OutputDebugStringA("\n");
    }

    const char* const kClassKey[kClassCount] = { "warrior", "miner", "mage", "rogue" };

    std::wstring SaveFolder()
    {
        if (!g_Folder.empty())
            return g_Folder;
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
        {
            g_Folder = std::wstring(path) + L"\\Deepvein";
            CoTaskMemFree(path);
            CreateDirectoryW(g_Folder.c_str(), nullptr);    // fine if it already exists
        }
        else
            SetStatus("ERROR: could not find %LOCALAPPDATA%");
        return g_Folder;
    }

    std::wstring FilePath(const wchar_t* name)
    {
        return SaveFolder() + L"\\" + name;
    }

    bool ReadFile(const std::wstring& path, std::string& out)
    {
        std::ifstream file(path.c_str(), std::ios::binary);
        if (!file)
            return false;
        std::stringstream buffer;
        buffer << file.rdbuf();
        out = buffer.str();
        return true;
    }

    // Write to name.tmp, then replace the real file in one step.
    bool WriteFileSafely(const std::wstring& path, const std::string& text)
    {
        const std::wstring tmp = path + L".tmp";
        {
            std::ofstream file(tmp.c_str(), std::ios::binary | std::ios::trunc);
            if (!file)
            {
                SetStatus("ERROR: could not create the .tmp save file");
                return false;
            }
            file << text;
            if (!file)
            {
                SetStatus("ERROR: could not write the .tmp save file");
                return false;
            }
        }
        if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            char msg[96];
            sprintf_s(msg, "ERROR: could not replace the save file (Windows error %lu)", GetLastError());
            SetStatus(msg);
            return false;
        }
        return true;
    }

    void KeepBadFile(const std::wstring& path)
    {
        const std::wstring bak = path + L".bak";
        MoveFileExW(path.c_str(), bak.c_str(), MOVEFILE_REPLACE_EXISTING);
        OutputDebugStringA("Deepvein: a save file could not be read; renamed to .bak and reset\n");
    }

    // Finds "key": and reads the number after it. Searches from 'from' onwards.
    bool ReadInt(const std::string& json, const char* key, int& out, size_t from = 0)
    {
        const std::string quoted = std::string("\"") + key + "\"";
        size_t pos = json.find(quoted, from);
        if (pos == std::string::npos)
            return false;
        pos = json.find(':', pos + quoted.size());
        if (pos == std::string::npos)
            return false;
        char* end = nullptr;
        const long value = strtol(json.c_str() + pos + 1, &end, 10);
        if (end == json.c_str() + pos + 1)
            return false;
        out = (int)value;
        return true;
    }

    void SaveRecords()
    {
        std::ostringstream s;
        s << "{\n  \"version\": 1,\n  \"deepestFloor\": " << g_Records.deepestFloor << ",\n  \"deepestByClass\": { ";
        for (int i = 0; i < kClassCount; i++)
            s << "\"" << kClassKey[i] << "\": " << g_Records.deepestByClass[i] << (i + 1 < kClassCount ? ", " : " ");
        s << "},\n  \"runs\": " << g_Records.runs << "\n}\n";
        if (WriteFileSafely(FilePath(L"records.json"), s.str()))
        {
            char msg[96];
            sprintf_s(msg, "saved records.json (deepest floor %d, runs %d)", g_Records.deepestFloor, g_Records.runs);
            SetStatus(msg);
        }
    }
}

namespace SaveData
{
    void Load()
    {
        // Settings: language. First run: follow the Windows display language.
        std::string text;
        const std::wstring settingsPath = FilePath(L"settings.json");
        if (ReadFile(settingsPath, text))
        {
            int version = 0;
            if (ReadInt(text, "version", version) && text.find("\"language\"") != std::string::npos)
                g_Settings.language = text.find("\"ja\"") != std::string::npos ? Language::Japanese : Language::English;
            else
                KeepBadFile(settingsPath);
        }
        else
            g_Settings.language = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_JAPANESE
            ? Language::Japanese : Language::English;

        // Records
        const std::wstring recordsPath = FilePath(L"records.json");
        if (ReadFile(recordsPath, text))
        {
            RecordsData r;
            int version = 0;
            bool ok = ReadInt(text, "version", version) && ReadInt(text, "deepestFloor", r.deepestFloor) &&
                ReadInt(text, "runs", r.runs);
            const size_t byClass = text.find("\"deepestByClass\"");
            for (int i = 0; ok && byClass != std::string::npos && i < kClassCount; i++)
                ReadInt(text, kClassKey[i], r.deepestByClass[i], byClass);
            if (ok)
            {
                g_Records = r;
                char msg[96];
                sprintf_s(msg, "loaded records.json (deepest floor %d, runs %d)", r.deepestFloor, r.runs);
                SetStatus(msg);
            }
            else
            {
                KeepBadFile(recordsPath);
                SetStatus("records.json was unreadable: renamed to .bak, starting fresh");
            }
        }
        else
            SetStatus("no records.json yet (it is written on a new deepest floor, death or restart)");
    }

    SettingsData& Settings()
    {
        return g_Settings;
    }

    const RecordsData& Records()
    {
        return g_Records;
    }

    void SaveSettings()
    {
        std::string text = "{\n  \"version\": 1,\n  \"language\": \"";
        text += g_Settings.language == Language::Japanese ? "ja" : "en";
        text += "\"\n}\n";
        if (WriteFileSafely(FilePath(L"settings.json"), text))
            SetStatus(g_Settings.language == Language::Japanese ? "saved settings.json (ja)" : "saved settings.json (en)");
    }

    void RecordFloor(PlayerClass cls, int floor)
    {
        bool changed = false;
        if (floor > g_Records.deepestFloor)
        {
            g_Records.deepestFloor = floor;
            changed = true;
        }
        if (floor > g_Records.deepestByClass[(int)cls])
        {
            g_Records.deepestByClass[(int)cls] = floor;
            changed = true;
        }
        if (changed)
            SaveRecords();
    }

    void RecordRunEnd(PlayerClass cls, int floor)
    {
        g_Records.runs++;
        RecordFloor(cls, floor);
        SaveRecords();
    }

    const wchar_t* Folder()
    {
        SaveFolder();                   // fills g_Folder the first time
        return g_Folder.c_str();
    }

    const char* LastStatus()
    {
        return g_Status;
    }

    void ResetRecords()
    {
        g_Records = RecordsData();
        SaveRecords();
    }
}
