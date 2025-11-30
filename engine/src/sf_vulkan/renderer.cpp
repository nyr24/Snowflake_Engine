#include "sf_allocators/arena_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/optional.hpp"
#include "sf_core/application.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/event.hpp"
#include "sf_core/input.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_core/model.hpp"
#include "sf_core/utility.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/material.hpp"
#include "sf_vulkan/pipeline.hpp"
#include "sf_vulkan/shared_types.hpp"
#include "sf_vulkan/texture.hpp"
#include "sf_vulkan/swapchain.hpp"
#include "sf_vulkan/synch.hpp"
#include "sf_vulkan/renderer.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_platform/platform.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_vulkan/mesh.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_platform/glfw3.h"
#include <glm/geometric.hpp>
#include <algorithm>
#include <span>
#include <string_view>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/trigonometric.hpp>
#include <stb_image.h>

namespace sf {

static VulkanRenderer vk_renderer{};
static VulkanContext vk_context{};

static constexpr u32 REQUIRED_VALIDATION_LAYER_CAPACITY{ 1 };
static constexpr u32 MAIN_PIPELINE_ATTRIB_COUNT{ 3 };
static constexpr u32 MAX_MODEL_COUNT{ 16 };
static constexpr u32 DEFAULT_MESH_COUNT{ 0 };
static std::string_view MAIN_SHADER_FILE_NAME{"shader.spv"};

// TODO: move to renderer struct
static FixedArray<std::string_view, VulkanShaderPipeline::MAX_DEFAULT_TEXTURES> preload_texture_file_names{
    "grass.jpg", "liquid_1.jpg", "liquid_2.jpg", "metal.jpg", "painting.jpg", "rock_wall.jpg",
    "soil.jpg", "water.jpg", "stones.jpg", "tree.jpg", "sand.jpg"
};
static FixedArray<TextureInputConfig, VulkanShaderPipeline::MAX_DEFAULT_TEXTURES> preload_texture_configs;
static FixedArray<std::string_view, VulkanShaderPipeline::MAX_DEFAULT_TEXTURES> preload_material_configs{ MaterialSystem::DEFAULT_FILE_NAME };
static FixedArray<std::string_view, 10> model_names{ /* { "box", "gltf" }, */ { "avocado.gltf" } };
static FixedArray<Texture*, VulkanShaderPipeline::MAX_DEFAULT_TEXTURES> preload_textures(preload_texture_file_names.count());
static FixedArray<Material*, VulkanShaderPipeline::MAX_DEFAULT_TEXTURES> preload_materials(preload_material_configs.count());
static FixedArray<Model, MAX_MODEL_COUNT> models(model_names.count());

bool create_instance(ApplicationConfig& config, PlatformState& platform_state);
void create_debugger();
bool init_extensions(VkInstanceCreateInfo& create_info, FixedArray<const char*, VK_MAX_EXTENSION_COUNT>& required_extensions);
bool init_validation_layers(VkInstanceCreateInfo& create_info, FixedArray<const char*, REQUIRED_VALIDATION_LAYER_CAPACITY>& required_validation_layers);
void init_synch_primitives(VulkanContext& context);
void init_descriptor_set_layouts_and_sets(VulkanContext& context);
void destroy_synch_primitives(VulkanContext& context);
void create_descriptor_layouts_and_sets_for_ubo(
    const VulkanDevice& device,
    VulkanDescriptorPool& pool,
    u32 descriptor_size,
    std::span<VulkanBuffer> buffers,
    std::span<VulkanDescriptorSetLayout> out_layouts,
    std::span<VkDescriptorSet> out_sets
);
static bool create_main_shader_pipeline();
static void preload_textures_and_materials(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc);
static void init_meshes(VulkanShaderPipeline& shader, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, ArenaAllocator& main_alloc, StackAllocator& temp_alloc);
static void init_global_descriptors(const VulkanDevice& device);
static void update_global_descriptors(const VulkanDevice& device);
static void renderer_create_default_meshes(const VulkanDevice& device, VulkanShaderPipeline& shader, VulkanCommandBuffer& cmd_buffer, StackAllocator& temp_alloc, u32 count = 1);
static void renderer_update_global_state();
static bool renderer_handle_mouse_move_event(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context);
static bool renderer_handle_key_press_event(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context);
static bool renderer_handle_mouse_wheel_event(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context);

VulkanContext::VulkanContext()
    : curr_frame{0}
{
    graphics_command_buffers.resize_to_capacity();
    transfer_command_buffers.resize_to_capacity();
    image_available_semaphores.resize_to_capacity();
    render_finished_semaphores.resize_to_capacity();
    draw_fences.resize_to_capacity(); transfer_fences.resize_to_capacity();
    global_descriptor_sets.resize_to_capacity();
}

// returns poitnter to static variable
VulkanDevice* renderer_init(ApplicationConfig& config, PlatformState& platform_state) {
    if (!create_instance(config, platform_state)) {
        LOG_FATAL("Failed to create vk_instance");
        return nullptr;
    }

    platform_state.create_vk_surface(vk_context);

    if (!VulkanDevice::create(vk_context)) {
        LOG_FATAL("Failed to create vk_device");
        return nullptr;
    }

    // Global descriptors
    if (!VulkanGlobalUniformBufferObject::create(vk_context.device, vk_renderer.global_ubo)) {
        LOG_FATAL("Failed to create uniform buffer object");
        return nullptr;
    }
    init_global_descriptors(vk_context.device);

    if (!VulkanSwapchain::create(vk_context.device, vk_context.surface, vk_context.framebuffer_width, vk_context.framebuffer_height, vk_context.swapchain)) {
        LOG_FATAL("Failed to create swapchain");
        return nullptr;
    }

    VulkanCommandPool::create(vk_context, VulkanCommandPoolType::GRAPHICS, vk_context.device.queue_family_info.graphics_family_index,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, vk_context.graphics_command_pool);
    VulkanCommandPool::create(vk_context, VulkanCommandPoolType::TRANSFER, vk_context.device.queue_family_info.transfer_family_index,
        static_cast<VkCommandPoolCreateFlagBits>(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT), vk_context.transfer_command_pool);

    VulkanCommandBuffer::allocate(vk_context.device, vk_context.graphics_command_pool.handle, {vk_context.graphics_command_buffers.data(), vk_context.graphics_command_buffers.capacity()}, true);
    VulkanCommandBuffer::allocate(vk_context.device, vk_context.graphics_command_pool.handle, {&vk_context.texture_load_command_buffer, 1}, true);
    VulkanCommandBuffer::allocate(vk_context.device, vk_context.transfer_command_pool.handle, {vk_context.transfer_command_buffers.data(), vk_context.transfer_command_buffers.capacity()}, true);
    
    init_synch_primitives(vk_context);

    return &vk_context.device;
}

// NOTE: main systems should be available at the moment of call
bool renderer_post_init(ArenaAllocator& main_alloc, StackAllocator& temp_alloc) {
    vk_context.texture_load_command_buffer.begin_recording(0);

    preload_textures_and_materials(vk_context.device, vk_context.texture_load_command_buffer, temp_alloc);

    if (!create_main_shader_pipeline()) {
        return false;
    }

    init_meshes(vk_context.pipeline, vk_context.device, vk_context.texture_load_command_buffer, main_alloc, temp_alloc);

    SF_ASSERT_MSG(vk_renderer.meshes.count() > 0, "Should have at least 1 geometry");

    if (!VulkanVertexIndexBuffer::create(vk_context.device, GeometrySystem::get_vertices(), GeometrySystem::get_indices(), vk_context.vertex_index_buffer)) {
        LOG_FATAL("Failed to create vertex buffer");
        return false;
    }

    event_system_add_listener(SystemEventCode::RESIZED, nullptr, renderer_on_resize);
    event_system_add_listener(SystemEventCode::MOUSE_MOVED, nullptr, renderer_handle_mouse_move_event);
    event_system_add_listener(SystemEventCode::KEY_PRESSED, nullptr, renderer_handle_key_press_event);
    event_system_add_listener(SystemEventCode::MOUSE_WHEEL, nullptr, renderer_handle_mouse_wheel_event);

    return true;
}

bool renderer_on_resize(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context) {
    if (code != SystemEventCode::RESIZED || maybe_context.is_none()) {
        return false;
    }

    EventContext& context{ maybe_context.unwrap_ref() };

    vk_context.framebuffer_width = context.data.u16[0];
    vk_context.framebuffer_height = context.data.u16[1];
    vk_context.framebuffer_size_generation++;

    return true;
}

bool renderer_begin_frame(f64 delta_time) {
    vk_renderer.delta_time = delta_time;
    
    vkQueueWaitIdle(vk_context.device.present_queue);

    if (vk_context.swapchain.is_recreating) {
        return false;
    }

    if (vk_context.framebuffer_last_size_generation != vk_context.framebuffer_size_generation) {
        vkDeviceWaitIdle(vk_context.device.logical_device);
        vk_context.swapchain.recreate(vk_context.device, vk_context.surface, vk_context.framebuffer_width, vk_context.framebuffer_height);
        vk_context.framebuffer_last_size_generation = vk_context.framebuffer_size_generation;
        return false;
    }
    
    if (!vk_context.swapchain.acquire_next_image_index(vk_context, UINT64_MAX, vk_context.image_available_semaphores[vk_context.curr_frame].handle, nullptr, vk_context.image_index)) {
        return false;
    }
    
    return true;
}

bool renderer_draw_frame(const RenderPacket& packet) {
    VulkanCommandBuffer& graphics_cmd_buffer = vk_context.graphics_command_buffers[vk_context.curr_frame];
    VulkanCommandBuffer& transfer_cmd_buffer = vk_context.transfer_command_buffers[vk_context.curr_frame];
    VulkanSemaphore& image_available_semaphore = vk_context.image_available_semaphores[vk_context.curr_frame];
    VulkanSemaphore& render_finished_semaphore = vk_context.render_finished_semaphores[vk_context.curr_frame];
    VulkanFence& draw_fence = vk_context.draw_fences[vk_context.curr_frame];
    VulkanFence& transfer_fence = vk_context.transfer_fences[vk_context.curr_frame];
    auto& shader = vk_context.pipeline;
    auto& device = vk_context.device;
    auto& vertex_index_buffer = vk_context.vertex_index_buffer;

    glm::mat4 rotate_mat = glm::mat4(1.0f);
    glm::mat4 translate_mat = glm::mat4(1.0f);
    glm::mat4 scale_mat = glm::mat4(1.0f);
    glm::mat4 result_mat;
    // NOTE: TEMP
    // glm::mat4 identity_mat = glm::mat4(1.0f);

    graphics_cmd_buffer.begin_recording(0);
    vertex_index_buffer.bind(graphics_cmd_buffer);
    vk_context.pipeline.bind(graphics_cmd_buffer, vk_context.curr_frame);
    renderer_update_global_state();
    graphics_cmd_buffer.begin_rendering(vk_context);

    // TODO + THINK: move transfer logic to initialization
    // transfer geometry data to GPU
    transfer_cmd_buffer.begin_recording(0);

    VkBufferCopy vertex_copy_region{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = vertex_index_buffer.vertices_size_bytes 
    };

    VkBufferCopy index_copy_region{
        .srcOffset = vertex_index_buffer.vertices_size_bytes,
        .dstOffset = vertex_index_buffer.vertices_size_bytes,
        .size = vertex_index_buffer.whole_size_bytes - vertex_index_buffer.vertices_size_bytes
    };

    transfer_cmd_buffer.copy_data_between_buffers(vertex_index_buffer.staging_buffer.handle, vertex_index_buffer.main_buffer.handle, vertex_copy_region);
    transfer_cmd_buffer.copy_data_between_buffers(vertex_index_buffer.staging_buffer.handle, vertex_index_buffer.main_buffer.handle, index_copy_region);
    transfer_cmd_buffer.end_recording();

    VkSubmitInfo transfer_submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &transfer_cmd_buffer.handle  
    };

