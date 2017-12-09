#pragma once

#include <d3d11.h>

class FrameGraph;
class RenderList;
class Scene;
struct ShadowConstants;

class ShadowRenderer
{
    public:
        ShadowRenderer(int resolution);
        ~ShadowRenderer();

        void render(FrameGraph *frameGraph, const Scene *scene, const RenderList *renderList, ShadowConstants *shadowConstants);

		ID3D11ShaderResourceView *getSRV() const { return this->srv; }
		ID3D11SamplerState *getSampler() const { return this->sampler; }

    private:
        int resolution;
        ID3D11Texture2D *shadowMap;
        ID3D11DepthStencilView *target;
        ID3D11ShaderResourceView *srv;
        ID3D11SamplerState *sampler;
        ID3D11DepthStencilState *depthState;

        ID3D11VertexShader *depthOnlyVertexShader;
        ID3D11PixelShader *depthOnlyPixelShader;

		ID3D11InputLayout *inputLayout;

        ID3D11Buffer *cbShadows;
};
