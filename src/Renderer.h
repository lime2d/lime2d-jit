#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>

class Renderer
{
public:
    inline static bool ready = false;

    Renderer();

    void init();
    void uploadSSBO();
    void render();
    void cleanup();

    void setFgColor(float r, float g, float b);
    void setBgColor(float r, float g, float b);

private:
    GLuint shaderProgram;
    GLuint vao, vbo, ebo;
    GLuint ssbo; // SSBO for monochrome canvas

    void setupShaders();
    void setupQuad();
};

#endif