    transfer_cmd_buffer.submit(vk_context, vk_context.device.transfer_queue, transfer_submit_info, {transfer_fence});
    if (!transfer_fence.wait(vk_context)) {
        return false;
    }
    transfer_cmd_buffer.reset();

    auto& meshes = vk_renderer.meshes;
    const u32 MESH_CNT = meshes.count();
    static constexpr f32 STEP{ 0.8f };

    // NOTE: TEMP
    // shader.update_model(graphics_cmd_buffer, identity_mat);

    for (u32 i{0}; i < MESH_CNT; ++i) {
        // update model matrix
        f32 mult_x = ((i & 0b1) == 0b1) ? -STEP : STEP;
        f32 mult_y = ((i & 0b1) == 0b0) ? -STEP : STEP;
        rotate_mat = glm::rotate(glm::mat4(1.0f), static_cast<f32>(packet.elapsed_time) * glm::radians(90.0f),
            ((i & 0b1) == 0b1) ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f));
        translate_mat = glm::translate(glm::mat4(1.0f), glm::vec3(mult_x * i, mult_y * i, 1.0f));
        scale_mat = glm::scale(glm::mat4(1.0f), { 3.0f, 3.0f, 3.0f });
        result_mat = { translate_mat * rotate_mat * scale_mat };
        shader.update_model(graphics_cmd_buffer, result_mat);

