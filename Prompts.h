#pragma once
// Prompts: the stairs prompt (stay or descend), the restart prompt (R) and the death
// screen. Only one is open at a time and the game is paused while it is. Queued card
// picks always open first. Drawn with Dear ImGui until the pixel-font UI.

#include "GameObject.h"

class Prompts : public GameObject
{
private:
    enum class State { None, Stairs, Restart, Death };

    State m_State = State::None;
    bool  m_StairLock = false;      // chose "Stay": no prompt until the player steps off the stairs
    float m_DeathTimer = 0.0f;      // the death screen opens 0.9 s after HP hits 0
    bool  m_Leaving = false;        // a restart was chosen: stay paused while the screen fades out

    void Descend();
    void RestartRun(bool sameClass);
    void DrawStairs();
    void DrawRestart();
    void DrawDeath();

public:
    void Init() override;
    void Update() override;
};