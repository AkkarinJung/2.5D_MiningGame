// Text.cpp
// Translation table (std::map with a transparent comparator, so Tr() looks up a
// const char* without allocating) and the current language.

#include "main.h"
#include "Text.h"
#include "SaveData.h"
#include <map>
#include <string>
#include <fstream>

namespace
{
    Language g_Language = Language::English;
    std::map<std::string, std::string, std::less<>> g_Japanese;
}

Language CurrentLanguage()
{
    return g_Language;
}

void SetLanguage(Language lang)
{
    g_Language = lang;
    SaveData::Settings().language = lang;
    SaveData::SaveSettings();
}

void ToggleLanguage()
{
    SetLanguage(g_Language == Language::English ? Language::Japanese : Language::English);
}

void LoadTranslations()
{
    g_Japanese.clear();
    std::ifstream file("asset\\text\\ja.txt", std::ios::binary);
    if (!file)
    {
        OutputDebugStringA("asset\\text\\ja.txt not found: Japanese falls back to English\n");
        return;
    }

    std::string line;
    bool firstLine = true;
    while (std::getline(file, line))
    {
        if (firstLine && line.size() >= 3 && (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
            line.erase(0, 3);                       // skip a UTF-8 BOM if the editor added one
        firstLine = false;

        if (!line.empty() && line.back() == '\r')
            line.pop_back();                        // Windows line endings
        if (line.empty() || line[0] == '#')
            continue;                               // blank lines and comments
        const size_t tab = line.find('\t');
        if (tab == std::string::npos)
            continue;
        g_Japanese[line.substr(0, tab)] = line.substr(tab + 1);
    }

    g_Language = SaveData::Settings().language;
}

const char* Tr(const char* english)
{
    if (g_Language != Language::Japanese || english == nullptr)
        return english;
    const auto it = g_Japanese.find(english);
    return it != g_Japanese.end() ? it->second.c_str() : english;
}

const char* LanguageButtonLabel()
{
    // "日本語" as UTF-8 bytes, so this file does not depend on the source encoding
    return g_Language == Language::English ? "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" : "English";
}
