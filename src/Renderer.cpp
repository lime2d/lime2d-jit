#include "App.h"
#include "Renderer.h"
#include "Window.h"
#include "Screen.h"
#include <iostream>
#include "misc.h"

// Project requirement: Code for shaders must be embedded in this file (final distributable must be a single exe)

const char* vertexShaderSource = R"glsl(
#version 430 core
layout (location = 0) in vec2 aPos;
uniform vec2 viewport;
uniform vec2 offset;
uniform float scale;
out vec2 fragCoord;
void main()
{
    vec2 pos = (aPos * scale + offset) / viewport * 2.0 - 1.0;
    pos.y = -pos.y;
    gl_Position = vec4(pos, 0.0, 1.0);
    fragCoord = aPos;
}
)glsl";

const char* fragmentShaderSource = R"glsl(
#version 430 core
layout (std430, binding = 0) buffer PixelBuffer {
    uint pixels[];
};
uniform vec2 canvasSize;
uniform vec3 fgColor;
uniform vec3 bgColor;
in vec2 fragCoord;
out vec4 FragColor;

void main()
{
    ivec2 pixelCoord = ivec2(fragCoord);
    int i = pixelCoord.y * int(canvasSize.x) + pixelCoord.x;
    //FragColor = (pixels[i / 32] & (1 << (i % 32))) != 0 ? vec4(fgColor, 1.0) : vec4(bgColor, 1.0);
    FragColor = vec4((pixels[i / 32] & (1 << (i % 32))) != 0 ? fgColor : bgColor, 1.0);
}
)glsl";

Renderer::Renderer() : shaderProgram(0), vao(0), vbo(0), ebo(0), ssbo(0) { init(); }

void Renderer::cleanup()
{
    ready = false;

    glDeleteBuffers(1, &ssbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(shaderProgram);

    cout(" Renderer [ok]");
}

void Renderer::init()
{
    setupShaders();
    setupQuad();

    // Create SSBO
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, Screen::width * Screen::height / 8, Screen::pixels, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    ready = true;
    cout(" Renderer [ready]");
}

void Renderer::setupShaders()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    int success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, sizeof(infoLog), NULL, infoLog);
        std::cerr << "Vertex Shader compilation failed:\n" << infoLog << "\n";
        app.fatal();
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, sizeof(infoLog), NULL, infoLog);
        std::cerr << "Fragment Shader compilation failed:\n" << infoLog << "\n";
        app.fatal();
    }

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // Check linking
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, sizeof(infoLog), NULL, infoLog);
        std::cerr << "Shader Program linking failed:\n" << infoLog << "\n";
        app.fatal();
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glUseProgram(shaderProgram);
    glUniform2f(glGetUniformLocation(shaderProgram, "canvasSize"), static_cast<GLfloat>(Screen::width), static_cast<GLfloat>(Screen::height));
    glUniform3f(glGetUniformLocation(shaderProgram, "fgColor"), 220.f / 255, 250.f / 255, 1.0f);
    glUniform3f(glGetUniformLocation(shaderProgram, "bgColor"), 0.0f, 72.f / 255, 80.f / 255);
}

void Renderer::setupQuad()
{
    // Note that this is not necessarily a fullscreen quad!
    // It gets scaled by whole integers and centered in the window
    // This is required for pixel-perfect rendering of the canvas
    float vertices[] = {
        0.0f, 0.0f, // top-left
        (float)Screen::width, 0.0f, // top-right
        (float)Screen::width, (float)Screen::height, // bottom-right
        0.0f, (float)Screen::height // bottom-left
    };

    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Renderer::uploadSSBO()
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, Screen::width * Screen::height / 8, Screen::pixels);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    app.metrics.ssbo_updates++;
}

void Renderer::setFgColor(float r, float g, float b)
{
    glUseProgram(shaderProgram);
    glUniform3f(glGetUniformLocation(shaderProgram, "fgColor"), r, g, b);
}

void Renderer::setBgColor(float r, float g, float b)
{
    glUseProgram(shaderProgram);
    glUniform3f(glGetUniformLocation(shaderProgram, "bgColor"), r, g, b);
}

// Render frame
void Renderer::render()
{
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram);

    // Scale the canvas by whole integer factors (keep the rendering pixel-perfect)
    int scaling = min(window.width / Screen::width, window.height / Screen::height);
    // Calculate offsets to center the scaled canvas
    int dx = (window.width - Screen::width * scaling) / 2;
    int dy = (window.height - Screen::height * scaling) / 2;

    glUniform2f(glGetUniformLocation(shaderProgram, "viewport"), (float)window.width, (float)window.height);
    glUniform2f(glGetUniformLocation(shaderProgram, "offset"), (float)dx, (float)dy);
    glUniform1f(glGetUniformLocation(shaderProgram, "scale"), (float)scaling);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    app.metrics.renders++;

    if (Screen::render_frames) Screen::render_frames--;
}