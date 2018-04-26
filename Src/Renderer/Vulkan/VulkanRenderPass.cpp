#include "VulkanRenderPass.h"

#include "VulkanContext.h"
#include "VulkanSwapchain.h"
#include <functional>

VulkanRenderPass::VulkanRenderPass() : CommandBuffer(true /* bSecondary */)
{

}

void VulkanRenderPass::BuildRenderPass(std::vector<VulkanRenderTarget*> RenderTargets, uint32_t Width, uint32_t Height, uint32_t BackbufferCount)
{
	//Note: Attachment Descriptions = Prototype, Framebuffer = Actual references
	std::vector<vk::AttachmentDescription> AttachmentDescriptions;
	std::vector<vk::AttachmentReference> ColorAttachmentReferences;
	vk::AttachmentReference DepthAttachmentReference;

	std::vector<std::vector<vk::ImageView>> FramebufferImageViewsPerBackbuffer(BackbufferCount);

	//[1] Iterate over attachments and create descriptions and references
	for (size_t i = 0; i < RenderTargets.size(); ++i)
	{
		auto& RenderTarget = RenderTargets[i];
		assert(RenderTarget->ImageViews.size() > 0);

		vk::AttachmentDescription AttachmentDescription;
		AttachmentDescription.format = RenderTarget->Format;
		AttachmentDescription.samples = vk::SampleCountFlagBits::e1; //TODO: allow multisampling
		AttachmentDescription.loadOp = RenderTarget->LoadOp;
		AttachmentDescription.storeOp = RenderTarget->StoreOp;
		AttachmentDescription.initialLayout = RenderTarget->InitialLayout;
		AttachmentDescription.finalLayout = RenderTarget->FinalLayout;

		AttachmentDescriptions.push_back(std::move(AttachmentDescription));

		//TODO: Enforce only one depth attachment (per subpass, or renderpass at this point) here
		if (RenderTarget->bDepthTarget)
		{
			DepthAttachmentReference.attachment = i;
			DepthAttachmentReference.layout = RenderTarget->UsageLayout;
		}
		else
		{
			vk::AttachmentReference AttachmentReference;
			AttachmentReference.attachment = i;
			AttachmentReference.layout = RenderTarget->UsageLayout;
			ColorAttachmentReferences.push_back(std::move(AttachmentReference));
		}

		//Add each image view of the render target (1 per backbuffer) to its corresponding framebuffer image view array
		for (size_t i = 0; i < RenderTarget->ImageViews.size(); ++i)
		{
			assert(i < FramebufferImageViewsPerBackbuffer.size());
			FramebufferImageViewsPerBackbuffer[i].push_back(*RenderTarget->ImageViews[i]);
		}
	}

	//Our subpass
	vk::SubpassDescription Subpass;
	Subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	Subpass.colorAttachmentCount = ColorAttachmentReferences.size();
	Subpass.pColorAttachments = ColorAttachmentReferences.data();
	Subpass.pDepthStencilAttachment = &DepthAttachmentReference;


	//The Renderpass itself
	vk::RenderPassCreateInfo CreateInfo;
	CreateInfo.attachmentCount = static_cast<uint32_t>(AttachmentDescriptions.size());
	CreateInfo.pAttachments = AttachmentDescriptions.data();
	CreateInfo.subpassCount = 1;
	CreateInfo.pSubpasses = &Subpass;

/*
//TODO: Generic way to handle subpass dependencies
	//Subpass Dependency
	vk::SubpassDependency Dependency;
	Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	Dependency.dstSubpass = 0; //Currently only have the one subpass
	Dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	Dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	Dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

	CreateInfo.dependencyCount = 1;
	CreateInfo.pDependencies = &Dependency;
*/	

	RenderPass = VulkanContext::Get()->GetDevice().createRenderPassUnique(CreateInfo);
	Extent.width = Width;
	Extent.height = Height;

	Framebuffers.clear();

	for (std::vector<vk::ImageView>& FrameBufferImageViews : FramebufferImageViewsPerBackbuffer)
	{
		vk::FramebufferCreateInfo FramebufferCreateInfo;
		FramebufferCreateInfo.renderPass = GetHandle();
		FramebufferCreateInfo.attachmentCount = static_cast<uint32_t>(FrameBufferImageViews.size());
		FramebufferCreateInfo.pAttachments = FrameBufferImageViews.data();
		FramebufferCreateInfo.width = Width;
		FramebufferCreateInfo.height = Height;
		FramebufferCreateInfo.layers = 1;

		Framebuffers.push_back(VulkanContext::Get()->GetDevice().createFramebufferUnique(FramebufferCreateInfo));
	}
}

#include <iostream>

void VulkanRenderPass::BuildCommandBuffer(std::vector<std::pair<VulkanRenderItem*, VulkanGraphicsPipeline*>> ItemsToRender)
{
	// Sort Input array of pairs by pipeline pointer address
	//TODO: Sort by pipeline
	std::sort(std::begin(ItemsToRender), std::end(ItemsToRender));

	vk::CommandBufferUsageFlags UsageFlags = vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse; 
	CommandBuffer.BeginSecondary(UsageFlags, GetHandle());

	VulkanGraphicsPipeline* CurrentPipeline = nullptr;
	for (auto& ItemAndPipeline : ItemsToRender)
	{
		VulkanRenderItem* RenderItem = ItemAndPipeline.first;
		VulkanGraphicsPipeline* Pipeline = ItemAndPipeline.second;

		if (RenderItem == nullptr || Pipeline == nullptr) continue;

		if (Pipeline != CurrentPipeline)
		{
			CurrentPipeline = Pipeline;
			CommandBuffer().bindPipeline(vk::PipelineBindPoint::eGraphics, Pipeline->GetHandle());
		}

		/*TODO: Need to create a descriptor set for this render item and pipeline type */
		RenderItem->AddCommands(CommandBuffer, Pipeline);
	}	 

	CommandBuffer.End();
}

void VulkanRenderPass::RecordCommands(VulkanCommandBuffer& CommandBuffer, int FrameIndex) 
{
	//TODO: Clear Values shouldn't be hard coded
	vk::ClearColorValue ClearColor(std::array<float, 4>{0.39f, 0.58f, 0.93f, 1.0f});
	vk::ClearDepthStencilValue ClearDepth(1.0f, 0);
	std::vector<vk::ClearValue> ClearValues = {ClearColor, ClearDepth};

	vk::RenderPassBeginInfo BeginInfo;
	BeginInfo.renderPass = GetHandle();
	BeginInfo.framebuffer = GetFramebuffers()[FrameIndex].get();
	BeginInfo.renderArea.offset = {0,0};
	BeginInfo.renderArea.extent = Extent;	
	BeginInfo.clearValueCount = static_cast<uint32_t>(ClearValues.size());
	BeginInfo.pClearValues = ClearValues.data();

	CommandBuffer().beginRenderPass(BeginInfo, vk::SubpassContents::eSecondaryCommandBuffers);	
	CommandBuffer().executeCommands(1, &GetCommandBuffer().GetHandle());
	CommandBuffer().endRenderPass();
}