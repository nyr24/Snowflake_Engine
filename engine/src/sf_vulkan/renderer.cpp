#include "glm/geometric.hpp"
#include "sf_containers/optional.hpp"
#include "sf_containers/result.hpp"
#include "sf_core/application.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/event.hpp"
#include "sf_core/input.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_core/utility.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/pipeline.hpp"
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
// #include "sf_vulkan/allocator.hpp"
// #include "sf_allocators/free_list_allocator.hpp"
// #include <list>
#include <algorithm>
#include <span>
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
static constexpr u32 MAIN_PIPELINE_ATTRIB_COUNT{ 2 };
static const char* MAIN_SHADER_FILE_NAME{"shader.spv"};

struct ObjectRenderData {
    Texture*    texture;  
    u32         id;
};

static const FixedArray<TextureInputConfig, VulkanShaderPipeline::MAX_DEFAULT_TEXTURES> texture_create_configs{
    {"brick_wall.jpg"}, {"asphalt.jpg"}, {"grass.jpg"}, {"painting.jpg"}, {"fabric_suit.jpg"}, {"mickey_mouse.jpg"}
}; 
static FixedArray<ObjectRenderData, MAX_OBJECT_COUNT> object_render_data;

bool create_instance(ApplicationConfig& config, PlatformState& platform_state);
void create_debugger();
bool init_extensions(VkInstanceCreateInfo& create_info, FixedArray<const char*, REQUIRED_EXTENSION_CAPACITY>& required_extensions);
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
static bool create_main_shader_pipeline(std::span<const TextureInputConfig> texture_configs);
static bool create_initial_textures(VulkanCommandBuffer& cmd_buffer);
static bool init_objects_render_data(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, VulkanShaderPipeline& pipeline);
static void update_global_state();
static void shader_update_object_state(VulkanShaderPipeline& shader, VulkanCommandBuffer& cmd_buffer, f64 elapsed_time);
static void init_global_descriptors(const VulkanDevice& device);
static void update_global_descriptors(const VulkanDevice& device);
static void renderer_update_global_state();
static bool renderer_handle_mouse_move_event(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context);
static bool renderer_handle_key_press_event(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context);
static bool renderer_handle_mouse_wheel_event(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context);

bool renderer_init(ApplicationConfig& config, PlatformState& platform_state) {
    if (!create_instance(config, platform_state)) {
        return false;
    }

    platform_create_vk_surface(platform_state, vk_context);

    if (!VulkanDevice::create(vk_context)) {
        return false;
    }

    // Global descriptors
    if (!VulkanGlobalUniformBufferObject::create(vk_context.device, vk_renderer.global_ubo)) {
        return false;
    }
    init_global_descriptors(vk_context.device);

    if (!VulkanSwapchain::create(vk_context.device, vk_context.surface, vk_context.framebuffer_width, vk_context.framebuffer_height, vk_context.swapchain)) {
        return false;
    }

    VulkanCommandPool::create(vk_context, VulkanCommandPoolType::GRAPHICS, vk_context.device.queue_family_info.graphics_family_index,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, vk_context.graphics_command_pool);
    VulkanCommandPool::create(vk_context, VulkanCommandPoolType::TRANSFER, vk_context.device.queue_family_info.transfer_family_index,
        static_cast<VkCommandPoolCreateFlagBits>(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT), vk_context.transfer_command_pool);

    if (!VulkanVertexIndexBuffer::create(vk_context.device, Mesh::get_cube_mesh(), vk_context.vertex_index_buffer)) {
       return false; 
    }

    VulkanCommandBuffer::allocate(vk_context.device, vk_context.graphics_command_pool.handle, {vk_context.graphics_command_buffers.data(), vk_context.graphics_command_buffers.capacity()}, true);
    VulkanCommandBuffer::allocate(vk_context.device, vk_context.graphics_command_pool.handle, {&vk_context.texture_load_command_buffer, 1}, true);
    VulkanCommandBuffer::allocate(vk_context.device, vk_context.transfer_command_pool.handle, {vk_context.transfer_command_buffers.data(), vk_context.transfer_command_buffers.capacity()}, true);

    if (!create_initial_textures(vk_context.texture_load_command_buffer)) {
        return false;
    }

    if (!create_main_shader_pipeline({texture_create_configs.data() + 2, 2})) {
        return false;
    }

    if (!init_objects_render_data(vk_context.device, vk_context.texture_load_command_buffer, vk_context.pipeline)) {
        return false;
    }

    init_synch_primitives(vk_context);

    event_set_listener(SystemEventCode::RESIZED, nullptr, renderer_on_resize);
    event_set_listener(SystemEventCode::MOUSE_MOVED, nullptr, renderer_handle_mouse_move_event);
    event_set_listener(SystemEventCode::KEY_PRESSED, nullptr, renderer_handle_key_press_event);
    event_set_listener(SystemEventCode::MOUSE_WHEEL, nullptr, renderer_handle_mouse_wheel_event);

    return true;
}

