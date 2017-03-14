#include <engine/Renderer.h>

#include <cstdio>

#include <windows.h>
#include <gl/GL.h>
#include <d3d11.h>

#include <engine/glm/glm.hpp>
#include <engine/glm/gtc/matrix_inverse.hpp>

#include <engine/Device.h>
#include <engine/GPUProfiler.h>
#include <engine/Material.h>
#include <engine/Mesh.h>
#include <engine/PostProcessor.h>
#include <engine/RenderList.h>
#include <engine/RenderTarget.h>
#include <engine/ResourceManager.h>
#include <engine/Scene.h>

#include <shaders/background.vs.hlsl.h>
#include <shaders/background.ps.hlsl.h>
#include <shaders/basic.vs.hlsl.h>
#include <shaders/basic.ps.hlsl.h>
#include <shaders/gbuffer.vs.hlsl.h>
#include <shaders/gbuffer.ps.hlsl.h>
#include <shaders/plop.vs.hlsl.h>
#include <shaders/plop.ps.hlsl.h>
#include <shaders/standard.vs.hlsl.h>
#include <shaders/standard.ps.hlsl.h>

#pragma pack(push)
#pragma pack(16)
struct LightData
{
    glm::vec3 position;
    float radius;
    glm::vec3 color;
    float _padding;
};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(16)
struct SceneState
{
    glm::mat4 viewMatrix;
    glm::mat4 viewMatrixInverse;
    glm::mat4 projectionMatrix;
    glm::mat4 projectionMatrixInverse;
    glm::mat4 viewProjectionInverseMatrix;
    glm::vec3 cameraPosition;
    int lightCount;
    LightData lights[16];
};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(16)
struct InstanceData
{
    glm::mat4 modelMatrix;
    glm::mat3 normalMatrix;
    float _padding[3];
};
#pragma pack(pop)

