#pragma once

#include <cassert>
#include <string>
#include <map>

class Resource;
struct cJSON;

class ResourceManager
{
    public:
        // this method clones the whole json tree passed to it; it is safe
        // to delete data after the call
        template <class ResourceType>
        void updateResourceData(const std::string &name, const cJSON *data);

        template <class ResourceType>
        ResourceType *requestResource(const std::string &name);

        inline void releaseResource(Resource *resource);

    private:
        static ResourceManager *instance;

        struct ResourceDescriptor
        {
            ResourceDescriptor()
                : resource(nullptr)
                , data(nullptr)
                , users(0)
            {}

            Resource *resource;
            cJSON *data;
            int users; // refcount, in blender terms
        };

        ResourceManager();
        ~ResourceManager();

        // if not found, creates a new one
        template <class ResourceType>
        ResourceDescriptor &findDescriptor(const std::string &name);

        typedef std::map<std::string, ResourceDescriptor> DescriptorMap;
        DescriptorMap descriptors;

    public:
        // singleton implementation
        static void create() { assert(!ResourceManager::instance); ResourceManager::instance = new ResourceManager; }
        static void destroy() { assert(ResourceManager::instance); delete ResourceManager::instance; }
        static ResourceManager *getInstance() { assert(ResourceManager::instance); return ResourceManager::instance; }
};

#include <engine/ResourceManager.inline.h>