bool renderer_on_resize(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context) {
    if (code != SystemEventCode::RESIZED || maybe_context.is_none()) {
        return false;
    }

    EventContext& context{ maybe_context.unwrap() };

    vk_context.framebuffer_width = static_cast<u16>(context.data.u16[0]);
    vk_context.framebuffer_height = static_cast<u16>(context.data.u16[1]);
    vk_context.framebuffer_size_generation++;

    return true;
}

bool renderer_begin_frame(f64 delta_time) {
    vk_renderer.delta_time = delta_time;
    
    vkQueueWaitIdle(vk_context.device.present_queue);

    if (vk_context.recreating_swapchain) {
        return false;
    }

    if (vk_context.framebuffer_last_size_generation != vk_context.framebuffer_size_generation) {
        vk_context.recreating_swapchain = true;
        vkDeviceWaitIdle(vk_context.device.logical_device);
        vk_context.swapchain.recreate(vk_context.device, vk_context.surface, vk_context.framebuffer_width, vk_context.framebuffer_height);
        vk_context.recreating_swapchain = false;
        vk_context.framebuffer_last_size_generation = vk_context.framebuffer_size_generation;
        return false;
    }
    
    if (!vk_context.swapchain.acquire_next_image_index(vk_context, UINT64_MAX, vk_context.image_available_semaphores[vk_context.curr_frame].handle, nullptr, vk_context.image_index)) {
        return false;
    }
    
    return true;
}

bool renderer_draw_frame(const RenderPacket& packet) {
    // THINK: can we do all stuff with copying buffer data on renderer_init()?
    VulkanCommandBuffer& curr_graphics_buffer = vk_context.graphics_command_buffers[vk_context.curr_frame];
    VulkanCommandBuffer& curr_transfer_buffer = vk_context.transfer_command_buffers[vk_context.curr_frame];
    VulkanSemaphore& curr_image_available_semaphore = vk_context.image_available_semaphores[vk_context.curr_frame];
    VulkanSemaphore& curr_render_finished_semaphore = vk_context.render_finished_semaphores[vk_context.curr_frame];
    VulkanFence& curr_draw_fence = vk_context.draw_fences[vk_context.curr_frame];
    VulkanFence& curr_transfer_fence = vk_context.transfer_fences[vk_context.curr_frame];

    // THINK: we maybe need semaphore to synch copy and draw commands
    curr_transfer_buffer.begin_recording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkBufferCopy vertex_copy_region{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = vk_context.vertex_index_buffer.indeces_offset 
    };

    VkBufferCopy index_copy_region{
        .srcOffset = vk_context.vertex_index_buffer.indeces_offset,
        .dstOffset = vk_context.vertex_index_buffer.indeces_offset,
        .size = vk_context.vertex_index_buffer.indeces_count * sizeof(u16)
    };
    
    curr_transfer_buffer.copy_data_between_buffers(vk_context.vertex_index_buffer.staging_buffer.handle, vk_context.vertex_index_buffer.main_buffer.handle, vertex_copy_region);
    curr_transfer_buffer.copy_data_between_buffers(vk_context.vertex_index_buffer.staging_buffer.handle, vk_context.vertex_index_buffer.main_buffer.handle, index_copy_region);
    curr_transfer_buffer.end_recording();

    VkSubmitInfo transfer_submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &curr_transfer_buffer.handle  
    };

    curr_transfer_buffer.submit(vk_context, vk_context.device.transfer_queue, transfer_submit_info, {curr_transfer_fence});

    curr_graphics_buffer.begin_recording(0);
    vk_context.pipeline.bind(curr_graphics_buffer, vk_context.curr_frame);
    renderer_update_global_state();
    shader_update_object_state(vk_context.pipeline, curr_graphics_buffer, packet.elapsed_time);
    vk_context.vertex_index_buffer.bind(curr_graphics_buffer);
    curr_graphics_buffer.begin_rendering(vk_context);
    vk_context.vertex_index_buffer.draw(curr_graphics_buffer);
    curr_graphics_buffer.end_rendering(vk_context);
    curr_graphics_buffer.end_recording();
    
    VkPipelineStageFlags wait_dest_mask{ VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &curr_image_available_semaphore.handle,
        .pWaitDstStageMask = &wait_dest_mask,
        .commandBufferCount = 1,
        .pCommandBuffers = &curr_graphics_buffer.handle,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &curr_render_finished_semaphore.handle,
    };

    curr_graphics_buffer.submit(vk_context, vk_context.device.present_queue, submit_info, Option<VulkanFence>{curr_draw_fence});

    if (!curr_transfer_fence.wait(vk_context)) {
        return false;
    }
    curr_transfer_buffer.reset();

    if (!curr_draw_fence.wait(vk_context)) {
        return false;
    }
    curr_graphics_buffer.reset();

    vk_context.swapchain.present(vk_context, vk_context.device.present_queue, curr_render_finished_semaphore.handle, vk_context.image_index);
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

