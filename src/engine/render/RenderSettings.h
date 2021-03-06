#pragma once

#include <glm/glm.hpp>

class Texture;

struct CameraSettings
{
	glm::mat4 viewMatrix = glm::mat4(1.0f);
	glm::mat4 projectionMatrix = glm::mat4(1.0f);
	float shutterSpeed = 0.01f;
	float focusDistance = 1.0f;
	float focalLength = 35.0f;
	float fstop = 16.0f;
};

struct EnvironmentSettings
{
	glm::vec3 ambientColor = glm::vec3(0.0f);
	float mist = 0.0f;
    Texture *environmentMap = nullptr;
};

struct BloomSettings
{
	float threshold = 1.0f;
	float intensity = 1.0f;
    float size = 4.0f;
	bool debug = false;
};

struct PostProcessSettings
{
    float pixellateDivider = 0.0f;
    float vignetteSize = 1.0f;
    float vignettePower = 1.6f;
    float abberationStrength = 0.1f;
    float scanlineStrength = 0.0f;
    float scanlineFrequency = 20.0f;
    float scanlineOffset = 0.0f;
};

struct RenderSettings
{
	int frameWidth;
	int frameHeight;

	CameraSettings camera;
	EnvironmentSettings environment;
	BloomSettings bloom;
    PostProcessSettings postProcess;
};
