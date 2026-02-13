#pragma once
#include "Screen.h"
#include <string>

class ScreenInfo : public Screen
{
public:
    enum class Kind
    {
        Info,
        Error
    };

    ScreenInfo(const char* label);

    void setKind(Kind k);
    void setTitle(std::string t);
    void setMessage(std::string msg);
    void setScroll(int amount);

    // Convenience
    void setInfo(std::string msg);
    void setError(std::string msg);

    void onSetActive(bool initial) override;
    void update(float dt) override;
    bool key_event(int key, int scancode, int action, int mods) override;

    Screen* prev = nullptr;

private:
    Kind kind = Kind::Info;
    std::string title = "--  I N F O  --";
    std::string message;
    int scroll = 0;

private:
    void draw() override;
};