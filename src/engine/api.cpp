#include <engine/api.h>

#include <cassert>
#include <cstdio>

#include <engine/Engine.h>
#include <engine/cJSON/cJSON.h>

LEAFENGINE_API void leaf_initialize(int backbufferWidth, int backbufferHeight, bool capture, const char *profileFilename)
{
    Engine::create();
    Engine::getInstance()->initialize(backbufferWidth, backbufferHeight, capture, profileFilename != nullptr ? profileFilename : "");
}

LEAFENGINE_API void leaf_shutdown()
{
    Engine::getInstance()->shutdown();
    Engine::destroy();
}

LEAFENGINE_API void leaf_load_data(const void *buffer, size_t size)
{
    assert(buffer);
    Engine::getInstance()->loadData(buffer, size);
}

LEAFENGINE_API void leaf_update_animation(float time)
{
    Engine::getInstance()->updateAnimation(time);
}

LEAFENGINE_API void leaf_render(int width, int height, float deltaTime)
{
    Engine::getInstance()->render(width, height, deltaTime);
}

LEAFENGINE_API void leaf_render_blender_viewport(int width, int height, float view_matrix[], float projection_matrix[])
{
    glm::mat4 viewMatrix(
        view_matrix[0], view_matrix[1], view_matrix[2], view_matrix[3],
        view_matrix[4], view_matrix[5], view_matrix[6], view_matrix[7],
        view_matrix[8], view_matrix[9], view_matrix[10], view_matrix[11],
        view_matrix[12], view_matrix[13], view_matrix[14], view_matrix[15]
        );
    glm::mat4 projectionMatrix(
        projection_matrix[0], projection_matrix[1], projection_matrix[2], projection_matrix[3],
        projection_matrix[4], projection_matrix[5], projection_matrix[6], projection_matrix[7],
        projection_matrix[8], projection_matrix[9], projection_matrix[10], projection_matrix[11],
        projection_matrix[12], projection_matrix[13], projection_matrix[14], projection_matrix[15]
        );
    Engine::getInstance()->renderBlenderViewport(width, height, viewMatrix, projectionMatrix);
}
