#pragma once

#include "glm/ext/vector_float3.hpp"
#include "glm/fwd.hpp"
#include "sf_allocators/linear_allocator.hpp"
#include "sf_core/application.hpp"
#include "sf_platform/platform.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_core/defines.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/swapchain.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/pipeline.hpp"
#include "sf_vulkan/synch.hpp"
#include "sf_vulkan/texture.hpp"
#include "sf_containers/optional.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace sf {

struct PlatformState;
struct VulkanContext;

// struct VulkanAllocator {
//     VkAllocationCallbacks callbacks;
// };

using VulkanAllocator = VkAllocationCallbacks;

struct VulkanContext {
public:
    VkInstance                            instance;
    VulkanAllocator                       allocator;
    LinearAllocator                       render_system_allocator;
    VulkanDevice                          device;
    VkSurfaceKHR                          surface;
#ifdef SF_DEBUG
    VkDebugUtilsMessengerEXT              debug_messenger;
#endif
    VulkanSwapchain                       swapchain;
    VulkanVertexIndexBuffer               vertex_index_buffer;
    VulkanShaderPipeline                  pipeline;
    VulkanCommandBuffer                   texture_load_command_buffer;
    VulkanCommandPool                     graphics_command_pool;
    VulkanCommandPool                     transfer_command_pool;
    VulkanDescriptorSetLayout             global_descriptor_layout;
    VulkanDescriptorPool                  global_descriptor_pool;
    FixedArray<VkDescriptorSet, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>            global_descriptor_sets;
    FixedArray<VulkanCommandBuffer, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>        graphics_command_buffers;
    FixedArray<VulkanCommandBuffer, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>        transfer_command_buffers;
    FixedArray<VulkanSemaphore, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>            image_available_semaphores;
    FixedArray<VulkanSemaphore, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>            render_finished_semaphores;
    FixedArray<VulkanFence, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>                draw_fences;
    FixedArray<VulkanFence, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>                transfer_fences;
    f64                                   frame_delta_time;
    u32                                   image_index;
    u32                                   curr_frame;
    u32                                   framebuffer_size_generation;
    u32                                   framebuffer_last_size_generation;
    u16                                   framebuffer_width;
    u16                                   framebuffer_height;
public:
    VulkanContext();
    ~VulkanContext();

    VkViewport    get_viewport() const;
    VkRect2D      get_scissors() const;
    VulkanCommandBuffer& curr_frame_graphics_cmd_buffer();
};

struct RenderPacket {
    f64 delta_time;
    f64 elapsed_time;
};

struct Camera {
    static constexpr glm::vec3 WORLD_UP{ 0.0f, 1.0f, 0.0f };
    static constexpr f32 ZOOM_SPEED{ 2.5f };
    static constexpr f32 NEAR{ 0.1f };
    static constexpr f32 FAR{ 90.0f };

    glm::vec3 pos{ 0.0f, 0.0f, 5.0f };
    glm::vec3 target{ 0.0f, 0.0f, -1.0f };
    glm::vec3 right{ 1.0f, 0.0f, 0.0f };
    glm::vec3 up{ WORLD_UP };
    f32       speed{55.0f};
    f32       yaw{-90.0f};
    f32       pitch{0.0f};
    f32       zoom{45.0f};
    bool      dirty{true};

    Camera();
    void update_vectors();
};

struct VulkanRenderer {
    PlatformState*                     platform_state;
    VulkanGlobalUniformBufferObject    global_ubo;
    Camera                             camera;
    u64                                frame_count;
    f64                                delta_time;
};

VulkanDevice* renderer_init(ApplicationConfig& config, PlatformState& platform_state);
bool renderer_post_init();
bool renderer_on_resize(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context);
bool renderer_begin_frame(f64 delta_time);
void renderer_end_frame(f64 delta_time);
bool renderer_draw_frame(const RenderPacket& packet);
const VulkanDevice& renderer_get_device();
SF_EXPORT void renderer_update_global_ubo(const glm::mat4& view, const glm::mat4& proj);
SF_EXPORT void renderer_update_view(const glm::mat4& view);
SF_EXPORT void renderer_update_proj(const glm::mat4& proj);
SF_EXPORT f32 renderer_get_aspect_ratio();

// shared functions
void sf_vk_check(VkResult vk_result);
u32 sf_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData
);

} // sf
