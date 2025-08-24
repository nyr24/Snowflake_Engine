#include "sf_vulkan/pipeline.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_containers/result.hpp"
#include "sf_core/logger.hpp"
#include "sf_vulkan/shaders.hpp"
#include "sf_vulkan/types.hpp"
#include <filesystem>
#include <vulkan/vulkan_core.h>

namespace fs = std::filesystem;

namespace sf {

 bool pipeline_create(VulkanContext& context) {
    #ifdef SF_DEBUG
    fs::path shader_path = fs::current_path() / "build/debug/engine/shaders/shader.spv";
    #else
    fs::path shader_path = fs::current_path() / "/build/release/engine/shaders/shader.spv";
    #endif

    Result<VkShaderModule> maybe_shader_module = create_shader_module(context, shader_path);
    if (maybe_shader_module.is_err()) {
        return false;
    }

    VkShaderModule module = maybe_shader_module.unwrap();

    FixedArray<VkPipelineShaderStageCreateInfo, 2> shader_stages{
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = module,
            .pName = "vertMain",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = module,
            .pName = "fragMain"
        }
    };

    FixedArray<VkDynamicState, 2> dynamic_state{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = dynamic_state.count(),
        .pDynamicStates = dynamic_state.data()
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
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
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisample_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineColorBlendAttachmentState color_blending_attachment{
        .blendEnable = true,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD
    };

    VkPipelineColorBlendStateCreateInfo color_blending_state{
        .logicOpEnable = false,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_blending_attachment,
    };

    // TODO: modify, after we have uniform variables to pass for shaders
    VkPipelineLayoutCreateInfo layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pushConstantRangeCount = 0,
    };

    // TODO: custom allocator
    sf_vk_check(vkCreatePipelineLayout(context.device.logical_device, &layout_create_info, nullptr, &context.pipeline.layout));

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
        .layout = context.pipeline.layout,
        .renderPass = nullptr,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    // TODO: custom allocator
    sf_vk_check(vkCreateGraphicsPipelines(context.device.logical_device, nullptr, 1, &pipeline_create_info, nullptr, &context.pipeline.handle));
    LOG_INFO("Graphics pipeline was successfully created!");

    return true;
}

} // sf