VulkanContext::VulkanContext()
    : curr_frame{0}
{
    graphics_command_buffers.resize_to_capacity();
    transfer_command_buffers.resize_to_capacity();
    image_available_semaphores.resize_to_capacity();
    render_finished_semaphores.resize_to_capacity();
    draw_fences.resize_to_capacity();
    transfer_fences.resize_to_capacity();
    global_descriptor_sets.resize_to_capacity();
}

VulkanContext::~VulkanContext() {
    vkDeviceWaitIdle(vk_context.device.logical_device);

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

    if (debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT debug_destroy_fn = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        debug_destroy_fn(instance, debug_messenger, &allocator);
    }

    if (instance) {
        vkDestroyInstance(instance, &allocator);
        instance = nullptr;
    }
}

bool create_instance(ApplicationConfig& config, PlatformState& platform_state) {
    vk_renderer.platform_state = &platform_state;
    vk_renderer.frame_count = 0;

    VkApplicationInfo vk_app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    vk_app_info.pApplicationName = config.name;
    vk_app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    vk_app_info.pEngineName = "SNOWFLAKE_ENGINE";
    vk_app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    vk_app_info.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pNext = nullptr;
    create_info.pApplicationInfo = &vk_app_info;

    // Extensions
    FixedArray<const char*, REQUIRED_EXTENSION_CAPACITY> required_extensions;
    required_extensions.append(VK_KHR_SURFACE_EXTENSION_NAME);
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

    vk_context.framebuffer_width = config.width;
    vk_context.framebuffer_height = config.height;

    return true;
}

void create_debugger() {
    u32 log_severity =  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
                        // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug_create_info.messageSeverity = log_severity;
    debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_create_info.pfnUserCallback = sf_vk_debug_callback;

    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk_context.instance, "vkCreateDebugUtilsMessengerEXT");

    SF_ASSERT_MSG(func, "Failed to create debug messenger!");

    // TODO: custom allocator
    sf_vk_check(func(vk_context.instance, &debug_create_info, nullptr, &vk_context.debug_messenger));
}

