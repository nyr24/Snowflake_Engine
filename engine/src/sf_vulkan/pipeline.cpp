#include "sf_vulkan/pipeline.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_containers/result.hpp"
#include "sf_core/io.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/renderer.hpp"
#include "sf_vulkan/swapchain.hpp"
#include <vulkan/vulkan_core.h>
#include <filesystem>
#include <span>

namespace fs = std::filesystem;

namespace sf {

VulkanShaderPipeline::VulkanShaderPipeline()
{
    push_constant_blocks.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    attrib_descriptions.resize(VulkanShaderPipeline::MAX_ATTRIB_COUNT);
    descriptor_layouts.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    descriptor_sets.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
}
    
bool VulkanShaderPipeline::create(
    const VulkanContext& context,
    const char* shader_file_name,
    VkPrimitiveTopology primitive_topology,
    bool enable_color_blend,
    std::span<VkVertexInputAttributeDescription> attrib_descriptions,
    VulkanShaderPipeline& out_pipeline
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

    if (out_pipeline.attrib_descriptions.count() != attrib_descriptions.size()) {
        out_pipeline.attrib_descriptions.resize(attrib_descriptions.size());
    }
    sf_mem_copy(out_pipeline.attrib_descriptions.data(), attrib_descriptions.data(), sizeof(VkVertexInputAttributeDescription) * attrib_descriptions.size());

    FixedArray<VkDynamicState, 2> dynamic_state{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = dynamic_state.count(),
        .pDynamicStates = dynamic_state.data()
    };

    auto binding_descr{ Vertex::get_binding_descr() };

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

    VkViewport viewport{ context.get_viewport() };
    VkRect2D scissors{ context.get_scissors() };

    VkPipelineViewportStateCreateInfo viewport_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissors
    };

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
        .size = sizeof(VulkanPushConstantBlock),
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = out_pipeline.descriptor_layouts.count(),
        .pSetLayouts = reinterpret_cast<VkDescriptorSetLayout*>(out_pipeline.descriptor_layouts.data()),
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
    vkCmdBindDescriptorSets(cmd_buffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_sets[curr_frame], 0, nullptr);
}

void VulkanShaderPipeline::update_global_state(u32 curr_frame, const glm::mat4& view, const glm::mat4& proj) {
    global_ubo.update(curr_frame, view, proj);
}

void VulkanShaderPipeline::destroy(const VulkanDevice& device) {
    for (u32 i{0}; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        descriptor_layouts[i].destroy(device);
    }

    // TODO: custom allocator
    global_ubo.destroy(device);
    vkDestroyPipeline(device.logical_device, pipeline_handle, nullptr);
    descriptor_pool.destroy(device);
}

void VulkanDescriptorPool::create(const VulkanDevice& device, u32 descriptor_count, VkDescriptorType type, u32 max_sets, VulkanDescriptorPool& out_pool) {
    VkDescriptorPoolSize pool_size{
        .type = type,
        .descriptorCount = descriptor_count
    };

    VkDescriptorPoolCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = max_sets,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };

    // TODO: custom allocator
    sf_vk_check(vkCreateDescriptorPool(device.logical_device, &create_info, nullptr, &out_pool.handle)); 
}

void VulkanDescriptorPool::allocate_sets(const VulkanDevice& device, u32 count, VkDescriptorSetLayout* layouts, VkDescriptorSet* sets) {
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

void VulkanDescriptorSetLayout::create_bindings(u32 index, VkDescriptorType descriptor_type, u32 descriptor_count, VkShaderStageFlags stage_flags, std::span<VkDescriptorSetLayoutBinding> out_bindings) {
    for (u32 i{0}; i < out_bindings.size(); ++i) {
        out_bindings[i].binding = index,
        out_bindings[i].descriptorType = descriptor_type,
        out_bindings[i].descriptorCount = descriptor_count,
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
