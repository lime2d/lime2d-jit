#ifndef SCREEN_LUA_H
#define SCREEN_LUA_H

#include "Screen.h"

class ScreenLua : public Screen
{
public:
    ScreenLua(const char* label);

    void onSetActive(bool initial) override;
    void update(float dt) override;
    bool key_event(int key, int scancode, int action, int mods) override;
    bool char_event(unsigned int c) override;

private:
    void draw() override;
    void showSystemInfoScreen();
};

#endif