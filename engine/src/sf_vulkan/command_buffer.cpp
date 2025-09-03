#include "sf_vulkan/command_buffer.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_core/logger.hpp"
#include "sf_vulkan/synch.hpp"
#include "sf_vulkan/types.hpp"
#include <vulkan/vulkan_core.h>
#include "sf_vulkan/swapchain.hpp"

namespace sf {

void VulkanCommandPool::create(VulkanContext& context, VulkanCommandPoolType type, u32 queue_family_index, VulkanCommandPool& out_pool) {
    VkCommandPoolCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
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

void VulkanCommandBuffer::allocate(VulkanContext& context, VkCommandPool command_pool, FixedArray<VulkanCommandBuffer, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>& out_buffers, bool is_primary) {
    FixedArray<VkCommandBuffer, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT> handles(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = is_primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        .commandBufferCount = VulkanSwapchain::MAX_FRAMES_IN_FLIGHT
    };

    sf_vk_check(vkAllocateCommandBuffers(context.device.logical_device, &alloc_info, handles.data()));

    for (u32 i{0}; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        out_buffers[i].handle = handles[i];
        out_buffers[i].state = VulkanCommandBufferState::READY;
    }
}

void VulkanCommandBuffer::begin_recording(VulkanContext& context, VkCommandBufferUsageFlags begin_flags, u32 image_index) {
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

    begin_rendering(context, context.image_index);
}

void VulkanCommandBuffer::begin_rendering(VulkanContext& context, u32 image_index) {
    transition_image_layout(
        context,
        context.image_index,
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
    vkCmdBindPipeline(handle, VK_PIPELINE_BIND_POINT_GRAPHICS, context.pipeline.handle);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(handle, 0, 1, &context.vertex_buffer.handle, offsets);

    VkViewport viewport{ context.get_viewport() };
    VkRect2D scissors{ context.get_scissors() };

    vkCmdSetViewport(handle, 0, 1, &viewport);
    vkCmdSetScissor(handle, 0, 1, &scissors);
    vkCmdDraw(handle, context.vertex_buffer.geometry.count(), 1, 0, 0);
    vkCmdEndRendering(handle);
}

void VulkanCommandBuffer::end_recording(VulkanContext& context) {
    if (state != VulkanCommandBufferState::RECORDING_BEGIN) {
        LOG_WARN("Trying to end command_buffer which is not in RECORDING_END state");
        return;
    }

    transition_image_layout(
        context,
        context.image_index,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        0,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    );

    sf_vk_check(vkEndCommandBuffer(handle));
    state = VulkanCommandBufferState::RECORDING_END;
}

void VulkanCommandBuffer::submit(VulkanContext& context, VkQueue queue, VkSubmitInfo& submit_info, VulkanFence& fence) {
    if (state != VulkanCommandBufferState::RECORDING_END) {
        LOG_WARN("Trying to submit command_buffer which is not in RECORDING_END state");
        return;
    }
    if (fence.is_signaled) {
        LOG_ERROR("Fence can't be in signaled state when submitting a queue");
        return;
    }

    sf_vk_check(vkQueueSubmit(queue, 1, &submit_info, fence.handle));
    state = VulkanCommandBufferState::SUBMITTED;
}

void VulkanCommandBuffer::reset(VulkanContext& context) {
    sf_vk_check(vkResetCommandBuffer(handle, 0));
    state = VulkanCommandBufferState::READY;
}

void VulkanCommandBuffer::free(VulkanContext &contex, VkCommandPool command_pool) {
    if (handle) {
        vkFreeCommandBuffers(contex.device.logical_device, command_pool, 1, &handle);
        handle = nullptr;
    }
    state = VulkanCommandBufferState::NOT_ALLOCATED;
}

void VulkanCommandBuffer::transition_image_layout(
    VulkanContext& context,
    uint32_t image_index,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkPipelineStageFlags2 src_stage_mask,
    VkAccessFlags2 src_access_mask,
    VkPipelineStageFlags2 dst_stage_mask,
    VkAccessFlags2 dst_access_mask
) {
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = src_stage_mask,
        .srcAccessMask = src_access_mask,
        .dstStageMask = dst_stage_mask,
        .dstAccessMask = dst_access_mask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = context.swapchain.images[image_index],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkDependencyInfo dep_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = 0,
        .imageMemoryBarrierCount =  1,
        .pImageMemoryBarriers = &barrier
    };

    vkCmdPipelineBarrier2(handle, &dep_info);
}

} // sf