        // update material
        MaterialUpdateData material_data{};
        material_data.descriptor_state_index = meshes[i].descriptor_state_index;
        material_data.material = meshes[i].material;
        shader.update_material(vk_context, graphics_cmd_buffer, material_data);

        meshes[i].draw(graphics_cmd_buffer);
    }

    graphics_cmd_buffer.end_rendering(vk_context);
    graphics_cmd_buffer.end_recording();

    VkPipelineStageFlags wait_dest_mask{ VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &image_available_semaphore.handle,
        .pWaitDstStageMask = &wait_dest_mask,
        .commandBufferCount = 1,
        .pCommandBuffers = &graphics_cmd_buffer.handle,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &render_finished_semaphore.handle,
    };

    graphics_cmd_buffer.submit(vk_context, vk_context.device.present_queue, submit_info, draw_fence);

    if (!draw_fence.wait(vk_context)) {
        return false;
    }
    graphics_cmd_buffer.reset();

    vk_context.swapchain.present(vk_context, vk_context.device.present_queue, render_finished_semaphore.handle, vk_context.image_index);

    return true;
}

void renderer_end_frame(f64 delta_time) {
    vk_context.draw_fences[vk_context.curr_frame].reset(vk_context);
    vk_context.transfer_fences[vk_context.curr_frame].reset(vk_context);
    ++vk_renderer.frame_count;
    vk_context.curr_frame = (vk_context.curr_frame + 1) % VulkanSwapchain::MAX_FRAMES_IN_FLIGHT;
}

