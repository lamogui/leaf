#pragma once

#include <string>

#include <engine/Device.h>
#include <engine/Resource.h>

class Image: public Resource
{
    public:
        static const std::string resourceClassName;
        static const std::string defaultResourceData;

        Image(): texture(nullptr) {}

        virtual void load(const cJSON *json) override;
        virtual void unload() override;

        const ID3D11Texture2D *getTexture() const { return this->texture; }

    private:
        ID3D11Texture2D *texture;
};
