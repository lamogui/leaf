#pragma once

#include <d3d11.h>

class FrameGraph;
class Mesh;
struct RenderSettings;
class RenderTarget;

class BloomRenderer
{
    public:
        BloomRenderer(int backbufferWidth, int backbufferHeight);
        ~BloomRenderer();

        void render(FrameGraph *frameGraph, const RenderSettings &settings, RenderTarget *inputTarget, RenderTarget *outputTarget, Mesh *quad);

    private:
		int backbufferWidth;
		int backbufferHeight;

		ID3D11VertexShader *bloomVertexShader;
		ID3D11PixelShader *bloomThresholdPixelShader;

		ID3D11InputLayout *inputLayout;

		static const int DOWNSAMPLE_LEVELS = 4;
		RenderTarget *downsampleTargets[DOWNSAMPLE_LEVELS];
};
