#include "sf_vulkan/pipeline.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float4.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_containers/result.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/io.hpp"
#include "sf_core/logger.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/renderer.hpp"
#include "sf_vulkan/resource.hpp"
#include "sf_vulkan/swapchain.hpp"
#include <vulkan/vulkan_core.h>
#include <filesystem>
#include <span>

namespace fs = std::filesystem;

namespace sf {

VulkanShaderPipeline::VulkanShaderPipeline()
{
    attrib_descriptions.resize_to_capacity();
    global_descriptor_sets.resize_to_capacity();
    object_shader_states.resize_to_capacity();
    for (auto& state : object_shader_states) {
        state.descriptor_sets.resize_to_capacity();
        state.descriptor_states.resize_to_capacity();

         for (auto& state : state.descriptor_states) {
             state.generations.resize_to_capacity();
         }
    }
}
    
bool VulkanShaderPipeline::create(
    const VulkanContext&           context,
    const char*                    shader_file_name,
    VkPrimitiveTopology            primitive_topology,
    bool                           enable_color_blend,
    const FixedArray<VkVertexInputAttributeDescription, MAX_ATTRIB_COUNT>& attrib_description_config,
    VkViewport                     viewport,
    VkRect2D                       scissors,
    VulkanShaderPipeline&          out_pipeline
) {
    #ifdef SF_DEBUG
    fs::path shader_path = fs::current_path() / "build" / "debug/engine/shaders/" / shader_file_name;
    #else
    fs::path shader_path = fs::current_path() / "build" / "release/engine/shaders/" / shader_file_name;
    #endif

    Result<VkShaderModule> maybe_shader_module = create_shader_module(context.device, std::move(shader_path));
    if (maybe_shader_module.is_err()) {
        return false;
    }

    out_pipeline.shader_handle = maybe_shader_module.unwrap();

    FixedArray<VkPipelineShaderStageCreateInfo, 2> shader_stages{
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = out_pipeline.shader_handle,
            .pName = "vertMain",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = out_pipeline.shader_handle,
            .pName = "fragMain"
        }
    };

    out_pipeline.create_attribute_descriptions(attrib_description_config);

    // Global descriptors
    if (!VulkanGlobalUniformBufferObject::create(context.device, out_pipeline.global_ubo)) {
        return false;
    }
    out_pipeline.create_global_descriptors(context.device);

    // Local/Object descriptors
    if (!VulkanLocalUniformBufferObject::create(context.device, out_pipeline.local_ubo)) {
        return false;
    }

    out_pipeline.create_local_descriptors(context.device);

    FixedArray<VkDynamicState, 2> dynamic_state{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = dynamic_state.count(),
        .pDynamicStates = dynamic_state.data()
    };

    VkVertexInputBindingDescription binding_descr{ Vertex::get_binding_descr() };

    VkPipelineVertexInputStateCreateInfo vertex_input_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_descr,
        .vertexAttributeDescriptionCount = out_pipeline.attrib_descriptions.count(),
        .pVertexAttributeDescriptions = out_pipeline.attrib_descriptions.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = primitive_topology
    };

    VkPipelineViewportStateCreateInfo viewport_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissors
    };
    out_pipeline.viewport = viewport;
    out_pipeline.scissors = scissors;

    VkPipelineRasterizationStateCreateInfo rasterization_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        // use VK_POLYGON_MODE_WIREFRAME to only see edges of the polygons
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasSlopeFactor = 1.0f,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisample_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE
    };

    VkPipelineColorBlendAttachmentState color_blending_attachment{
        .blendEnable = enable_color_blend,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo color_blending_state{
        .logicOpEnable = false,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_blending_attachment,
    };

    VkPushConstantRange push_constant_range{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(glm::mat4),
    };

    FixedArray<VkDescriptorSetLayout, 2> layouts = {
        out_pipeline.global_descriptor_layout.handle,
        out_pipeline.object_descriptor_layout.handle  
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = layouts.count(),
        .pSetLayouts = layouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range,
    };

    // TODO: custom allocator
    sf_vk_check(vkCreatePipelineLayout(context.device.logical_device, &pipeline_layout_create_info, nullptr, &out_pipeline.pipeline_layout));

    VkPipelineRenderingCreateInfo rendering_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &context.swapchain.image_format.format
    };

    VkGraphicsPipelineCreateInfo pipeline_create_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_create_info,
        .stageCount = 2,
        .pStages = shader_stages.data(),
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly_create_info,
        .pViewportState = &viewport_create_info,
        .pRasterizationState = &rasterization_create_info,
        .pMultisampleState = &multisample_create_info,
        .pColorBlendState = &color_blending_state,
        .pDynamicState = &dynamic_create_info,
        .layout = out_pipeline.pipeline_layout,
        .renderPass = nullptr,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    // TODO: custom allocator
    sf_vk_check(vkCreateGraphicsPipelines(context.device.logical_device, nullptr, 1, &pipeline_create_info, nullptr, &out_pipeline.pipeline_handle));
    LOG_INFO("Graphics pipeline was successfully created!");

    return true;
}

