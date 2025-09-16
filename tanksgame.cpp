#include "core.h"

#define EXPORT extern "C" __declspec(dllexport)

vec4 HSVtoRGBA(float h, float s, float v)
{
    float c = v * s;
    float x = c * (1 - fabs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;

    float r, g, b;
    if (h < 60)      { r = c; g = x; b = 0; }
    else if (h < 120){ r = x; g = c; b = 0; }
    else if (h < 180){ r = 0; g = c; b = x; }
    else if (h < 240){ r = 0; g = x; b = c; }
    else if (h < 300){ r = x; g = 0; b = c; }
    else             { r = c; g = 0; b = x; }

    return { r + m, g + m, b + m, 1.0f};
}

vec4 GetHSVSpectrumColor(float time, float speed = 1.0f)
{
    float hue = fmod(time * speed * 60.0f, 360.0f);
    return HSVtoRGBA(hue, 1.0f, 1.0f);
}

EXPORT GAME_UPDATE_FUNCTION(update)
{
    // Update code here
    GameState * state = (GameState*)gameMemory->permStorage;
    state->time += deltaTime;
    double time = state->time;

    state->tempPlayerPos.y += state->tempInput.y * deltaTime;
    state->tempPlayerPos.x += state->tempInput.x * deltaTime;

    vec4 color = GetHSVSpectrumColor(time);
    //color = {0.2f, 0.3f, 0.3f, 1.0f};
    state->clearColor = color;

    instanceBuffer->data[0] = {{state->tempPlayerPos.x, state->tempPlayerPos.y, 0.0f}, {1.0f, 1.0f}, 0.0f};
    instanceBuffer->data[1] = {{0.5f, 0.0f}, {1.0f + sinf(time) * 0.5f, 1.0f + sinf(time) * 0.5f}, 1.57079633f};
    instanceBuffer->data[2] = {{-0.0f, -0.5f}, {1.0f, 1.0f}, (float)fmod(time, 360.0)};
    instanceBuffer->data[3] = {{-sinf(time) * 0.5f, cosf(time) * 0.5f, 0.0f}, {1.0f, 1.0f}, 0.0f};
    instanceBuffer->instanceCount = 4;
}