void sf_vk_check(VkResult vk_result) {
    if (vk_result != VK_SUCCESS) {
        panic("Vk check failed!");
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL sf_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           message_severity,
    VkDebugUtilsMessageTypeFlagsEXT                  message_types,
    const VkDebugUtilsMessengerCallbackDataEXT*      callback_data,
    void*                                            user_data
) {
    switch (message_severity) {
        default:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            LOG_ERROR("{}", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            LOG_WARN("{}", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            LOG_INFO("{}", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            LOG_TRACE("{}", callback_data->pMessage);
            break;
    }
    return VK_FALSE;
}

VulkanContext::~VulkanContext() {
    if (vk_context.device.logical_device) {
        vkDeviceWaitIdle(vk_context.device.logical_device);
    }

    destroy_synch_primitives(*this);
    
    if (transfer_command_pool.handle) {
        for (auto& cmd_buffer : transfer_command_buffers) {
            cmd_buffer.reset();
            cmd_buffer.free(device, transfer_command_pool.handle);
        }
    }

    texture_load_command_buffer.reset();
    texture_load_command_buffer.free(device, graphics_command_pool.handle);

    if (graphics_command_pool.handle) {
        for (auto& cmd_buffer : graphics_command_buffers) {
            cmd_buffer.reset();
            cmd_buffer.free(device, graphics_command_pool.handle);
        }
    }

    vertex_index_buffer.destroy(vk_context.device);

    transfer_command_pool.destroy(*this);
    graphics_command_pool.destroy(*this);

    pipeline.destroy(device);

    swapchain.destroy(device);

    vk_context.device.destroy(*this);

    if (surface) {
        vkDestroySurfaceKHR(instance, surface, &allocator);
        surface = nullptr;
    }

#ifdef SF_DEBUG
    if (debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT debug_destroy_fn = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        debug_destroy_fn(instance, debug_messenger, &allocator);
    }
#endif

    if (instance) {
        vkDestroyInstance(instance, &allocator);
        instance = nullptr;
    }
}

bool create_instance(ApplicationConfig& config, PlatformState& platform_state) {
    vk_renderer.platform_state = &platform_state;
    vk_renderer.frame_count = 0;

    VkApplicationInfo vk_app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = config.name,
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "SNOWFLAKE_ENGINE",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_4,
    };

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &vk_app_info,
    };

    // Extensions
    FixedArray<const char*, VK_MAX_EXTENSION_COUNT> required_extensions;
    required_extensions.append(VK_KHR_SURFACE_EXTENSION_NAME);
    platform_get_required_extensions(required_extensions);

#ifdef SF_DEBUG
    required_extensions.append(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    if (!init_extensions(create_info, required_extensions)) {
        return false;
    }

    // Layers
#ifdef SF_DEBUG
    FixedArray<const char*, 1> required_validation_layers;
    required_validation_layers.append("VK_LAYER_KHRONOS_validation");

    if (!init_validation_layers(create_info, required_validation_layers)) {
        return false;
    }
#endif

    // TODO: custom allocator
    sf_vk_check(vkCreateInstance(&create_info, nullptr, &vk_context.instance));

#ifdef SF_DEBUG
    create_debugger();
#endif

    vk_context.framebuffer_width = config.window_width;
    vk_context.framebuffer_height = config.window_height;

    return true;
}

void create_debugger() {
#ifdef SF_DEBUG
    u32 log_severity =  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
                        // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = log_severity,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = sf_vk_debug_callback,
    };

    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk_context.instance, "vkCreateDebugUtilsMessengerEXT");

    SF_ASSERT_MSG(func, "Failed to create debug messenger!");

    // TODO: custom allocator
    sf_vk_check(func(vk_context.instance, &debug_create_info, nullptr, &vk_context.debug_messenger));
#endif
}

bool init_extensions(VkInstanceCreateInfo& create_info, FixedArray<const char*, VK_MAX_EXTENSION_COUNT>& required_extensions) {
#ifdef SF_DEBUG
    // Log required extensions
    LOG_INFO("Required vulkan extensions: ");
    for (const char* ext_name : required_extensions) {
        LOG_INFO("{}", ext_name);
    }
#endif
    u32 available_extensions_count{0};
    vkEnumerateInstanceExtensionProperties(nullptr, &available_extensions_count, nullptr);

    FixedArray<VkExtensionProperties, 40> available_extensions(available_extensions_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &available_extensions_count, available_extensions.data());

    // check if we have all required extensions
    for (const char* req_ext_name : required_extensions) {
        bool is_found{false};
        for (u32 i{0}; i < available_extensions_count; ++i) {
            const auto& av_ext = available_extensions[i];
            if (sf_str_cmp(req_ext_name, av_ext.extensionName)) {
                is_found = true;
                break;
            }
        }
        if (!is_found) {
            LOG_FATAL("Missing required extension: {}", req_ext_name);
            return false;
        }
    }

    create_info.enabledExtensionCount = required_extensions.count();
    create_info.ppEnabledExtensionNames = required_extensions.data();

    return true;
}

bool init_validation_layers(VkInstanceCreateInfo& create_info, FixedArray<const char*, REQUIRED_VALIDATION_LAYER_CAPACITY>& required_validation_layers) {
    u32 available_layer_count = 0;
    sf_vk_check(vkEnumerateInstanceLayerProperties(&available_layer_count, 0));
    FixedArray<VkLayerProperties, 30> available_layers(available_layer_count);
    sf_vk_check(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers.data()));

    for (const char* req_layer : required_validation_layers) {
        bool is_available = false;
        for (const VkLayerProperties& available_layer : available_layers) {
            if (sf_str_cmp(req_layer, available_layer.layerName)) {
                is_available = true;
                break;
            }
        }

        if (!is_available) {
            LOG_FATAL("Required validation layer is missing: {}", req_layer);
            return false;
        }
    }

    create_info.enabledLayerCount = required_validation_layers.count();
    create_info.ppEnabledLayerNames = required_validation_layers.data();
    return true;
}