void VulkanShaderPipeline::bind(const VulkanCommandBuffer& cmd_buffer, u32 curr_frame) {
    vkCmdBindPipeline(cmd_buffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_handle);
    vkCmdBindDescriptorSets(cmd_buffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &global_descriptor_sets[curr_frame], 0, nullptr);
    vkCmdSetViewport(cmd_buffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmd_buffer.handle, 0, 1, &scissors);
}

void VulkanShaderPipeline::update_global_state(const VulkanDevice& device, u32 curr_frame, const glm::mat4& view, const glm::mat4& proj) {
    global_ubo.update(curr_frame, view, proj);
    update_global_descriptors(device);
}

void VulkanShaderPipeline::update_object_state(VulkanContext& context, const GeometryRenderData& render_data) {
    VulkanCommandBuffer& curr_cmd_buffer{ context.curr_frame_graphics_cmd_buffer() };    
    SF_ASSERT_MSG(curr_cmd_buffer.state == VulkanCommandBufferState::RECORDING_BEGIN, "Should be in recording state");

    vkCmdPushConstants(curr_cmd_buffer.handle, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &render_data.model);

    ObjectShaderState& object_state{ object_shader_states[render_data.id] };

    u32 curr_frame{ context.curr_frame };
    VkDescriptorSet& curr_frame_object_descriptor_set = object_state.descriptor_sets[curr_frame];

    FixedArray<VkWriteDescriptorSet, OBJECT_SHADER_DESCRIPTOR_COUNT> descriptor_writes{};
    u32 descriptor_index{0};

    u32 range{sizeof(LocalUniformObject)};
    u64 offset{sizeof(LocalUniformObject) * render_data.id};

    // TODO: get diffuse color from a material
    // static f32 accumulator{0.0f};
    // accumulator += context.frame_delta_time;
    // f32 s{(std::sin(accumulator) + 1.0f) / 2.0f};
    glm::vec4 diffuse_color{ glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)};

    local_ubo.update(offset, diffuse_color);

    if (object_state.descriptor_states[descriptor_index].generations[curr_frame] == INVALID_ID) {
        VkDescriptorBufferInfo buffer_info{
            .buffer = local_ubo.buffer.handle,
            .offset = offset,
            .range = range   
        };

        VkWriteDescriptorSet descriptor_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = curr_frame_object_descriptor_set,
            .dstBinding = descriptor_index,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &buffer_info,
            .pTexelBufferView = nullptr,
        };

        descriptor_writes.append(descriptor_write);

        // update the frame generation (needed only once)
        object_state.descriptor_states[descriptor_index].generations[curr_frame] = 1;
    }

    descriptor_index++;

    // Samplers
    constexpr u32 SAMPLER_COUNT{1};
    FixedArray<VkDescriptorImageInfo, SAMPLER_COUNT> image_infos(SAMPLER_COUNT);

    for (u32 i{0}; i < SAMPLER_COUNT; ++i) {
        const Texture* texture{ render_data.textures[i] };
        u32& descriptor_generation{ object_state.descriptor_states[descriptor_index].generations[curr_frame] };

        if (descriptor_generation != texture->generation || texture->generation == INVALID_ID) {
            image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_infos[i].imageView = texture->image.view;
            image_infos[i].sampler = texture->sampler; 

            VkWriteDescriptorSet descriptor_write{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = curr_frame_object_descriptor_set,
                .dstBinding = descriptor_index,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &image_infos[i],
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr,
            };

            descriptor_writes.append(descriptor_write);

            if (texture->generation != INVALID_ID) {
                descriptor_generation = texture->generation;
            }
            descriptor_index++;
        }
    }

    if (descriptor_writes.count() > 0) {
        vkUpdateDescriptorSets(context.device.logical_device, descriptor_writes.count(), descriptor_writes.data(), 0, nullptr);
    }

    bind_object_descriptor_sets(curr_cmd_buffer, render_data.id, curr_frame);    
}

