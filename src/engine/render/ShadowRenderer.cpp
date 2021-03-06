#include <engine/render/ShadowRenderer.h>

#include <engine/render/Device.h>
#include <engine/render/RenderList.h>
#include <engine/render/Shaders.h>
#include <engine/render/graph/Batch.h>
#include <engine/render/graph/FrameGraph.h>
#include <engine/render/graph/GPUProfiler.h>
#include <engine/render/graph/Job.h>
#include <engine/render/graph/Pass.h>
#include <engine/scene/Scene.h>

#include <engine/render/shaders/constants/StandardConstants.h>

ShadowRenderer::ShadowRenderer(int resolution)
{
    HRESULT res;

    this->resolution = resolution;

    D3D11_TEXTURE2D_DESC shadowMapDesc;
    ZeroMemory(&shadowMapDesc, sizeof(shadowMapDesc));
    shadowMapDesc.Width = resolution * 2;
    shadowMapDesc.Height = resolution * 2;
    shadowMapDesc.MipLevels = 1;
    shadowMapDesc.ArraySize = 1;
    shadowMapDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    shadowMapDesc.SampleDesc.Count = 1;
    shadowMapDesc.SampleDesc.Quality = 0;
    shadowMapDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    res = Device::device->CreateTexture2D(&shadowMapDesc, NULL, &this->shadowMap);
    CHECK_HRESULT(res);

    D3D11_DEPTH_STENCIL_VIEW_DESC targetDesc;
    ZeroMemory(&targetDesc, sizeof(targetDesc));
    targetDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    targetDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    targetDesc.Texture2D.MipSlice = 0;

    res = Device::device->CreateDepthStencilView(this->shadowMap, &targetDesc, &this->target);
    CHECK_HRESULT(res);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    res = Device::device->CreateShaderResourceView(this->shadowMap, &srvDesc, &this->srv);
    CHECK_HRESULT(res);

    D3D11_SAMPLER_DESC samplerDesc;
    ZeroMemory(&samplerDesc, sizeof(samplerDesc));
    samplerDesc.Filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MipLODBias = 0;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    res = Device::device->CreateSamplerState(&samplerDesc, &this->sampler);
    CHECK_HRESULT(res);
    D3D11_DEPTH_STENCIL_DESC depthStateDesc;

    ZeroMemory(&depthStateDesc, sizeof(depthStateDesc));

    depthStateDesc.DepthEnable = TRUE;
    depthStateDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    depthStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    res = Device::device->CreateDepthStencilState(&depthStateDesc, &this->depthState);
    CHECK_HRESULT(res);
}

ShadowRenderer::~ShadowRenderer()
{
    this->shadowMap->Release();
    this->target->Release();
    this->srv->Release();
    this->sampler->Release();
    this->depthState->Release();
}

void ShadowRenderer::render(FrameGraph *frameGraph, const Scene *scene, const RenderList *renderList, ShadowConstants *shadowConstants, ID3D11InputLayout *inputLayout)
{
    const std::vector<RenderList::Job> &jobs = renderList->getJobs();
    const std::vector<RenderList::Light> &lights = renderList->getLights();

	frameGraph->addClearTarget(this->target, 1.0f, 0);

    int shadowCount = 0;
	for (int i = 0; i < lights.size(); i++)
	{
		// only spotlights cast shadows
		if (!lights[i].spot || shadowCount >= 4)
			continue;

		int index = shadowCount++;

		// apply NDC [-1, 1] to texture space [0, 1] to atlas rect
		/*glm::vec3 scale = glm::vec3(0.5f, 0.5f, 1.0f);
		glm::vec3 offset = glm::vec3(0.5f, 0.5f, 0.0f);
		//glm::vec3 scale = glm::vec3(0.125f, 0.125f, 1.0f);
		//glm::vec3 offset = glm::vec3(0.125f + 0.25f * (float)(index % 4), 0.125f + 0.25f * (float)(index / 4), 0.0f);
		glm::mat4 biasMatrix(
			scale.x, 0.0f, 0.0f, offset.x,
			0.0f, scale.y, 0.0f, offset.y,
			0.0f, 0.0f, scale.z, offset.z,
			0.0f, 0.0f, 0.0f, 1.0f
		);*/
		shadowConstants->lightMatrix[index] = lights[i].shadowTransform;

		GPUProfiler::ScopedProfile profile("Shadow");

		Pass *shadowPass = frameGraph->addPass("ShadowMap");
		shadowPass->setTargets({}, this->target);

		D3D11_VIEWPORT viewport;
        viewport.Width = (float)this->resolution;
        viewport.Height = (float)this->resolution;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        viewport.TopLeftX = (float)((index % 2) * this->resolution);
        viewport.TopLeftY = (float)((index / 2) * this->resolution);
        shadowPass->setViewport(viewport, glm::mat4(1.0f), glm::mat4(1.0f));

		Batch *batch = shadowPass->addBatch("Light");
		batch->setDepthStencil(this->depthState);
		batch->setVertexShader(Shaders::vertex.depthOnly);
		batch->setPixelShader(Shaders::pixel.depthOnly);
		batch->setInputLayout(inputLayout);

        const Mesh::SubMesh *currentSubMesh = nullptr;
		Job *currentJob = nullptr;
        for (const auto &job : jobs)
        {
            if (currentSubMesh != job.subMesh)
            {
                currentSubMesh = job.subMesh;

				currentJob = batch->addJob();
                currentJob->setBuffers(currentSubMesh->vertexBuffer, currentSubMesh->indexBuffer, currentSubMesh->indexCount);
            }

			DepthOnlyInstanceData instanceData;
			instanceData.transformMatrix = lights[i].shadowTransform * job.transform;
	
			currentJob->addInstance(instanceData);
		}
    }
}