void init_synch_primitives(VulkanContext& context) {
    for (u32 i{0}; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        context.image_available_semaphores[i] = VulkanSemaphore::create(context);
        context.render_finished_semaphores[i] = VulkanSemaphore::create(context);
        context.draw_fences[i] = VulkanFence::create(context, false);
        context.transfer_fences[i] = VulkanFence::create(context, false);
    }
}

void destroy_synch_primitives(VulkanContext& context) {
    for (u32 i{0}; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        context.image_available_semaphores[i].destroy(context);
        context.render_finished_semaphores[i].destroy(context);
        context.draw_fences[i].destroy(context);
        context.transfer_fences[i].destroy(context);
    }
}

static bool create_main_shader_pipeline() {
    FixedArray<VkVertexInputAttributeDescription, VulkanShaderPipeline::MAX_ATTRIB_COUNT> attrib_descriptions(MAIN_PIPELINE_ATTRIB_COUNT);
    // Position
    attrib_descriptions[0].binding = 0;
    attrib_descriptions[0].location = 0;
    attrib_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrib_descriptions[0].offset = 0;
    // Normal
    attrib_descriptions[1].binding = 0;
    attrib_descriptions[1].location = 1;
    attrib_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrib_descriptions[1].offset = sizeof(glm::vec3);
    // Texture sampler
    attrib_descriptions[2].binding = 0;
    attrib_descriptions[2].location = 2;
    attrib_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrib_descriptions[2].offset = sizeof(glm::vec3) * 2;

    VkViewport viewport{ vk_context.get_viewport() };
    VkRect2D scissors{ vk_context.get_scissors() };
    auto& temp_alloc = application_get_temp_allocator();

    return VulkanShaderPipeline::create(
        vk_context,
        MAIN_SHADER_FILE_NAME,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        true,     
        attrib_descriptions,
        viewport,
        scissors,
        preload_textures.to_span(),
        vk_context.pipeline,
        temp_alloc
    );
}

