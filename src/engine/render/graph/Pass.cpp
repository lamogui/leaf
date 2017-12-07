#include <engine/render/graph/Pass.h>

#include <engine/render/Device.h>
#include <engine/render/graph/Batch.h>
#include <engine/render/graph/GPUProfiler.h>

Pass::Pass(const std::string &name)
    : name(name)
{
    D3D11_VIEWPORT defaultViewport;
    defaultViewport.Width = 16.0f;
    defaultViewport.Height = 16.0f;
    defaultViewport.MinDepth = 0.0f;
    defaultViewport.MaxDepth = 1.0f;
    defaultViewport.TopLeftX = 0;
    defaultViewport.TopLeftY = 0;

    this->setViewport(defaultViewport, glm::mat4(), glm::mat4());
}

void Pass::setViewport(D3D11_VIEWPORT viewport, const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix)
{
    this->viewport = viewport;

    // derive secondary info from matrices
    glm::mat4 viewMatrixInverse = glm::inverse(viewMatrix);
    this->passConstants.viewMatrix = viewMatrix;
    this->passConstants.viewMatrixInverse = viewMatrixInverse;
    this->passConstants.projectionMatrix = projectionMatrix;
    this->passConstants.projectionMatrixInverse = glm::inverse(projectionMatrix);
    this->passConstants.viewProjectionInverseMatrix = glm::inverse(projectionMatrix * viewMatrix);
    this->passConstants.cameraPosition = glm::vec3(viewMatrixInverse[3][0], viewMatrixInverse[3][1], viewMatrixInverse[3][2]);
}

Batch *Pass::addBatch(const std::string &name)
{
    Batch *batch = new Batch(name);
    this->batches.push_back(batch);
    return batch;
}

void Pass::execute(ID3D11DeviceContext *context, ID3D11Buffer *passConstantBuffer)
{
    GPUProfiler::ScopedProfile profile(this->name);

    // upload pass constants to GPU
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT res = context->Map(passConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    CHECK_HRESULT(res);
    memcpy(mappedResource.pData, &this->passConstants, sizeof(PassConstants));
    context->Unmap(passConstantBuffer, 0);

    context->RSSetViewports(1, &this->viewport);

    if ((this->colorTargets.size() > 0) || (this->depthStencilTarget != nullptr))
        context->OMSetRenderTargets((UINT)this->colorTargets.size(), this->colorTargets.size() ? &this->colorTargets[0] : nullptr, this->depthStencilTarget);

    // render batches
    for (auto *batch : this->batches)
    {
        batch->execute(context);
        delete batch;
    }

    if ((this->colorTargets.size() > 0) || (this->depthStencilTarget != nullptr))
        context->OMSetRenderTargets(0, nullptr, nullptr);
}