Renderer::Renderer(HWND hwnd, int backbufferWidth, int backbufferHeight, bool capture, const std::string &profileFilename)
{
    this->backbufferWidth = backbufferWidth;
    this->backbufferHeight = backbufferHeight;
    this->capture = capture;
    this->profileFilename = profileFilename;
    this->renderList = new RenderList;

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

    //set buffer dimensions and format
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = backbufferWidth;
    swapChainDesc.BufferDesc.Height = backbufferHeight;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    //set refresh rate
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;

    //sampling settings
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.SampleDesc.Count = 1;

    //output window handle
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.Windowed = true;

    UINT flags = 0;
    #ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif
    HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, NULL, 0, D3D11_SDK_VERSION, &swapChainDesc, &this->swapChain, &Device::device, NULL, &Device::context);
    CHECK_HRESULT(res);

    res = this->swapChain->GetBuffer(0, __uuidof(this->backBuffer), (void **)&this->backBuffer);
    CHECK_HRESULT(res);

    res = Device::device->CreateRenderTargetView(this->backBuffer, NULL, &this->renderTarget);
    CHECK_HRESULT(res);

    D3D11_TEXTURE2D_DESC depthBufferDesc;
    ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));
    depthBufferDesc.Width = this->backbufferWidth;
    depthBufferDesc.Height = this->backbufferHeight;
    depthBufferDesc.MipLevels = 1;
    depthBufferDesc.ArraySize = 1;
    depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthBufferDesc.SampleDesc.Count = 1;
    depthBufferDesc.SampleDesc.Quality = 0;
    depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    res = Device::device->CreateTexture2D(&depthBufferDesc, NULL, &this->depthBuffer);
    CHECK_HRESULT(res);

    D3D11_DEPTH_STENCIL_VIEW_DESC depthTargetDesc;
    ZeroMemory(&depthTargetDesc, sizeof(depthTargetDesc));
    depthTargetDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthTargetDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthTargetDesc.Texture2D.MipSlice = 0;

    res = Device::device->CreateDepthStencilView(depthBuffer, &depthTargetDesc, &this->depthTarget);
    CHECK_HRESULT(res);

    D3D11_DEPTH_STENCIL_DESC depthStateDesc;
    ZeroMemory(&depthStateDesc, sizeof(depthStateDesc));

    depthStateDesc.DepthEnable = TRUE;
    depthStateDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    depthStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    Device::device->CreateDepthStencilState(&depthStateDesc, &this->gBufferDepthState);

    depthStateDesc.DepthEnable = TRUE;
    depthStateDesc.DepthFunc = D3D11_COMPARISON_GREATER;
    depthStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    Device::device->CreateDepthStencilState(&depthStateDesc, &this->lightingDepthState);

    depthStateDesc.DepthEnable = TRUE;
    depthStateDesc.DepthFunc = D3D11_COMPARISON_EQUAL;
    depthStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    Device::device->CreateDepthStencilState(&depthStateDesc, &this->backgroundDepthState);

    D3D11_RASTERIZER_DESC rasterizerDesc;
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_BACK;
    rasterizerDesc.FrontCounterClockwise = TRUE;
    rasterizerDesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.ScissorEnable = FALSE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;

    ID3D11RasterizerState *rasterizerState;
    Device::device->CreateRasterizerState(&rasterizerDesc, &rasterizerState);
    Device::context->RSSetState(rasterizerState);

    for (int i = 0; i < GBUFFER_PLANE_COUNT; i++)
        this->gBuffer[i] = new RenderTarget(this->backbufferWidth, this->backbufferHeight, DXGI_FORMAT_R16G16B16A16_FLOAT);

    this->postProcessor = new PostProcessor(this->renderTarget);

    if (this->capture)
    {
        D3D11_TEXTURE2D_DESC captureBufferDesc;
        this->backBuffer->GetDesc(&captureBufferDesc);
        captureBufferDesc.BindFlags = 0;
        captureBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        captureBufferDesc.Usage = D3D11_USAGE_STAGING;

        res = Device::device->CreateTexture2D(&captureBufferDesc, NULL, &this->captureBuffer);
        CHECK_HRESULT(res);
    }

    res = Device::device->CreateVertexShader(backgroundVS, sizeof(backgroundVS), NULL, &backgroundVertexShader); CHECK_HRESULT(res);
    res = Device::device->CreateVertexShader(basicVS, sizeof(basicVS), NULL, &basicVertexShader); CHECK_HRESULT(res);
    res = Device::device->CreateVertexShader(gbufferVS, sizeof(gbufferVS), NULL, &gbufferVertexShader); CHECK_HRESULT(res);
    res = Device::device->CreateVertexShader(plopVS, sizeof(plopVS), NULL, &plopVertexShader); CHECK_HRESULT(res);
    res = Device::device->CreateVertexShader(standardVS, sizeof(standardVS), NULL, &standardVertexShader); CHECK_HRESULT(res);

    res = Device::device->CreatePixelShader(backgroundPS, sizeof(backgroundPS), NULL, &backgroundPixelShader); CHECK_HRESULT(res);
    res = Device::device->CreatePixelShader(basicPS, sizeof(basicPS), NULL, &basicPixelShader); CHECK_HRESULT(res);
    res = Device::device->CreatePixelShader(gbufferPS, sizeof(gbufferPS), NULL, &gbufferPixelShader); CHECK_HRESULT(res);
    res = Device::device->CreatePixelShader(plopPS, sizeof(plopPS), NULL, &plopPixelShader); CHECK_HRESULT(res);
    res = Device::device->CreatePixelShader(standardPS, sizeof(standardPS), NULL, &standardPixelShader); CHECK_HRESULT(res);

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    res = Device::device->CreateInputLayout(layout, 4, basicVS, sizeof(basicVS), &inputLayout);
    CHECK_HRESULT(res);

    D3D11_BUFFER_DESC cbDesc;
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.StructureByteStride = 0;
    cbDesc.MiscFlags = 0;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    cbDesc.ByteWidth = sizeof(SceneState);
    res = Device::device->CreateBuffer(&cbDesc, NULL, &this->cbScene);
    CHECK_HRESULT(res);

    cbDesc.ByteWidth = sizeof(Material::MaterialData);
    res = Device::device->CreateBuffer(&cbDesc, NULL, &this->cbMaterial);
    CHECK_HRESULT(res);

    cbDesc.ByteWidth = sizeof(InstanceData);
    res = Device::device->CreateBuffer(&cbDesc, NULL, &this->cbInstance);
    CHECK_HRESULT(res);

    ID3D11Buffer *allConstantBuffers[] = { this->cbScene, this->cbMaterial, this->cbInstance };
    Device::context->VSSetConstantBuffers(0, 3, allConstantBuffers);
    Device::context->PSSetConstantBuffers(0, 3, allConstantBuffers);

    // built-in rendering resources
    const char *fullscreenQuad = "{"
            "\"vertices\": ["
                "-1, -1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1,"
                "-1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0,"
                "1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0,"
                "1, -1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1"
            "],"
            "\"vertexCount\": 4,"
            "\"indices\": ["
                "0, 2, 1,"
                "2, 0, 3"
            "],"
            "\"indexCount\": 6,"
            "\"material\": \"__default\""
        "}";
    ResourceManager::getInstance()->updateResourceData<Mesh>("__fullscreenQuad", (const unsigned char *)fullscreenQuad, strlen(fullscreenQuad));

    this->fullscreenQuad = ResourceManager::getInstance()->requestResource<Mesh>("__fullscreenQuad");

    GPUProfiler::create(!this->profileFilename.empty());
    GPUProfiler::getInstance()->beginJsonCapture();
}

