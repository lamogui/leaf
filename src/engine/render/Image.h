#pragma once

#include <string>

#include <engine/render/Device.h>
#include <engine/resource/Resource.h>

class Image: public Resource
{
    public:
        static const std::string resourceClassName;
        static const std::string defaultResourceData;

        Image(): texture(nullptr) {}

        virtual void load(const unsigned char *buffer, size_t size) override;
        virtual void unload() override;

        ID3D11ShaderResourceView *getSRV() const { return this->srv; }

    private:
        ID3D11Resource *texture;
        ID3D11ShaderResourceView *srv;
};