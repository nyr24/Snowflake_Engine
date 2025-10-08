#include "sf_containers/optional.hpp"
#include "sf_containers/result.hpp"
#include "sf_core/application.hpp"
#include "sf_core/event.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_core/utility.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/pipeline.hpp"
#include "sf_vulkan/resource.hpp"
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
#include <span>
#include <string_view>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/trigonometric.hpp"

namespace sf {

static VulkanRenderer vk_renderer{};
static VulkanContext vk_context{};

static constexpr u32 REQUIRED_VALIDATION_LAYER_CAPACITY{ 1 };
static constexpr u32 MAIN_PIPELINE_ATTRIB_COUNT{ 2 };
static constexpr u32 MAX_TEXTURE_COUNT{ 10 };
static const char* MAIN_SHADER_FILE_NAME{"shader.spv"};

// NOTE: testing
struct ObjectRenderData {
    Texture*    texture;  
    u32         id;
};

static const FixedArray<std::string_view, MAX_TEXTURE_COUNT> texture_names{ /* "brick_wall.jpg", "brick.jpg", "ocean.jpg", */ "mickey_mouse.jpg" }; 
static FixedArray<Texture, MAX_TEXTURE_COUNT> textures;
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
bool create_main_shader_pipeline();
bool create_textures();
bool init_objects_render_data();
void update_global_state(const VulkanDevice& device, VulkanShaderPipeline& pipeline, u32 curr_frame, f32 aspect_ratio);
void update_object_state(VulkanShaderPipeline& pipeline, VulkanCommandBuffer& cmd_buffer, f64 elapsed_time);

bool renderer_init(ApplicationConfig& config, PlatformState& platform_state) {
    if (!create_instance(config, platform_state)) {
        return false;
    }

    platform_create_vk_surface(platform_state, vk_context);

    if (!VulkanDevice::create(vk_context)) {
        return false;
    }

    if (!VulkanSwapchain::create(vk_context.device, vk_context.surface, vk_context.framebuffer_width, vk_context.framebuffer_height, vk_context.swapchain)) {
        return false;
    }

    if (!create_main_shader_pipeline()) {
        return false;
    }

    VulkanCommandPool::create(vk_context, VulkanCommandPoolType::GRAPHICS, vk_context.device.queue_family_info.graphics_family_index,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, vk_context.graphics_command_pool);
    VulkanCommandPool::create(vk_context, VulkanCommandPoolType::TRANSFER, vk_context.device.queue_family_info.transfer_family_index,
        static_cast<VkCommandPoolCreateFlagBits>(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT), vk_context.transfer_command_pool);

    if (!VulkanVertexIndexBuffer::create(vk_context.device, Mesh::get_cube_mesh(), vk_context.vertex_index_buffer)) {
       return false; 
    }

    VulkanCommandBuffer::allocate(vk_context, vk_context.graphics_command_pool.handle, std::span{vk_context.graphics_command_buffers.data(), vk_context.graphics_command_buffers.capacity()}, true);
    VulkanCommandBuffer::allocate(vk_context, vk_context.transfer_command_pool.handle, std::span{vk_context.transfer_command_buffers.data(), vk_context.transfer_command_buffers.capacity()}, true);

    init_synch_primitives(vk_context);

    event_set_listener(SystemEventCode::RESIZED, nullptr, renderer_on_resize);

    if (!create_textures()) {
        return false;
    }
    
    if (!init_objects_render_data()) {
        return false;
    }

    return true;
}

bool renderer_on_resize(u8 code, void* sender, void* listener_inst, EventContext* context) {
    if (code != SystemEventCode::RESIZED || !context) {
        return false;
    }

    vk_context.framebuffer_width = static_cast<u16>(context->data.u16[0]);
    vk_context.framebuffer_height = static_cast<u16>(context->data.u16[1]);
    vk_context.framebuffer_size_generation++;

    return true;
}

bool renderer_begin_frame(f64 delta_time) {
    vk_context.frame_delta_time = delta_time;

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
    curr_transfer_buffer.end_recording(vk_context);

    VkSubmitInfo transfer_submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &curr_transfer_buffer.handle  
    };

    curr_transfer_buffer.submit(vk_context, vk_context.device.transfer_queue, transfer_submit_info, {curr_transfer_fence});