void VulkanShaderPipeline::bind_object_descriptor_sets(VulkanCommandBuffer& cmd_buffer, u32 object_id, u32 curr_frame) {
    vkCmdBindDescriptorSets(
        cmd_buffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
        1, 1, &object_shader_states[object_id].descriptor_sets[curr_frame],
        0, nullptr
    );
}

Result<u32> VulkanShaderPipeline::acquire_resouces(const VulkanDevice& device) {
    u32 object_id{ object_id_counter };
    ++object_id_counter;

    ObjectShaderState& object_state{ object_shader_states[object_id] };

    for (u32 i{0}; i < OBJECT_SHADER_DESCRIPTOR_COUNT; ++i) {
        for (u32 j{0}; j < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++j) {
            object_state.descriptor_states[i].generations[j] = INVALID_ID;
        }
    }

    FixedArray<VkDescriptorSetLayout, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT> layouts(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    layouts.fill(object_descriptor_layout.handle);

    object_descriptor_pool.allocate_sets(device, object_state.descriptor_sets.count(), layouts.data(), object_state.descriptor_sets.data());
    return {object_id};
}

void VulkanShaderPipeline::release_resouces(const VulkanDevice& device, u32 object_id) {
    ObjectShaderState& object_state{ object_shader_states[object_id] };     

    VkResult result{ vkFreeDescriptorSets(device.logical_device, object_descriptor_pool.handle, object_state.descriptor_sets.count(), object_state.descriptor_sets.data()) };
    if (result != VK_SUCCESS) {
        LOG_ERROR("Error freeing object descriptor sets");
    }

    for (u32 i{0}; i < OBJECT_SHADER_DESCRIPTOR_COUNT; ++i) {
        for (u32 j{0}; j < 3; ++j) {
            object_state.descriptor_states[i].generations[j] = INVALID_ID;
        }
    }
}

void VulkanShaderPipeline::create_attribute_descriptions(const FixedArray<VkVertexInputAttributeDescription, VulkanShaderPipeline::MAX_ATTRIB_COUNT>& input_descriptions) {
    if (attrib_descriptions.count() != input_descriptions.count()) {
        attrib_descriptions.resize(input_descriptions.count());
    }

    for (u32 i{0}; i < input_descriptions.count(); ++i) {
        attrib_descriptions[i] = input_descriptions[i];
    }
}

void VulkanShaderPipeline::create_global_descriptors(const VulkanDevice& device) {
    FixedArray<VkDescriptorPoolSize, 1> pool_sizes{{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = VulkanSwapchain::MAX_FRAMES_IN_FLIGHT }};
    VulkanDescriptorPool::create(device, {pool_sizes.data(), pool_sizes.count()}, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT, global_descriptor_pool);

    FixedArray<VkDescriptorType, 1> types = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
    FixedArray<VkDescriptorSetLayoutBinding, 1> bindings(1);
    VulkanDescriptorSetLayout::create_bindings(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, {types.data(), types.count()}, {bindings.data(), bindings.count()});
    VulkanDescriptorSetLayout::create(device, {bindings.data(), bindings.count()}, global_descriptor_layout);
    FixedArray<VkDescriptorSetLayout, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT> layouts(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    layouts.fill(global_descriptor_layout.handle);

    global_descriptor_pool.allocate_sets(device, global_descriptor_sets.count(), reinterpret_cast<VkDescriptorSetLayout*>(layouts.data()), global_descriptor_sets.data());

    update_global_descriptors(device);
}

void VulkanShaderPipeline::create_local_descriptors(const VulkanDevice& device) {
    constexpr u32 LOCAL_SAMPLER_COUNT{1};
    
    FixedArray<VkDescriptorPoolSize, OBJECT_SHADER_DESCRIPTOR_COUNT> pool_sizes{
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = MAX_OBJECT_COUNT },
        { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = LOCAL_SAMPLER_COUNT * MAX_OBJECT_COUNT  }
    };
    VulkanDescriptorPool::create(device, {pool_sizes.data(), pool_sizes.count()}, MAX_OBJECT_COUNT, object_descriptor_pool);

    FixedArray<VkDescriptorType, OBJECT_SHADER_DESCRIPTOR_COUNT> descriptor_types = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER };
    
    FixedArray<VkDescriptorSetLayoutBinding, OBJECT_SHADER_DESCRIPTOR_COUNT> bindings(OBJECT_SHADER_DESCRIPTOR_COUNT);

    VulkanDescriptorSetLayout::create_bindings(VK_SHADER_STAGE_FRAGMENT_BIT, {descriptor_types.data(), descriptor_types.count()}, {bindings.data(), bindings.count()});
    VulkanDescriptorSetLayout::create(device, {bindings.data(), bindings.count()}, object_descriptor_layout);
}

