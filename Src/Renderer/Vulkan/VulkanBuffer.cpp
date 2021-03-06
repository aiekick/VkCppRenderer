#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include "VulkanCommandBuffer.h"

VulkanBuffer::VulkanBuffer(void* Data, vk::DeviceSize DataSize, EBufferType BufferType)
{
	vk::BufferUsageFlagBits BufferTypeBit;
	switch (BufferType)
	{
		case EBufferType::VertexBuffer:
		BufferTypeBit = vk::BufferUsageFlagBits::eVertexBuffer;
		break;
		case EBufferType::IndexBuffer:
		BufferTypeBit = vk::BufferUsageFlagBits::eIndexBuffer;
		break;
	} 

	vk::Device Device = VulkanContext::Get()->GetDevice();

	//Staging Buffer
	vk::UniqueBuffer StagingBuffer;
	vk::UniqueDeviceMemory StagingMemory;
	VulkanBufferUtils::CreateBuffer(DataSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, StagingBuffer, StagingMemory);

	void* MappedMemory = Device.mapMemory(StagingMemory.get(), 0, DataSize);
	memcpy(MappedMemory, Data, (size_t) DataSize);
	Device.unmapMemory(StagingMemory.get());

	VulkanBufferUtils::CreateBuffer(DataSize, vk::BufferUsageFlagBits::eTransferDst | BufferTypeBit, vk::MemoryPropertyFlagBits::eDeviceLocal, Buffer, Memory);

	VulkanBufferUtils::CopyBuffer(StagingBuffer, Buffer, DataSize);
}

void VulkanBufferUtils::CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::UniqueBuffer& OutBuffer, vk::UniqueDeviceMemory& OutMemory)
{
	vk::Device Device = VulkanContext::Get()->GetDevice();
	
	vk::BufferCreateInfo CreateInfo;
	CreateInfo.size = size;
	CreateInfo.usage = usage;
	CreateInfo.sharingMode = vk::SharingMode::eExclusive;

	OutBuffer = Device.createBufferUnique(CreateInfo);

	vk::MemoryRequirements MemoryRequirements = Device.getBufferMemoryRequirements(OutBuffer.get());
	
	vk::MemoryAllocateInfo AllocInfo;
	AllocInfo.allocationSize = MemoryRequirements.size;
	AllocInfo.memoryTypeIndex = VulkanContext::FindMemoryType(MemoryRequirements.memoryTypeBits, properties);

	OutMemory = Device.allocateMemoryUnique(AllocInfo);

	Device.bindBufferMemory(OutBuffer.get(), OutMemory.get(), 0);
}

void VulkanBufferUtils::CopyBuffer(vk::UniqueBuffer& SourceBuffer, vk::UniqueBuffer& DestinationBuffer, vk::DeviceSize CopySize)
{
	VulkanCommandBuffer CommandBuffer;
	CommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	vk::BufferCopy CopyRegion;
	CopyRegion.size = CopySize;
	CommandBuffer.GetHandle().copyBuffer(SourceBuffer.get(), DestinationBuffer.get(), 1, &CopyRegion);
	CommandBuffer.End();
	CommandBuffer.SubmitWaitIdle();
}