Renderer::~Renderer()
{
    GPUProfiler::getInstance()->endJsonCapture(this->profileFilename);
    GPUProfiler::destroy();

    this->swapChain->Release();
    this->backBuffer->Release();
    this->depthBuffer->Release();
    if (this->capture)
        this->captureBuffer->Release();
    this->renderTarget->Release();
    this->depthTarget->Release();

    this->backgroundVertexShader->Release();
    this->basicVertexShader->Release();
    this->gbufferVertexShader->Release();
    this->plopVertexShader->Release();
    this->standardVertexShader->Release();

    this->backgroundPixelShader->Release();
    this->basicPixelShader->Release();
    this->gbufferPixelShader->Release();
    this->plopPixelShader->Release();
    this->standardPixelShader->Release();

    this->inputLayout->Release();

    this->cbScene->Release();
    this->cbMaterial->Release();
    this->cbInstance->Release();

    delete this->renderList;

    ResourceManager::getInstance()->releaseResource(this->fullscreenQuad);

    for (int i = 0; i < GBUFFER_PLANE_COUNT; i++)
        delete this->gBuffer[i];

    this->gBufferDepthState->Release();
    this->lightingDepthState->Release();
    this->backgroundDepthState->Release();

    delete this->postProcessor;

    // make sure all graphics resources are released before destroying the context
    ResourceManager::getInstance()->clearPendingUnloads();

    Device::context->Release();
    Device::device->Release();
}