bool init_extensions(VkInstanceCreateInfo& create_info, FixedArray<const char*, REQUIRED_EXTENSION_CAPACITY>& required_extensions) {
    platform_get_required_extensions(required_extensions);
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

static bool create_main_shader_pipeline(std::span<const TextureInputConfig> texture_configs) {
    FixedArray<VkVertexInputAttributeDescription, VulkanShaderPipeline::MAX_ATTRIB_COUNT> attrib_descriptions(MAIN_PIPELINE_ATTRIB_COUNT);
    // Position
    attrib_descriptions[0].binding = 0;
    attrib_descriptions[0].location = 0;
    attrib_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrib_descriptions[0].offset = 0;
    // Texture sampler
    attrib_descriptions[1].binding = 0;
    attrib_descriptions[1].location = 1;
    attrib_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrib_descriptions[1].offset = sizeof(glm::vec3);

    VkViewport viewport{ vk_context.get_viewport() };
    VkRect2D scissors{ vk_context.get_scissors() };

    return VulkanShaderPipeline::create(
        vk_context,
        MAIN_SHADER_FILE_NAME,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        true,     
        attrib_descriptions,
        viewport,
        scissors,
        // assign all loaded textures to this shader
        texture_configs,
        vk_context.pipeline
    );
}

static bool create_initial_textures(VulkanCommandBuffer& cmd_buffer) {
    cmd_buffer.begin_recording(0);
    texture_system_create_textures(vk_context.device, cmd_buffer, {texture_create_configs.data(), texture_create_configs.count()});
    cmd_buffer.end_recording();

    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer.handle
    };

    cmd_buffer.submit(vk_context, vk_context.device.graphics_queue, submit_info, {None::VALUE});
    vkQueueWaitIdle(vk_context.device.graphics_queue);

    return true;
}

static bool init_objects_render_data(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, VulkanShaderPipeline& pipeline) {
    Result<u32> maybe_resource_id = pipeline.acquire_resouces(vk_context.device);
    if (maybe_resource_id.is_err()) {
        return false;
    }

    // texture_system_get_texture(device, cmd_buffer, {"grass.jpg"});

    object_render_data.append({
        .texture = nullptr,
        .id = maybe_resource_id.unwrap_copy()
    });

    return true;
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

    EventContext& context{ maybe_context.unwrap() };

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
    target = glm::normalize(target);

    right = glm::normalize(glm::cross(target, Camera::WORLD_UP));
    up = glm::normalize(glm::cross(right, target));
}

static bool renderer_handle_key_press_event(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context)  {
    SF_ASSERT_MSG(maybe_context.is_some(), "Event context should be available");
    Key key{ maybe_context.unwrap().data.u8[0] };
    Camera& camera{ vk_renderer.camera };
    const f32 velocity = camera.speed * static_cast<f32>(vk_renderer.delta_time);

    switch (key) {
        case Key::W: {
            camera.pos += camera.target * velocity;
        } break;
        case Key::S: {
            camera.pos -= camera.target * velocity;
        } break;
        case Key::A: {
            camera.pos -= camera.right * velocity;
        } break;
        case Key::D: {
            camera.pos += camera.right * velocity;
        } break;
        default: break;
    }

    vk_renderer.camera.dirty = true;
    return false;
}

static bool renderer_handle_mouse_wheel_event(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context) {
    SF_ASSERT_MSG(maybe_context.is_some(), "Event context should be available");
    EventContext& context{ maybe_context.unwrap() };
    i8 delta{ context.data.i8[0] };

    if (delta > 0) {
        vk_renderer.camera.zoom += Camera::ZOOM_SPEED;
    } else {
        vk_renderer.camera.zoom -= Camera::ZOOM_SPEED;
    }

    vk_renderer.camera.dirty = true;
    return false;
}

static void renderer_update_global_state() {
    if (!vk_renderer.camera.dirty) {
        return;
    }

    glm::mat4 view = glm::lookAt(vk_renderer.camera.pos, vk_renderer.camera.pos + vk_renderer.camera.target, vk_renderer.camera.up);
    glm::mat4 proj = glm::perspective(glm::radians(vk_renderer.camera.zoom), sf::renderer_get_aspect_ratio(), 0.1f, 90.0f);
    proj[1][1] *= -1.0f;
    renderer_update_global_ubo(view, proj);

    vk_renderer.camera.dirty = false;
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

static void shader_update_object_state(VulkanShaderPipeline& shader, VulkanCommandBuffer& cmd_buffer, f64 elapsed_time) {
    glm::mat4 rotate_mat{ glm::rotate(glm::mat4(1.0f), static_cast<f32>(elapsed_time) * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 1.0f)) };

    for (u32 i{0}; i < object_render_data.count(); ++i) {
        GeometryRenderData render_data{};
        // glm::mat4 translate_mat{ glm::translate(glm::mat4(1.0f), glm::vec3(1.50f, 0.00f, 0.00f)) };
        render_data.model = { /* translate_mat */ rotate_mat };
        render_data.id = object_render_data[i].id;
        render_data.textures[0] = object_render_data[i].texture;
        shader.update_object_state(vk_context, render_data);
    }
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
