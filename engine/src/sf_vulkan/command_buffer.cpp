#include "sf_vulkan/command_buffer.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_containers/optional.hpp"
#include "sf_core/logger.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/image.hpp"
#include "sf_vulkan/pipeline.hpp"
#include "sf_vulkan/synch.hpp"
#include "sf_vulkan/renderer.hpp"
#include "sf_vulkan/swapchain.hpp"
#include <span>
#include <vulkan/vulkan_core.h>

namespace sf {

void VulkanCommandPool::create(VulkanContext& context, VulkanCommandPoolType type, u32 queue_family_index, VkCommandPoolCreateFlagBits create_flags, VulkanCommandPool& out_pool) {
    VkCommandPoolCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = create_flags,
        .queueFamilyIndex = queue_family_index
    };

    // TODO: custom allocator
    sf_vk_check(vkCreateCommandPool(context.device.logical_device, &create_info, nullptr, &out_pool.handle));
    out_pool.type = type;
}

void VulkanCommandPool::destroy(VulkanContext& context) {
    if (handle) {
        // TODO: custom allocator
        vkDestroyCommandPool(context.device.logical_device, handle, nullptr);
        handle = nullptr;
    }
}

VulkanCommandBuffer::VulkanCommandBuffer()
    : state{ VulkanCommandBufferState::NOT_ALLOCATED }
{
}

void VulkanCommandBuffer::allocate(const VulkanContext& context, VkCommandPool command_pool, std::span<VulkanCommandBuffer> out_buffers, bool is_primary) {
    FixedArray<VkCommandBuffer, MAX_BUFFER_ALLOC_COUNT> handles(out_buffers.size());

    VkCommandBufferAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = is_primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        .commandBufferCount = static_cast<u32>(out_buffers.size())
    };

    sf_vk_check(vkAllocateCommandBuffers(context.device.logical_device, &alloc_info, handles.data()));

    for (u32 i{0}; i < out_buffers.size(); ++i) {
        out_buffers[i].handle = handles[i];
        out_buffers[i].state = VulkanCommandBufferState::READY;
    }
}

void VulkanCommandBuffer::begin_recording(VkCommandBufferUsageFlags begin_flags) {
    if (state != VulkanCommandBufferState::READY) {
        LOG_ERROR("Trying to begin command_buffer which is not in READY state");
        return;
    }

    VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = begin_flags
    };

    sf_vk_check(vkBeginCommandBuffer(handle, &begin_info));
    state = VulkanCommandBufferState::RECORDING_BEGIN;
}

void VulkanCommandBuffer::record_draw_commands(VulkanContext& context, VulkanShaderPipeline& pipeline, u32 image_index) {
    VulkanImage::transition_layout(
        context.swapchain.images[context.image_index],
        *this,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        0,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    );

    VkClearColorValue clear_color{
        .float32{ 0.0f, 0.0f, 0.0f, 1.0f }
    };

    VkRenderingAttachmentInfo attachment_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = context.swapchain.views[context.image_index],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue{ clear_color },
    };

    VkRenderingInfo rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea{ .offset = { 0, 0 }, .extent{ context.framebuffer_width, context.framebuffer_height } },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment_info,
    };

    vkCmdBeginRendering(handle, &rendering_info);
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(handle, 0, 1, &context.vertex_index_buffer.main_buffer.handle, offsets);
    vkCmdBindIndexBuffer(handle, context.vertex_index_buffer.main_buffer.handle, context.vertex_index_buffer.indeces_offset, VK_INDEX_TYPE_UINT16);
    vkCmdPushConstants(handle, pipeline.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VulkanPushConstantBlock), &pipeline.push_constant_blocks[context.curr_frame]);

    VkViewport viewport{ context.get_viewport() };
    VkRect2D scissors{ context.get_scissors() };

    vkCmdSetViewport(handle, 0, 1, &viewport);
    vkCmdSetScissor(handle, 0, 1, &scissors);
    vkCmdDrawIndexed(handle, context.vertex_index_buffer.indeces_count, 1, 0, 0, 0);
    vkCmdEndRendering(handle);

    VulkanImage::transition_layout(
        context.swapchain.images[context.image_index],
        *this,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        0,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    );
}

void VulkanCommandBuffer::copy_data_between_buffers(VkBuffer src, VkBuffer dst, VkBufferCopy& copy_region) {
    vkCmdCopyBuffer(handle, src, dst, 1, &copy_region);
}

void VulkanCommandBuffer::copy_data_from_buffer_to_image(VkBuffer src_buffer, VulkanImage& dst_image, VkImageLayout dst_image_layout) {
    VkBufferImageCopy copy_region{
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageExtent = {
            .width = dst_image.width,
            .height = dst_image.height,
            .depth = 1
        }
    };
    
    vkCmdCopyBufferToImage(handle, src_buffer, dst_image.handle, dst_image_layout, 1, &copy_region);
}

void VulkanCommandBuffer::end_recording(const VulkanContext& context) {
    if (state != VulkanCommandBufferState::RECORDING_BEGIN) {
        LOG_WARN("Trying to end command_buffer which is not in RECORDING_END state");
        return;
    }

    sf_vk_check(vkEndCommandBuffer(handle));
    state = VulkanCommandBufferState::RECORDING_END;
}

void VulkanCommandBuffer::submit(const VulkanContext& context, VkQueue queue, VkSubmitInfo& submit_info, Option<VulkanFence> maybe_fence) {
    if (state != VulkanCommandBufferState::RECORDING_END) {
        LOG_WARN("Trying to submit command_buffer which is not in RECORDING_END state");
        return;
    }

    if (maybe_fence.is_none()) {
        sf_vk_check(vkQueueSubmit(queue, 1, &submit_info, nullptr));
        state = VulkanCommandBufferState::SUBMITTED;
        return;
    }

    VulkanFence& fence = maybe_fence.unwrap();
    
    if (fence.is_signaled) {
        LOG_WARN("Fence can't be in signaled state when submitting a queue, resetting...");
        fence.reset(context);
    }

    sf_vk_check(vkQueueSubmit(queue, 1, &submit_info, fence.handle));
    state = VulkanCommandBufferState::SUBMITTED;
}

void VulkanCommandBuffer::reset(VulkanContext& context) {
    sf_vk_check(vkResetCommandBuffer(handle, 0));
    state = VulkanCommandBufferState::READY;
}

void VulkanCommandBuffer::allocate_and_begin_single_use(const VulkanContext& context, VkCommandPool command_pool, VulkanCommandBuffer& out_buffer) {
    VulkanCommandBuffer::allocate(context, command_pool, {&out_buffer, 1}, true);
    out_buffer.begin_recording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
}

void VulkanCommandBuffer::end_single_use(const VulkanContext& context, VkCommandPool command_pool) {
    this->end_recording(context);

    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &handle
    };
    
    this->submit(context, context.device.graphics_queue, submit_info, {None::VALUE});
    sf_vk_check(vkQueueWaitIdle(context.device.graphics_queue));

    this->free(context, command_pool);
}

void VulkanCommandBuffer::free(const VulkanContext &contex, VkCommandPool command_pool) {
    if (handle) {
        vkFreeCommandBuffers(contex.device.logical_device, command_pool, 1, &handle);
        handle = nullptr;
    }
    state = VulkanCommandBufferState::NOT_ALLOCATED;
}

} // sf