void Renderer::render(const Scene *scene, int width, int height, bool overrideCamera, const glm::mat4 &viewMatrixOverride, const glm::mat4 &projectionMatrixOverride)
{
    GPUProfiler::getInstance()->beginFrame();

    D3D11_VIEWPORT viewport;
    viewport.Width = (float)backbufferWidth;
    viewport.Height = (float)backbufferHeight;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    Device::context->RSSetViewports(1, &viewport);

    float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };
    {
        GPUProfiler::ScopedProfile profile("Clear");
        Device::context->ClearRenderTargetView(this->renderTarget, clearColor);
        Device::context->ClearDepthStencilView(this->depthTarget, D3D11_CLEAR_DEPTH, 1.0f, 0);
    }

    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;

    if (overrideCamera)
    {
        // use the provided camera parameters
        viewMatrix = viewMatrixOverride;
        projectionMatrix = projectionMatrixOverride;
    }
    else
    {
        // get camera from scene
        scene->setupCameraMatrices(viewMatrix, projectionMatrix, (float)width / (float)height);
    }

    this->renderList->clear();
    scene->fillRenderList(this->renderList);
    this->renderList->sort();

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT res = Device::context->Map(this->cbScene, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    CHECK_HRESULT(res);
    SceneState *sceneState = (SceneState *)mappedResource.pData;
    sceneState->viewMatrix = viewMatrix;
    glm::mat4 viewMatrixInverse = glm::inverse(viewMatrix);
    sceneState->viewMatrixInverse = viewMatrixInverse;
    sceneState->projectionMatrix = projectionMatrix;
    sceneState->projectionMatrixInverse = glm::inverse(projectionMatrix);
    sceneState->viewProjectionInverseMatrix = glm::inverse(projectionMatrix * viewMatrix);
    sceneState->cameraPosition = glm::vec3(viewMatrixInverse[3][0], viewMatrixInverse[3][1], viewMatrixInverse[3][2]);

    const std::vector<RenderList::Light> &lights = this->renderList->getLights();
    sceneState->lightCount = std::min((int)lights.size(), 16);
    for (int i = 0; i < sceneState->lightCount; i++)
    {
        sceneState->lights[i].position = lights[i].position;
        sceneState->lights[i].radius = lights[i].radius;
        sceneState->lights[i].color = lights[i].color;
    }

    Device::context->Unmap(this->cbScene, 0);

    Device::context->IASetInputLayout(inputLayout);

    // G-Buffer pass
    /*for (int i = 0; i < GBUFFER_PLANE_COUNT; i++)
        Device::context->ClearRenderTargetView(this->gBuffer[i]->getTarget(), clearColor);

    ID3D11RenderTargetView *gBufferTargets[GBUFFER_PLANE_COUNT] = {
        this->gBuffer[0]->getTarget(),
        this->gBuffer[1]->getTarget()
    };
    Device::context->OMSetRenderTargets(GBUFFER_PLANE_COUNT, gBufferTargets, this->depthTarget);

    Device::context->VSSetShader(gbufferVertexShader, NULL, 0);
    Device::context->PSSetShader(gbufferPixelShader, NULL, 0);*/

    Device::context->OMSetDepthStencilState(this->gBufferDepthState, 0);

    RenderTarget *radianceTarget = this->postProcessor->getRadianceTarget();
    ID3D11RenderTargetView *radianceTargetView = radianceTarget->getTarget();
    Device::context->OMSetRenderTargets(1, &radianceTargetView, this->depthTarget);

    Device::context->VSSetShader(standardVertexShader, NULL, 0);
    Device::context->PSSetShader(standardPixelShader, NULL, 0);

    const std::vector<RenderList::Job> &jobs = this->renderList->getJobs();

    {
        GPUProfiler::ScopedProfile profile("Geometry");
        Material *currentMaterial = nullptr;
        Mesh * currentMesh = nullptr;
        for (const auto &job : jobs)
        {
            if (currentMaterial != job.material)
            {
                currentMaterial = job.material;

                HRESULT res = Device::context->Map(this->cbMaterial, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
                CHECK_HRESULT(res);
                memcpy(mappedResource.pData, &currentMaterial->getMaterialData(), sizeof(Material::MaterialData));
                Device::context->Unmap(this->cbMaterial, 0);

                currentMaterial->bindTextures();
            }

            if (currentMesh != job.mesh)
            {
                currentMesh = job.mesh;
                currentMesh->bind();
            }

            HRESULT res = Device::context->Map(this->cbInstance, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            CHECK_HRESULT(res);
            InstanceData *instanceData = (InstanceData *)mappedResource.pData;
            instanceData->modelMatrix = job.transform;
            instanceData->normalMatrix = glm::mat3(glm::inverseTranspose(job.transform));
            Device::context->Unmap(this->cbInstance, 0);

            Device::context->DrawIndexed(currentMesh->getIndexCount(), 0, 0);
        }
    }

    // lighting pass
    /*{
        GPUProfiler::ScopedProfile profile("Lighting");

        ID3D11SamplerState *gBufferSamplers[GBUFFER_PLANE_COUNT] = {
            this->gBuffer[0]->getSamplerState(),
            this->gBuffer[1]->getSamplerState()
        };

        ID3D11ShaderResourceView *gBufferSRVs[GBUFFER_PLANE_COUNT] = {
            this->gBuffer[0]->getSRV(),
            this->gBuffer[1]->getSRV()
        };

        RenderTarget *radianceTarget = this->postProcessor->getRadianceTarget();
        ID3D11RenderTargetView *radianceTargetView = radianceTarget->getTarget();

        Device::context->OMSetDepthStencilState(this->lightingDepthState, 0);
        Device::context->OMSetRenderTargets(1, &radianceTargetView, this->depthTarget);
        Device::context->VSSetShader(basicVertexShader, NULL, 0);
        Device::context->PSSetShader(basicPixelShader, NULL, 0);
        Device::context->PSSetSamplers(0, GBUFFER_PLANE_COUNT, gBufferSamplers);
        Device::context->PSSetShaderResources(0, GBUFFER_PLANE_COUNT, gBufferSRVs);
        this->fullscreenQuad->bind();
        Device::context->DrawIndexed(this->fullscreenQuad->getIndexCount(), 0, 0);
    }*/

    ID3D11ShaderResourceView *srvNulls[GBUFFER_PLANE_COUNT] = { nullptr, nullptr };
    Device::context->PSSetShaderResources(0, GBUFFER_PLANE_COUNT, srvNulls);

    // background pass
    {
        GPUProfiler::ScopedProfile profile("Background");

        /*viewport.Width = (float)width;
        viewport.Height = (float)height;
        Device::context->RSSetViewports(1, &viewport);*/
        Device::context->VSSetShader(backgroundVertexShader, NULL, 0);
        Device::context->PSSetShader(backgroundPixelShader, NULL, 0);
        Device::context->OMSetDepthStencilState(this->backgroundDepthState, 0);
        this->fullscreenQuad->bind();
        Device::context->DrawIndexed(this->fullscreenQuad->getIndexCount(), 0, 0);
    }

    this->postProcessor->render(width, height);

    {
        GPUProfiler::ScopedProfile profile("V-Sync");
        this->swapChain->Present(0, 0);
    }

    GPUProfiler::getInstance()->endFrame();

    if (this->capture)
        Device::context->CopyResource(this->captureBuffer, this->backBuffer);
}

void Renderer::renderBlenderViewport(const Scene *scene, int width, int height, const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix)
{
    assert(this->capture);

    this->render(scene, width, height, true, viewMatrix, projectionMatrix);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    D3D11_MAPPED_SUBRESOURCE mappedCaptureBuffer;
    HRESULT res = Device::context->Map(this->captureBuffer, 0, D3D11_MAP_READ, 0, &mappedCaptureBuffer);
    CHECK_HRESULT(res);

    // direct copy from D3D mapped memory to GL backbuffer :)
    glRasterPos2i(0, height - 1);
    glPixelZoom(1, -1);
    glDrawPixels(this->backbufferWidth, this->backbufferHeight - 1, GL_RGBA, GL_UNSIGNED_BYTE, mappedCaptureBuffer.pData);
    glPixelZoom(1, 1);

    Device::context->Unmap(this->captureBuffer, 0);
}