static void init_global_descriptors(const VulkanDevice& device) {
    FixedArray<VkDescriptorPoolSize, 1> pool_sizes{{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = VulkanSwapchain::MAX_FRAMES_IN_FLIGHT }};
    VulkanDescriptorPool::create(device, {pool_sizes.data(), pool_sizes.count()}, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT, vk_context.global_descriptor_pool);

    FixedArray<VkDescriptorType, 1> types = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
    FixedArray<VkDescriptorSetLayoutBinding, 1> bindings(1);
    VulkanDescriptorSetLayout::create_bindings(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, {types.data(), types.count()}, {bindings.data(), bindings.count()});
    VulkanDescriptorSetLayout::create(device, {bindings.data(), bindings.count()}, vk_context.global_descriptor_layout);

    FixedArray<VkDescriptorSetLayout, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT> layouts(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    layouts.fill(vk_context.global_descriptor_layout.handle);

    vk_context.global_descriptor_pool.allocate_sets(device, vk_context.global_descriptor_sets.count(), reinterpret_cast<VkDescriptorSetLayout*>(layouts.data()), vk_context.global_descriptor_sets.data());

    update_global_descriptors(device);
}

static bool renderer_handle_mouse_move_event(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context)  {
    SF_ASSERT_MSG(maybe_context.is_some(), "Context should be available");

    EventContext& context{ maybe_context.unwrap_ref() };
    f32 delta_x = context.data.f32[0];
    f32 delta_y = context.data.f32[1];
    Camera& camera{ vk_renderer.camera };
    camera.yaw += delta_x * MouseDelta::SENSITIVITY;
    camera.pitch = std::clamp(camera.pitch + delta_y * MouseDelta::SENSITIVITY, -89.0f, 89.0f);
    camera.update_vectors();
    vk_renderer.camera.dirty = true;
    return false;
}

Camera::Camera()
{
    update_vectors();
}

void Camera::update_vectors() {
    target.x = glm::cos(glm::radians(yaw)) * glm::cos(glm::radians(pitch));
    target.y = glm::sin(glm::radians(pitch));
    target.z = glm::sin(glm::radians(yaw)) * glm::cos(glm::radians(pitch));
    target   = glm::normalize(target);
    right    = glm::normalize(glm::cross(target, Camera::WORLD_UP));
    up       = glm::normalize(glm::cross(right, target));
}

static bool renderer_handle_key_press_event(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context)  {
    SF_ASSERT_MSG(maybe_context.is_some(), "Event context should be available");
    u16 key{ maybe_context.unwrap_ref().data.u16[0] };
    Camera& camera{ vk_renderer.camera };
    const f32 velocity = camera.speed * static_cast<f32>(vk_renderer.delta_time);

    switch (key) {
        case GLFW_KEY_W: {
            camera.pos += camera.target * velocity;
        } break;
        case GLFW_KEY_S: {
            camera.pos -= camera.target * velocity;
        } break;
        case GLFW_KEY_A: {
            camera.pos -= camera.right * velocity;
        } break;
        case GLFW_KEY_D: {
            camera.pos += camera.right * velocity;
        } break;
        default: break;
    }

    vk_renderer.camera.dirty = true;
    return false;
}

static bool renderer_handle_mouse_wheel_event(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context) {
    SF_ASSERT_MSG(maybe_context.is_some(), "Event context should be available");
    EventContext& context{ maybe_context.unwrap_ref() };
    i8 delta{ context.data.i8[0] };

    if (delta > 0) {
        vk_renderer.camera.zoom += Camera::ZOOM_SPEED;
    } else {
        vk_renderer.camera.zoom -= Camera::ZOOM_SPEED;
    }

    vk_renderer.camera.zoom = sf_clamp<f32>(vk_renderer.camera.zoom, Camera::NEAR, Camera::FAR);
    vk_renderer.camera.dirty = true;
    return false;
}

static void renderer_update_global_state() {
    if (!vk_renderer.camera.dirty) {
        return;
    }

    glm::mat4 view = glm::lookAt(vk_renderer.camera.pos, vk_renderer.camera.pos + vk_renderer.camera.target, vk_renderer.camera.up);
    glm::mat4 proj = glm::perspective(glm::radians(vk_renderer.camera.zoom), sf::renderer_get_aspect_ratio(), Camera::NEAR, Camera::FAR);
    proj[1][1] *= -1.0f;
    renderer_update_global_ubo(view, proj);
    vk_renderer.camera.dirty = false;
}

const VulkanDevice& renderer_get_device() {
    return vk_context.device;
}

// THINK: maybe should update only for current frame
SF_EXPORT void renderer_update_global_ubo(const glm::mat4& view, const glm::mat4& proj) {
    vk_renderer.global_ubo.update(vk_context.curr_frame, view, proj);
    update_global_descriptors(vk_context.device);
}

SF_EXPORT void renderer_update_view(const glm::mat4& view) {
    vk_renderer.global_ubo.update_view(view);
    update_global_descriptors(vk_context.device);
}

SF_EXPORT void renderer_update_proj(const glm::mat4& proj) {
    vk_renderer.global_ubo.update_proj(proj);
    update_global_descriptors(vk_context.device);
}

static void update_global_descriptors(const VulkanDevice& device) {
    for (u32 i{0}; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorBufferInfo buffer_info{
            .buffer = vk_renderer.global_ubo.buffers[i].handle,
            .offset = 0,
            .range = sizeof(GlobalUniformObject)
        };

        VkWriteDescriptorSet descriptor_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vk_context.global_descriptor_sets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &buffer_info,
            .pTexelBufferView = nullptr,
        };

        vkUpdateDescriptorSets(device.logical_device, 1, &descriptor_write, 0, nullptr);
    }
}