    curr_graphics_buffer.begin_recording(0);
    vk_context.pipeline.bind(curr_graphics_buffer, vk_context.curr_frame);
    update_object_state(vk_context.pipeline, curr_graphics_buffer, packet.elapsed_time);
    update_global_state(vk_context.device, vk_context.pipeline, vk_context.curr_frame, vk_context.get_aspect_ratio());
    vk_context.vertex_index_buffer.bind(curr_graphics_buffer);
    curr_graphics_buffer.begin_rendering(vk_context);
    vk_context.vertex_index_buffer.draw(curr_graphics_buffer);
    curr_graphics_buffer.end_rendering(vk_context);
    curr_graphics_buffer.end_recording(vk_context);
    
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
    curr_transfer_buffer.reset(vk_context);

    if (!curr_draw_fence.wait(vk_context)) {
        return false;
    }
    curr_graphics_buffer.reset(vk_context);

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
    graphics_command_buffers.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    transfer_command_buffers.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    image_available_semaphores.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    render_finished_semaphores.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    draw_fences.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    transfer_fences.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
}

VulkanContext::~VulkanContext() {
    vkDeviceWaitIdle(vk_context.device.logical_device);

    default_texture.destroy(vk_context.device);

    destroy_synch_primitives(*this);
    
    if (transfer_command_pool.handle) {
        for (auto& cmd_buffer : transfer_command_buffers) {
            cmd_buffer.free(*this, transfer_command_pool.handle);
        }
    }
    if (graphics_command_pool.handle) {
        for (auto& cmd_buffer : graphics_command_buffers) {
            cmd_buffer.free(*this, graphics_command_pool.handle);
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

// TODO: generate attrib descriptions from the input type of Vertex inside a pipeline
bool create_main_shader_pipeline() {
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
        vk_context.pipeline
    );
}


// TODO: textures temp shared cmd_buffer
bool create_textures() {
    textures.resize(texture_names.count());
    
    for (u32 i{0}; i < texture_names.count(); ++i) {
        if (!Texture::create(vk_context, texture_names[i].data(), true, textures[i])) {
            LOG_ERROR("Error when trying to create default texture with name: {}", texture_names[i]);
            return false;
        }
    }
    
    return true;
}

bool init_objects_render_data() {
    for (u32 i{0}; i < texture_names.count(); ++i) {
        Result<u32> maybe_resource_id = vk_context.pipeline.acquire_resouces(vk_context.device);
        if (maybe_resource_id.is_err()) {
            return false;
        }
    
        object_render_data.append(ObjectRenderData{
            .texture = &textures[i],
            .id = maybe_resource_id.unwrap_copy()
        });
    }

    return true;
}

void update_global_state(const VulkanDevice& device, VulkanShaderPipeline& pipeline, u32 curr_frame, f32 aspect_ratio) {
    glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(45.0f, aspect_ratio, 0.1f, 10.0f);
    proj[1][1] *= -1.0f;

    pipeline.update_global_state(device, curr_frame, view, proj);
}

// THINK: bind then update or otherwise?
void update_object_state(VulkanShaderPipeline& pipeline, VulkanCommandBuffer& cmd_buffer, f64 elapsed_time) {
    SF_ASSERT_MSG(object_render_data.count() == texture_names.count(), "Each object should have a bounded texture");
    
    glm::mat4 rotate_mat{ glm::rotate(glm::mat4(1.0f), static_cast<f32>(elapsed_time) * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)) };

    for (u32 i{0}; i < object_render_data.count(); ++i) {
        GeometryRenderData render_data{};
        // glm::mat4 translate_mat{ glm::translate(glm::mat4(1.0f), glm::vec3(1.50f, 0.00f, 0.00f)) };
        render_data.model = { /* translate_mat */ rotate_mat };
        render_data.id = object_render_data[i].id;
        render_data.textures[0] = object_render_data[i].texture;
        pipeline.update_object_state(vk_context, render_data);
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

f32 VulkanContext::get_aspect_ratio() const {
    return static_cast<f32>(framebuffer_width) / framebuffer_height;
}

VulkanCommandBuffer& VulkanContext::curr_frame_graphics_cmd_buffer() {
    return graphics_command_buffers[curr_frame];
}

} // sf
