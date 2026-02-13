#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "Window.h"

void key_callback(GLFWwindow* glfw_window, int key, int scancode, int action, int mods);
void char_callback(GLFWwindow* window, unsigned int c);

#endif