static void preload_textures_and_materials(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& temp_alloc) {
    u32 cnt = preload_texture_file_names.count();
    for (u32 i{0}; i < cnt; ++i) {
        preload_texture_configs.append(TextureInputConfig{ TextureSystem::acquire_default_texture_path(preload_texture_file_names[i], temp_alloc) });
    }
    TextureSystem::get_or_load_textures_many(device, cmd_buffer, temp_alloc, preload_texture_configs.to_span(), preload_textures.to_span());
    MaterialSystem::load_and_get_material_from_file_many(device, cmd_buffer, temp_alloc, preload_material_configs.to_span(), preload_materials.to_span());
}

static void renderer_create_default_meshes(const VulkanDevice& device, VulkanShaderPipeline& shader, VulkanCommandBuffer& cmd_buffer, StackAllocator& temp_alloc, u32 count) {
    SF_ASSERT_MSG(count > 0, "Should create at least 1 default mesh");
    
    GeometryView& default_geometry = GeometrySystem::get_default_geometry_view();
    MaterialSystem::create_default_material(device, cmd_buffer, temp_alloc);
    Material& default_material = MaterialSystem::get_default_material();
    for (u32 i = 0; i < count; ++i) {
        Mesh new_mesh;
        new_mesh.descriptor_state_index = shader.acquire_resouces(device);
        Mesh::create_from_existing_data(shader, device, &default_geometry, &default_material, new_mesh);
        vk_renderer.meshes.append(new_mesh);
    }
}

