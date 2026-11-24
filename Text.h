#pragma once
// Text: English / Japanese UI text.
// Code is written with English strings; Tr(english) returns the Japanese line from
// asset\text\ja.txt when Japanese is selected, or the English string itself otherwise
// (and whenever a line has no translation yet). ja.txt is UTF-8, one "English<TAB>Japanese"
// pair per line, so translations can be edited without touching code or source encodings.
// Format strings (%s, %d) must keep the same order of arguments in both languages.

enum class Language { English, Japanese };

Language CurrentLanguage();
void SetLanguage(Language lang);        // also saves the choice (settings.json)
void ToggleLanguage();

void LoadTranslations();                // reads asset\text\ja.txt once at startup
const char* Tr(const char* english);

// Label for the language button: shows the language you would switch TO.
const char* LanguageButtonLabel();