void VulkanShaderPipeline::update_global_descriptors(const VulkanDevice& device) {
    for (u32 i{0}; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorBufferInfo buffer_info{
            .buffer = global_ubo.buffers[i].handle,
            .offset = 0,
            .range = sizeof(GlobalUniformObject)
        };

        VkWriteDescriptorSet descriptor_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = global_descriptor_sets[i],
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

void VulkanShaderPipeline::destroy(const VulkanDevice& device) {
    global_descriptor_layout.destroy(device);
    object_descriptor_layout.destroy(device);

    // TODO: custom allocator
    global_ubo.destroy(device);
    local_ubo.destroy(device);
    vkDestroyPipeline(device.logical_device, pipeline_handle, nullptr);
    global_descriptor_pool.destroy(device);
    object_descriptor_pool.destroy(device);
    vkDestroyShaderModule(device.logical_device, shader_handle, nullptr);
}

void VulkanDescriptorPool::create(const VulkanDevice& device, std::span<VkDescriptorPoolSize> pool_sizes, u32 max_sets, VulkanDescriptorPool& out_pool) {
    VkDescriptorPoolCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = max_sets,
        .poolSizeCount = static_cast<u32>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };

    // TODO: custom allocator
    sf_vk_check(vkCreateDescriptorPool(device.logical_device, &create_info, nullptr, &out_pool.handle)); 
}

// layouts and sets array should be equal
void VulkanDescriptorPool::allocate_sets(const VulkanDevice& device, u32 count, VkDescriptorSetLayout* layouts, VkDescriptorSet* sets) const {
    VkDescriptorSetAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = handle,
        .descriptorSetCount = count,
        .pSetLayouts = layouts
    };

    sf_vk_check(vkAllocateDescriptorSets(device.logical_device, &alloc_info, sets));
}

void VulkanDescriptorPool::destroy(const VulkanDevice& device) {
    if (handle) {
        // TODO: custom allocator
        vkDestroyDescriptorPool(device.logical_device, handle, nullptr);
        handle = nullptr;
    }
}

void VulkanDescriptorSetLayout::create(const VulkanDevice& device, std::span<VkDescriptorSetLayoutBinding> bindings, VulkanDescriptorSetLayout& out_layout) {
    VkDescriptorSetLayoutCreateInfo descriptor_layout_create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<u32>(bindings.size()),
        .pBindings = bindings.data(),
    };
    // TODO: custom allocator
    sf_vk_check(vkCreateDescriptorSetLayout(device.logical_device, &descriptor_layout_create_info, nullptr, &out_layout.handle));
}

void VulkanDescriptorSetLayout::create_bindings(VkShaderStageFlags stage_flags, std::span<VkDescriptorType> descriptor_types, std::span<VkDescriptorSetLayoutBinding> out_bindings) {
    SF_ASSERT_MSG(descriptor_types.size() == out_bindings.size(), "Should be equal");
    
    for (u32 i{0}; i < out_bindings.size(); ++i) {
        out_bindings[i].binding = i,
        out_bindings[i].descriptorType = descriptor_types[i],
        out_bindings[i].descriptorCount = 1,
        out_bindings[i].stageFlags = stage_flags,
        out_bindings[i].pImmutableSamplers = nullptr;
    }
}

void VulkanDescriptorSetLayout::destroy(const VulkanDevice& device) {
    if (handle) {
        // TODO: custom allocator
        vkDestroyDescriptorSetLayout(device.logical_device, handle, nullptr);
        handle = nullptr;
    }
}

Result<VkShaderModule> create_shader_module(const VulkanDevice& device, fs::path&& shader_file_path) {
    Result<DynamicArray<char>> shader_file_contents = read_file(shader_file_path);

    if (shader_file_contents.is_err()) {
        return {ResultError::VALUE};
    }

    DynamicArray<char>& shader_file_contents_unwrapped{ shader_file_contents.unwrap() };

    VkShaderModuleCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shader_file_contents_unwrapped.count(),
        .pCode = reinterpret_cast<u32*>(shader_file_contents_unwrapped.data()),
    };

    VkShaderModule shader_handle;

    // TODO: custom allocator
    sf_vk_check(vkCreateShaderModule(device.logical_device, &create_info, nullptr, &shader_handle));
    return {shader_handle};
}

} // sf