static void init_meshes(VulkanShaderPipeline& shader, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, ArenaAllocator& main_alloc, StackAllocator& temp_alloc) {
    if (DEFAULT_MESH_COUNT > 0) {
        renderer_create_default_meshes(vk_context.device, shader, vk_context.texture_load_command_buffer, temp_alloc, DEFAULT_MESH_COUNT);
    }

    for (u32 i{0}; i < model_names.count(); ++i) {
        if (!Model::load(model_names[i], main_alloc, temp_alloc, vk_context.device, vk_context.texture_load_command_buffer, vk_context.pipeline, models[i])) {
            LOG_ERROR("Model with name {} fails to load", model_names[i]);
        }
        for (auto& mesh : models[i].meshes) {
            vk_renderer.meshes.append(mesh);
        }
    }

    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer.handle
    };

    cmd_buffer.end_recording();
    cmd_buffer.submit(vk_context, vk_context.device.graphics_queue, submit_info, {None::VALUE});
    vkQueueWaitIdle(vk_context.device.graphics_queue);
    cmd_buffer.reset();
}

VkViewport VulkanContext::get_viewport() const {
    return {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(framebuffer_width),
        .height = static_cast<float>(framebuffer_height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
}

VkRect2D VulkanContext::get_scissors() const {
    return VkRect2D{
        .offset = { 0, 0 },
        .extent = { framebuffer_width, framebuffer_height }
    };
}

SF_EXPORT f32 renderer_get_aspect_ratio() {
    return static_cast<f32>(vk_context.framebuffer_width) / vk_context.framebuffer_height;
}

VulkanCommandBuffer& VulkanContext::curr_frame_graphics_cmd_buffer() {
    return graphics_command_buffers[curr_frame];
}

} // sf
