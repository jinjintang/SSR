/*
* Vulkan Example - Screen space ambient occlusion example
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <random>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "vulkanexamplebase.h"
#include "VulkanTexture.hpp"
#include "VulkanModel.hpp"
#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h"
#include"HaltonValue.h"
#define ENABLE_VALIDATION false

#define ssr_KERNEL_SIZE 32
#define ssr_RADIUS 0.5f
class VulkanExample : public VulkanExampleBase
{
public:
	HaltonValue h;
	uint32_t numMips;
	vks::Texture2D texture;
	struct {
		vks::Texture2D ssrNoise;
		vks::TextureCubeMap envmap;

	} textures;
	int current = 0;
	// Vertex layout for the models
	vks::VertexLayout vertexLayout = vks::VertexLayout({
		vks::VERTEX_COMPONENT_POSITION,
		vks::VERTEX_COMPONENT_UV,
		vks::VERTEX_COMPONENT_COLOR,
		vks::VERTEX_COMPONENT_NORMAL,
		});

	vks::VertexLayout vertexLayout_sphere = vks::VertexLayout({
	vks::VERTEX_COMPONENT_POSITION,
	vks::VERTEX_COMPONENT_NORMAL,
	vks::VERTEX_COMPONENT_UV,
		});

	struct {
		vks::Model example;
		vks::Model quad;
		vks::Model plane;
	} models;
	
	std::vector<glm::vec4>colors;
	
	struct UBOraycastParams {
		glm::mat4 view;
		glm::mat4 perspective;
		glm::vec4 randomVector;
		glm::vec4 time;
		glm::vec3 _WorldSpaceCameraPos;
		int _NumSteps;
		int num = 0;

	} uboraycastParams;

	struct UBOssrParams {
		glm::mat4 view;
		glm::mat4 perspective;
		glm::vec4 _WorldSpaceCameraPos;
		glm::ivec4 _ScreenSize;
		int _MaxMipMap;
	} ubossrParams;

	struct UBO {
		
		glm::mat4 projection;
		glm::mat4 model;
		glm::mat4 view;
		
	} uboParams;

	struct UBOtemprol {

		glm::vec4 _SinTime;
		glm::vec4 _FeedbackMin_Max_Mscale;
		int num = 0;
	} uboTemprolParams;

	struct {
		
		VkPipeline skybox;
		VkPipeline model;
		VkPipeline ssr;
		VkPipeline raycast;
		VkPipeline temprol;
		VkPipeline show;

	} pipelines;

	struct {
		
		VkPipelineLayout gBuffer;
		VkPipelineLayout ssr;
		VkPipelineLayout raycast;
		VkPipelineLayout temprol;
		VkPipelineLayout show;

	} pipelineLayouts;

	struct {
		const uint32_t count = 6;
	
		VkDescriptorSet gbuffer_plane;
		VkDescriptorSet gbuffer_dragon;

		VkDescriptorSet ssr;
		VkDescriptorSet raycast;
		VkDescriptorSet temprol;
		VkDescriptorSet show;

	} descriptorSets;

	struct {
		VkDescriptorSetLayout gBuffer;
		VkDescriptorSetLayout ssr;
		VkDescriptorSetLayout raycast;
		VkDescriptorSetLayout temprol;
		VkDescriptorSetLayout show;

	} descriptorSetLayouts;

	struct {
		vks::Buffer ssr;
		vks::Buffer raycast;
		vks::Buffer gbuffer_plane;
		vks::Buffer gbuffer_dragon;
		vks::Buffer temprol;
	
	} uniformBuffers;

	// Framebuffer for offscreen rendering
	struct FrameBufferAttachment {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
		VkFormat format;
		VkSampler sampler;
		VkDescriptorImageInfo descriptor;
		void destroy(VkDevice device)
		{
			vkDestroySampler(device, sampler, nullptr);
			vkDestroyImage(device, image, nullptr);
			vkDestroyImageView(device, view, nullptr);
			vkFreeMemory(device, mem, nullptr);
		}
	};
	struct FrameBuffer {
		int32_t width, height;
		std::vector<VkFramebuffer> frameBuffer;
		VkRenderPass renderPass;
		void setSize(int32_t w, int32_t h)
		{
			this->width = w;
			this->height = h;
		}
		void setFrameBufferSize(int32_t fb_size)
		{
			frameBuffer.resize(fb_size);
		}
		void destroy(VkDevice device)
		{
			for (auto fb:frameBuffer) {
				vkDestroyFramebuffer(device, fb, nullptr);
			}
			vkDestroyRenderPass(device, renderPass, nullptr);
		}
	};

	struct {
		struct Offscreen : public FrameBuffer {
			FrameBufferAttachment normal, depth,color;
		} offscreen;
		struct RayCast : public FrameBuffer {
			FrameBufferAttachment rayCast, rayCastMask;
		} rayCast;
		struct ssr : public FrameBuffer {
			FrameBufferAttachment color;
		} ssr;
		struct temprol : public FrameBuffer {
			FrameBufferAttachment color[2];
		} temprol;
		
	} frameBuffers;

	
	enum MaterialType
	{
		DISNEY,
		GLASS
	};

	class Material
	{
	public:
		Material()
		{
			albedo = glm::vec3(1.0f, 1.0f, 1.0f);
			materialType = DISNEY;
			emission = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
			metallic = 0.0f;
			roughness = 0.5f;
	
		};

		glm::vec3 albedo;
		float materialType;
		glm::vec4 emission;
		float metallic;
		float roughness;
	
	};

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Screen space ambient occlusion";
		settings.overlay = true;
		camera.type = Camera::CameraType::firstperson;
		camera.movementSpeed = 5.0f;
#ifndef __ANDROID__
		camera.rotationSpeed = 0.25f;
#endif
		camera.position = { 7.5f, -6.75f, 0.0f };
		camera.setRotation(glm::vec3(5.0f, 90.0f, 0.0f));
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 64.0f);
		const int32_t dim = width > height ? height : width;

		numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;
	}

	~VulkanExample()
	{

		// Attachments
		
		frameBuffers.offscreen.normal.destroy(device);
		
		frameBuffers.offscreen.depth.destroy(device);
		frameBuffers.ssr.color.destroy(device);
		frameBuffers.temprol.color[0].destroy(device);
		frameBuffers.temprol.color[1].destroy(device);
		frameBuffers.rayCast.rayCast.destroy(device);
		frameBuffers.rayCast.rayCastMask.destroy(device);
			
		// Framebuffers

		frameBuffers.offscreen.destroy(device);
		frameBuffers.ssr.destroy(device);
		frameBuffers.rayCast.destroy(device);
		

		vkDestroyPipeline(device, pipelines.skybox, nullptr);
		vkDestroyPipeline(device, pipelines.model, nullptr);
		vkDestroyPipeline(device, pipelines.raycast, nullptr);
		vkDestroyPipeline(device, pipelines.ssr, nullptr);

		vkDestroyPipelineLayout(device, pipelineLayouts.gBuffer, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayouts.ssr, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayouts.raycast, nullptr);

		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.gBuffer, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.ssr, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.raycast, nullptr);
	    models.example.destroy();
		models.plane.destroy();
		//models.quad.destroy();

		//textures.envmap.destroy();

		// Uniform buffers
		uniformBuffers.ssr.destroy();
		uniformBuffers.raycast.destroy();

	}
	void loadTexture()
	{
		// We use the Khronos texture format (https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/)
		std::string filename = getAssetPath() + "textures/noise.png";
		// Texture data contains 4 channels (RGBA) with unnormalized 8-bit values, this is the most commonly supported format
		VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

		
		if (!vks::tools::fileExists(filename)) {
			vks::tools::exitFatal("Could not load texture from " + filename + "\n\nThe file may be part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.", -1);
		}
		// TODO: maybe move to vulkan_util or a VulkanDevice class

	// load image file
		int tex_width, tex_height, tex_channels;
		
		stbi_uc * pixels = stbi_load(filename.c_str()
			, &tex_width, &tex_height
			, &tex_channels
			, STBI_rgb_alpha);
		texture.width = tex_width;
		texture.height = tex_height;
		VkDeviceSize image_size = tex_width * tex_height * 4;
		if (!pixels)
		{
			throw std::runtime_error("Failed to load image" + filename);
		}	
		texture.fromBuffer(pixels, image_size, VK_FORMAT_R8G8B8A8_UNORM, tex_width, tex_height, vulkanDevice, queue);
		
		
	}

	void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
		// Check if image format supports linear blitting
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);

		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
			throw std::runtime_error("texture image format does not support linear blitting!");
		}
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = texWidth;
		int32_t mipHeight = texHeight;

		for (uint32_t i = 1; i < mipLevels; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit = {};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; 
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandBuffer,
				image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
		vulkanDevice->flushCommandBuffer(commandBuffer, queue);

	}


	// Create a frame buffer attachment
	void createAttachment(
		VkFormat format,
		VkImageUsageFlagBits usage,
		FrameBufferAttachment *attachment,
		uint32_t width,
		uint32_t height,uint32_t miplevels)
	{
		VkImageAspectFlags aspectMask = 0;
		VkImageLayout imageLayout;

		attachment->format = format;

		if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		{
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;// | VK_IMAGE_ASPECT_STENCIL_BIT;
			imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		assert(aspectMask > 0);

		VkImageCreateInfo image = vks::initializers::imageCreateInfo();
		image.imageType = VK_IMAGE_TYPE_2D;
		image.format = format;
		image.extent.width = width;
		image.extent.height = height;
		image.extent.depth = 1;
		image.mipLevels = miplevels;
		image.arrayLayers = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &attachment->image));
		vkGetImageMemoryRequirements(device, attachment->image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &attachment->mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, attachment->image, attachment->mem, 0));

		VkImageViewCreateInfo imageView = vks::initializers::imageViewCreateInfo();
		imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageView.format = format;
		imageView.subresourceRange = {};
		imageView.subresourceRange.aspectMask = aspectMask;
		imageView.subresourceRange.baseMipLevel = 0;
		imageView.subresourceRange.levelCount = miplevels;
		imageView.subresourceRange.baseArrayLayer = 0;
		imageView.subresourceRange.layerCount = 1;
		imageView.image = attachment->image;
		VK_CHECK_RESULT(vkCreateImageView(device, &imageView, nullptr, &attachment->view));
		// Shared sampler used for all color attachments
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.minLod = 0.0f;
		sampler.maxLod = static_cast<float>(miplevels);
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &attachment->sampler));
		attachment->descriptor = vks::initializers::descriptorImageInfo(attachment->sampler, attachment->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	}

	void prepareOffscreenFramebuffers()
	{

		const uint32_t ssrWidth = width;
		const uint32_t ssrHeight = height;

		frameBuffers.offscreen.setFrameBufferSize(1);
		frameBuffers.ssr.setFrameBufferSize(1);
		frameBuffers.rayCast.setFrameBufferSize(1);
		frameBuffers.temprol.setFrameBufferSize(2);

		frameBuffers.offscreen.setSize(width, height);
		frameBuffers.ssr.setSize(ssrWidth, ssrHeight);
		frameBuffers.rayCast.setSize(width, height);
		frameBuffers.temprol.setSize(width, height);


		// Find a suitable depth format
		VkFormat attDepthFormat;
		VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &attDepthFormat);
		assert(validDepthFormat);

		// G-Buffer 
		
		
		createAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &frameBuffers.offscreen.color, width, height, numMips);			// Normals
		createAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &frameBuffers.offscreen.normal, width, height,1);			// Normals
		createAttachment(attDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &frameBuffers.offscreen.depth, width, height,1);			// Depth

		// ssr
		createAttachment(VK_FORMAT_R32G32B32A32_UINT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &frameBuffers.ssr.color, ssrWidth, ssrHeight,1);				// Color
		createAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &frameBuffers.temprol.color[0], ssrWidth, ssrHeight,1);				// Color
		createAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &frameBuffers.temprol.color[1], ssrWidth, ssrHeight,1);				// Color

		createAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &frameBuffers.rayCast.rayCast, width, height,1);					// Color
		createAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &frameBuffers.rayCast.rayCastMask, width, height,1);					// Color

		// Render passes

		// G-Buffer creation
		{
			std::array<VkAttachmentDescription, 3> attachmentDescs = {};

			// Init attachment properties
			for (uint32_t i = 0; i < static_cast<uint32_t>(attachmentDescs.size()); i++)
			{
				attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
				attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachmentDescs[i].finalLayout = (i == 2) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			// Formats
			attachmentDescs[0].format = frameBuffers.offscreen.normal.format;
			attachmentDescs[1].format = frameBuffers.offscreen.color.format;
			attachmentDescs[2].format =  frameBuffers.offscreen.depth.format;

			std::vector<VkAttachmentReference> colorReferences;
			colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
			colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

			

			VkAttachmentReference depthReference = {};
			depthReference.attachment = 2;
			depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.pColorAttachments = colorReferences.data();
			subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
			subpass.pDepthStencilAttachment = &depthReference;

			// Use subpass dependencies for attachment layout transitions
			std::array<VkSubpassDependency, 2> dependencies;

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			VkRenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.pAttachments = attachmentDescs.data();
			renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 2;
			renderPassInfo.pDependencies = dependencies.data();
			VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &frameBuffers.offscreen.renderPass));

			std::array<VkImageView, 3> attachments;
			attachments[0] = frameBuffers.offscreen.normal.view;
			attachments[1] = frameBuffers.offscreen.color.view;
			attachments[2] =  frameBuffers.offscreen.depth.view;

			VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
			fbufCreateInfo.renderPass = frameBuffers.offscreen.renderPass;
			fbufCreateInfo.pAttachments = attachments.data();
			fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			fbufCreateInfo.width = frameBuffers.offscreen.width;
			fbufCreateInfo.height = frameBuffers.offscreen.height;
			fbufCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &frameBuffers.offscreen.frameBuffer[0]));
		}

		
		{
			VkAttachmentDescription attachmentDescription{};
			attachmentDescription.format = frameBuffers.ssr.color.format;
			attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

			VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.pColorAttachments = &colorReference;
			subpass.colorAttachmentCount = 1;

			std::array<VkSubpassDependency, 2> dependencies;

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			VkRenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.pAttachments = &attachmentDescription;
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 2;
			renderPassInfo.pDependencies = dependencies.data();
			VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &frameBuffers.ssr.renderPass));


			VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
			fbufCreateInfo.renderPass = frameBuffers.ssr.renderPass;
			fbufCreateInfo.pAttachments = &frameBuffers.ssr.color.view;
			fbufCreateInfo.attachmentCount = 1;
			fbufCreateInfo.width = frameBuffers.ssr.width;
			fbufCreateInfo.height = frameBuffers.ssr.height;
			fbufCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &frameBuffers.ssr.frameBuffer[0]));
			
		}
		{
			VkAttachmentDescription attachmentDescription{};
			attachmentDescription.format = frameBuffers.temprol.color[0].format;
			attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

			VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.pColorAttachments = &colorReference;
			subpass.colorAttachmentCount = 1;

			std::array<VkSubpassDependency, 2> dependencies;

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			VkRenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.pAttachments = &attachmentDescription;
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 2;
			renderPassInfo.pDependencies = dependencies.data();
			VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &frameBuffers.temprol.renderPass));


			VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
			fbufCreateInfo.renderPass = frameBuffers.temprol.renderPass;
			fbufCreateInfo.pAttachments = &frameBuffers.temprol.color[current].view;
			fbufCreateInfo.attachmentCount = 1;
			fbufCreateInfo.width = frameBuffers.temprol.width;
			fbufCreateInfo.height = frameBuffers.temprol.height;
			fbufCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &frameBuffers.temprol.frameBuffer[current]));
			fbufCreateInfo.pAttachments = &frameBuffers.temprol.color[1-current].view;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &frameBuffers.temprol.frameBuffer[1-current]));
		}
		
		{
			VkAttachmentDescription attachmentDescription[2];
			attachmentDescription[0].format = frameBuffers.rayCast.rayCast.format;
			attachmentDescription[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescription[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescription[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescription[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescription[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDescription[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescription[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			attachmentDescription[1].format = frameBuffers.rayCast.rayCastMask.format;
			attachmentDescription[1].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescription[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescription[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescription[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescription[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDescription[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescription[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			std::vector<VkAttachmentReference> colorReferences;
			colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
			colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

			VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.pColorAttachments = colorReferences.data();
			subpass.colorAttachmentCount = 2;

			std::array<VkSubpassDependency, 2> dependencies;

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			VkRenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.pAttachments = attachmentDescription;
			renderPassInfo.attachmentCount = 2;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 2;
			renderPassInfo.pDependencies = dependencies.data();
			VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &frameBuffers.rayCast.renderPass));

			std::array<VkImageView, 2> attachments;
			attachments[0] = frameBuffers.rayCast.rayCast.view;
			attachments[1] = frameBuffers.rayCast.rayCastMask.view;

			VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
			fbufCreateInfo.renderPass = frameBuffers.rayCast.renderPass;
			fbufCreateInfo.pAttachments = attachments.data();
			fbufCreateInfo.attachmentCount = 2;
			fbufCreateInfo.width = frameBuffers.rayCast.width;
			fbufCreateInfo.height = frameBuffers.rayCast.height;
			fbufCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &frameBuffers.rayCast.frameBuffer[0]));
		}

		
	}

	void loadAssets()
	{
		models.plane.loadFromFile(getAssetPath() + "models/sibenik/sibenik.dae", vertexLayout, 0.5f, vulkanDevice, queue);
	//	models.example.loadFromFile(getAssetPath() + "models/chinesedragon.dae", vertexLayout, 0.3f, vulkanDevice, queue);
		models.example.loadFromFile(getAssetPath() + "models/geosphere.obj", vertexLayout_sphere, 0.05f, vulkanDevice, queue);
		loadTexture();
		}


	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkDeviceSize offsets[1] = { 0 };
		VkViewport viewport;
		VkRect2D scissor;
		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));


			/*
				Offscreen ssr generation
			*/
			{
				// Clear values for all attachments written in the fragment sahder
				std::vector<VkClearValue> clearValues(3);
				clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
				clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
				clearValues[2].depthStencil = { 1.0f, 0 };

				VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
				renderPassBeginInfo.renderPass = frameBuffers.offscreen.renderPass;
				renderPassBeginInfo.framebuffer =  frameBuffers.offscreen.frameBuffer[0];
				renderPassBeginInfo.renderArea.extent.width = frameBuffers.offscreen.width;
				renderPassBeginInfo.renderArea.extent.height = frameBuffers.offscreen.height;
				renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
				renderPassBeginInfo.pClearValues = clearValues.data();

				

				vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				VkViewport viewport = vks::initializers::viewport((float)frameBuffers.offscreen.width, (float)frameBuffers.offscreen.height, 0.0f, 1.0f);
				vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

				VkRect2D scissor = vks::initializers::rect2D(frameBuffers.offscreen.width, frameBuffers.offscreen.height, 0, 0);
				vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.gBuffer, 0, 1, &descriptorSets.gbuffer_dragon, 0, NULL);
					
				
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.model);
				vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &models.example.vertices.buffer, offsets);
				vkCmdBindIndexBuffer(drawCmdBuffers[i], models.example.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(drawCmdBuffers[i], models.example.indexCount, 1, 0, 0, 0);

				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.gBuffer, 0, 1, &descriptorSets.gbuffer_plane, 0, NULL);

				vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &models.plane.vertices.buffer, offsets);
				vkCmdBindIndexBuffer(drawCmdBuffers[i], models.plane.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
				vkCmdDrawIndexed(drawCmdBuffers[i], models.plane.indexCount, 1, 0, 0, 0);
				
				vkCmdEndRenderPass(drawCmdBuffers[i]);
				generateMipmaps(frameBuffers.offscreen.color.image, VK_FORMAT_R8G8B8A8_SNORM, width, height, numMips);
				/*
					Second pass: ssr generation
				*/
				clearValues.resize(1);
				clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
			
				renderPassBeginInfo.framebuffer = frameBuffers.rayCast.frameBuffer[0];
				renderPassBeginInfo.renderPass = frameBuffers.rayCast.renderPass;
				renderPassBeginInfo.renderArea.extent.width = frameBuffers.rayCast.width;
				renderPassBeginInfo.renderArea.extent.height = frameBuffers.rayCast.height;
				renderPassBeginInfo.clearValueCount = 1;
				renderPassBeginInfo.pClearValues = clearValues.data();

				vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				viewport = vks::initializers::viewport((float)frameBuffers.rayCast.width, (float)frameBuffers.rayCast.height, 0.0f, 1.0f);
				vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
				scissor = vks::initializers::rect2D(frameBuffers.rayCast.width, frameBuffers.rayCast.height, 0, 0);
				vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.raycast, 0, 1, &descriptorSets.raycast, 0, NULL);
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.raycast);
				vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

				vkCmdEndRenderPass(drawCmdBuffers[i]);

				/*
					Third pass: ssr blur
			*/
	         	renderPassBeginInfo.framebuffer =  frameBuffers.ssr.frameBuffer[0];
				renderPassBeginInfo.renderPass = frameBuffers.ssr.renderPass;
				renderPassBeginInfo.renderArea.extent.width = frameBuffers.ssr.width;
				renderPassBeginInfo.renderArea.extent.height = frameBuffers.ssr.height;

				vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				viewport = vks::initializers::viewport((float)frameBuffers.ssr.width, (float)frameBuffers.ssr.height, 0.0f, 1.0f);
				vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
				scissor = vks::initializers::rect2D(frameBuffers.ssr.width, frameBuffers.ssr.height, 0, 0);
				vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.ssr, 0, 1, &descriptorSets.ssr, 0, NULL);
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.ssr);
				vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

				vkCmdEndRenderPass(drawCmdBuffers[i]);

				VkImageMemoryBarrier  imageMemoryBarrier = {};
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemoryBarrier.image = frameBuffers.temprol.color[current].image;
				imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT; 
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;


				vkCmdPipelineBarrier(
					drawCmdBuffers[i],
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					VK_FLAGS_NONE,
					0, nullptr,
					0, nullptr,
					1, &imageMemoryBarrier);

			
				renderPassBeginInfo.framebuffer = frameBuffers.temprol.frameBuffer[current];
				renderPassBeginInfo.renderPass = frameBuffers.temprol.renderPass;
				renderPassBeginInfo.renderArea.extent.width = frameBuffers.temprol.width;
				renderPassBeginInfo.renderArea.extent.height = frameBuffers.temprol.height;

				vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				viewport = vks::initializers::viewport((float)frameBuffers.temprol.width, (float)frameBuffers.temprol.height, 0.0f, 1.0f);
				vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
				scissor = vks::initializers::rect2D(frameBuffers.temprol.width, frameBuffers.temprol.height, 0, 0);
				vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.temprol, 0, 1, &descriptorSets.temprol, 0, NULL);
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.temprol);
				vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

				vkCmdEndRenderPass(drawCmdBuffers[i]);

				imageMemoryBarrier = {};
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemoryBarrier.image = frameBuffers.temprol.color[current].image;
				imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			

				vkCmdPipelineBarrier(
					drawCmdBuffers[i],
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					VK_FLAGS_NONE,
					0, nullptr,
					0, nullptr,
					1, &imageMemoryBarrier);

				clearValues.resize(2);
				clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
				clearValues[1].depthStencil = { 1.0f, 0 };

				renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
				renderPassBeginInfo.pClearValues = clearValues.data();
				renderPassBeginInfo.framebuffer = VulkanExampleBase::frameBuffers[i];
				renderPassBeginInfo.renderPass = renderPass;
				renderPassBeginInfo.renderArea.extent.width = width;
				renderPassBeginInfo.renderArea.extent.height = height;

				vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
				vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
				scissor = vks::initializers::rect2D(width,height, 0, 0);
				vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.show, 0, 1, &descriptorSets.show, 0, NULL);
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.show);
				vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

				vkCmdEndRenderPass(drawCmdBuffers[i]);
				
			}

		

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void setupDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 15),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,30)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, descriptorSets.count);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}
	void updateModelDescriptorSet() {
		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.gbuffer_dragon, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.gbuffer_dragon.descriptor),

		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
		
	
		writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.gbuffer_plane, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.gbuffer_plane.descriptor),

		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

	}

	void updateDescriptorSet() {
		
		std::vector<VkDescriptorImageInfo> imageDescriptors;

		imageDescriptors = {
			frameBuffers.offscreen.normal.descriptor,
			frameBuffers.offscreen.depth.descriptor,
			frameBuffers.rayCast.rayCast.descriptor,
			frameBuffers.offscreen.color.descriptor,
			frameBuffers.rayCast.rayCastMask.descriptor,
			frameBuffers.ssr.color.descriptor,
			frameBuffers.temprol.color[current].descriptor,
			frameBuffers.temprol.color[1-current].descriptor
			
		};
		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		if (uboraycastParams.num == 1) {
			writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(descriptorSets.temprol, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.temprol.descriptor),
				vks::initializers::writeDescriptorSet(descriptorSets.temprol, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageDescriptors[1]),					// FS Position+Depth
				vks::initializers::writeDescriptorSet(descriptorSets.temprol, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageDescriptors[5]),					// FS Normals
				vks::initializers::writeDescriptorSet(descriptorSets.temprol, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &imageDescriptors[3]),

			};
		}
		else {
			writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(descriptorSets.temprol, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.temprol.descriptor),
				vks::initializers::writeDescriptorSet(descriptorSets.temprol, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageDescriptors[1]),					// FS Position+Depth
				vks::initializers::writeDescriptorSet(descriptorSets.temprol, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &frameBuffers.ssr.color.descriptor),					// FS Normals
				vks::initializers::writeDescriptorSet(descriptorSets.temprol, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &imageDescriptors[7]),

			};
		}
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		
		writeDescriptorSets = {

			vks::initializers::writeDescriptorSet(descriptorSets.show, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageDescriptors[6]),
			vks::initializers::writeDescriptorSet(descriptorSets.show, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &frameBuffers.offscreen.color.descriptor),

		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

	}

	void setupLayoutsAndDescriptors()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo;
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo();
		VkDescriptorSetAllocateInfo descriptorAllocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, nullptr, 1);
		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		std::vector<VkDescriptorImageInfo> imageDescriptors;

		imageDescriptors = {
			 frameBuffers.offscreen.normal.descriptor,
			 frameBuffers.offscreen.depth.descriptor,
			 frameBuffers.rayCast.rayCast.descriptor,
			 frameBuffers.offscreen.color.descriptor,
			 frameBuffers.rayCast.rayCastMask.descriptor,
			 frameBuffers.ssr.color.descriptor

		};
		// G-Buffer creation (offscreen scene rendering)
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS, 0),	

			// VS UBO
		};
		setLayoutCreateInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &descriptorSetLayouts.gBuffer));
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayouts.gBuffer;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.gBuffer));
		descriptorAllocInfo.pSetLayouts = &descriptorSetLayouts.gBuffer;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &descriptorSets.gbuffer_dragon));
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &descriptorSets.gbuffer_plane));

		updateModelDescriptorSet();
		
	
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),								// FS ssr Kernel UBO
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),						// FS Position+Depth
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),						// FS Normals
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),		
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),						// FS ssr Noise
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),	
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 6),						// FS ssr Noise
			// FS ssr Noise
		};
		setLayoutCreateInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &descriptorSetLayouts.ssr));
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayouts.ssr;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.ssr));
		descriptorAllocInfo.pSetLayouts = &descriptorSetLayouts.ssr;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &descriptorSets.ssr));

		
		writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.ssr, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.ssr.descriptor),		// FS ssr Kernel UBO
			vks::initializers::writeDescriptorSet(descriptorSets.ssr, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageDescriptors[0]),					// FS Position+Depth
			vks::initializers::writeDescriptorSet(descriptorSets.ssr, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageDescriptors[1]),					// FS Normals
			vks::initializers::writeDescriptorSet(descriptorSets.ssr, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &imageDescriptors[2]),
			vks::initializers::writeDescriptorSet(descriptorSets.ssr, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &imageDescriptors[3]),		// FS ssr Noise
			vks::initializers::writeDescriptorSet(descriptorSets.ssr, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, &imageDescriptors[4]),		// FS ssr Noise
			vks::initializers::writeDescriptorSet(descriptorSets.ssr, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6, &imageDescriptors[5]),		// FS ssr Noise

		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),								// VS UBO
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),	
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),


		};
		setLayoutCreateInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &descriptorSetLayouts.raycast));
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayouts.raycast;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.raycast));
		descriptorAllocInfo.pSetLayouts = &descriptorSetLayouts.raycast;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &descriptorSets.raycast));
		
		writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.raycast, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.raycast.descriptor),		
			vks::initializers::writeDescriptorSet(descriptorSets.raycast, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageDescriptors[1]),
			vks::initializers::writeDescriptorSet(descriptorSets.raycast, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageDescriptors[0]),
			vks::initializers::writeDescriptorSet(descriptorSets.raycast, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &texture.descriptor),


		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		setLayoutBindings = {
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS, 0),
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),						// FS Position+Depth
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),						// FS Position+Depth
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),						// FS Position+Depth
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),						// FS Position+Depth
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),						// FS Position+Depth					// FS Position+Depth

		};

		setLayoutCreateInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &descriptorSetLayouts.temprol));
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayouts.temprol;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.temprol));
		descriptorAllocInfo.pSetLayouts = &descriptorSetLayouts.temprol;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &descriptorSets.temprol));

	
		setLayoutBindings = {
				vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
				vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),


		};
		setLayoutCreateInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &descriptorSetLayouts.show));
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayouts.show;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.show));
		descriptorAllocInfo.pSetLayouts = &descriptorSetLayouts.show;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &descriptorSets.show));

		updateDescriptorSet();
		
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

		VkPipelineRasterizationStateCreateInfo rasterizationState =
			vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE);

		std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates = {
			vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
			vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),

	};

		VkPipelineColorBlendStateCreateInfo colorBlendState =
			vks::initializers::pipelineColorBlendStateCreateInfo(2, blendAttachmentStates.data());

		VkPipelineDepthStencilStateCreateInfo depthStencilState =
			vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

		VkPipelineViewportStateCreateInfo viewportState =
			vks::initializers::pipelineViewportStateCreateInfo(1, 1);

		VkPipelineMultisampleStateCreateInfo multisampleState =
			vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState =
			vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo =
			vks::initializers::pipelineCreateInfo(pipelineLayouts.gBuffer, frameBuffers.offscreen.renderPass);

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();

		VkSpecializationInfo specializationInfo;
		std::array<VkSpecializationMapEntry, 1> specializationMapEntries;

		// Vertex bindings an attributes
		// Binding description
		std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
			vks::initializers::vertexInputBindingDescription(0, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX),
		};

		const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
				vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),					// Location 0: Position			
				vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 3),		// Location 1: UV
				vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 5),	// Location 2: Color
				vks::initializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 8),	// Location 3: Normal
		};

		VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
		vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

		pipelineCreateInfo.pVertexInputState = &vertexInputState;

		// PBR pipeline
		shaderStages[0] = loadShader(getAssetPath() + "shaders/ssao/gbuffer.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/ssao/gbuffer.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Enable depth test and write
		
		// Flip cull mode
		rasterizationState.cullMode = VK_CULL_MODE_NONE;// VK_CULL_MODE_BACK_BIT;
	
		specializationMapEntries[0] = vks::initializers::specializationMapEntry(0, 0, sizeof(uint32_t));
		uint32_t shadertype = 0;
		specializationInfo = vks::initializers::specializationInfo(1, specializationMapEntries.data(), sizeof(shadertype), &shadertype);
		shaderStages[0].pSpecializationInfo = &specializationInfo;
		shaderStages[1].pSpecializationInfo = &specializationInfo;

		depthStencilState.depthTestEnable = VK_TRUE;
		depthStencilState.depthWriteEnable = VK_TRUE;


		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.skybox));

		rasterizationState.cullMode = VK_CULL_MODE_NONE;// VK_CULL_MODE_FRONT_BIT;
	   
		shadertype = 1;
		std::vector<VkVertexInputBindingDescription> vertexInputBindings_sphere = {
		vks::initializers::vertexInputBindingDescription(0, vertexLayout_sphere.stride(), VK_VERTEX_INPUT_RATE_VERTEX),
		};
		const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes_sphere = {
				vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),					// Position
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3),	// Normal
			vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6),		// UV
		};
		VkPipelineVertexInputStateCreateInfo vertexInputState_sphere = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState_sphere.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings_sphere.size());
		vertexInputState_sphere.pVertexBindingDescriptions = vertexInputBindings_sphere.data();
		vertexInputState_sphere.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes_sphere.size());
		vertexInputState_sphere.pVertexAttributeDescriptions = vertexInputAttributes_sphere.data();
		pipelineCreateInfo.pVertexInputState = &vertexInputState_sphere;
		shaderStages[0] = loadShader(getAssetPath() + "shaders/ssao/gbuffer_sphere.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/ssao/gbuffer_sphere.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.model));

		// Empty vertex input state, full screen triangles are generated by the vertex shader
		VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		pipelineCreateInfo.pVertexInputState = &emptyInputState;
		// Final fullscreen composition pass pipeline
		shaderStages[0] = loadShader(getAssetPath() + "shaders/ssao/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/ssao/rayCast.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineCreateInfo.layout = pipelineLayouts.raycast;
		pipelineCreateInfo.renderPass = frameBuffers.rayCast.renderPass;
		rasterizationState.cullMode = VK_CULL_MODE_NONE;// VK_CULL_MODE_FRONT_BIT;
		
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.raycast));
		shaderStages[0] = loadShader(getAssetPath() + "shaders/ssao/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/ssao/stochastic.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineCreateInfo.layout = pipelineLayouts.ssr;
		pipelineCreateInfo.renderPass = frameBuffers.ssr.renderPass;
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments =&blendAttachmentStates[0];
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.ssr));

		shaderStages[0] = loadShader(getAssetPath() + "shaders/ssao/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/ssao/TemprolReprojectionStatic.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineCreateInfo.layout = pipelineLayouts.temprol;
		pipelineCreateInfo.renderPass = frameBuffers.temprol.renderPass;
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = &blendAttachmentStates[0];
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.temprol));

		shaderStages[0] = loadShader(getAssetPath() + "shaders/ssao/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/ssao/fullscreen.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineCreateInfo.layout = pipelineLayouts.show;
		pipelineCreateInfo.renderPass = renderPass;
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = &blendAttachmentStates[0];
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.show));

	}

	float lerp(float a, float b, float f)
	{
		return a + f * (b - a);
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Scene matrices
		vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.ssr,
			sizeof(ubossrParams));

		// ssr parameters 
		vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.raycast,
			sizeof(uboraycastParams));

		vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.gbuffer_dragon,
			sizeof(uboParams));

		vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.gbuffer_plane,
			sizeof(uboParams));

		vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.temprol,
			sizeof(uboTemprolParams));

		// Update
		updateUniformBufferraycastParams();
		updateUniformBufferssrParams();
		updateUniformBufferParams();
		updateUniformBufferTemprolParams();
		}
	void updateUniformBufferraycastParams() {
		uboraycastParams.perspective = camera.matrices.perspective;
		uboraycastParams.view = camera.matrices.view;
		uboraycastParams._NumSteps = 1024;
		uboraycastParams._WorldSpaceCameraPos = camera.position;
		uboraycastParams.time= glm::vec4(timer / 8.0, timer / 4.0, timer / 2.0, timer);
		/*float r1 = ((float)rand() / (RAND_MAX));
		float r2 = ((float)rand() / (RAND_MAX));
		float r3 = ((float)rand() / (RAND_MAX));*/
		
		std::vector<float>jitter = h.GenerateRandomOffset();
		uboraycastParams.randomVector = glm::vec4(jitter[0],jitter[1], 0, 0);//glm::vec4(r1, r2, r3,1.0);
		VK_CHECK_RESULT(uniformBuffers.raycast.map());
		uniformBuffers.raycast.copyTo(&uboraycastParams, sizeof(uboraycastParams));
		uniformBuffers.raycast.unmap();
	}
	
	void updateUniformBufferssrParams()
	{
		ubossrParams.perspective = camera.matrices.perspective;
		ubossrParams.view = camera.matrices.view;
		ubossrParams._ScreenSize = glm::vec4(width, height, 0.0, 0.0);
		ubossrParams._WorldSpaceCameraPos =glm::vec4( camera.position,1.0);
		ubossrParams._MaxMipMap = numMips;;
		VK_CHECK_RESULT(uniformBuffers.ssr.map());
		uniformBuffers.ssr.copyTo(&ubossrParams, sizeof(ubossrParams));
		uniformBuffers.ssr.unmap();
	}
	void updateUniformBufferParams()
	{
		uboParams.projection = camera.matrices.perspective;
		uboParams.view = camera.matrices.view;
		uboParams.model = glm::mat4(1.0f);
		uboParams.model = glm::translate(uboParams.model, glm::vec3(0.0f, 6.3f, 0.0f));
	    glm::vec3 modelRotation = glm::vec3(-90.0f);

		uboParams.model = glm::rotate(uboParams.model, glm::radians(modelRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		VK_CHECK_RESULT(uniformBuffers.gbuffer_dragon.map());
		uniformBuffers.gbuffer_dragon.copyTo(&uboParams, sizeof(uboParams));
		uniformBuffers.gbuffer_dragon.unmap();

		uboParams.model = glm::mat4(1.0f);
//		glm::vec3 modelRotation = glm::vec3(180.0f);

	//	uboParams.model = glm::rotate(uboParams.model, glm::radians(modelRotation.y), glm::vec3(1.0f, 0.0f, 0.0f));
		VK_CHECK_RESULT(uniformBuffers.gbuffer_plane.map());
		uniformBuffers.gbuffer_plane.copyTo(&uboParams, sizeof(uboParams));
		uniformBuffers.gbuffer_plane.unmap();
	}
	void updateUniformBufferTemprolParams()
	{
		uboTemprolParams._SinTime = glm::vec4(timer / 8.0, timer / 4.0, timer / 2.0, timer);
		uboTemprolParams._FeedbackMin_Max_Mscale = glm::vec4(0.8f, 1.0f, 0.0f, 0.0f);
		uboTemprolParams.num++;
		VK_CHECK_RESULT(uniformBuffers.temprol.map());
		uniformBuffers.temprol.copyTo(&uboTemprolParams, sizeof(uboTemprolParams));
		uniformBuffers.temprol.unmap();	
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
		VulkanExampleBase::submitFrame();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		loadAssets();		
		prepareOffscreenFramebuffers();
		prepareUniformBuffers();
		setupDescriptorPool();
		setupLayoutsAndDescriptors();
		preparePipelines();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		draw();
		current = 1 - current;
	
		updateUniformBufferTemprolParams();
		updateUniformBufferraycastParams();
	    updateDescriptorSet();
		buildCommandBuffers();
	}

	virtual void viewChanged()
	{
		updateUniformBufferraycastParams();
		updateUniformBufferssrParams();
		updateUniformBufferParams();
		std::cout << camera.rotation.x<<" "<< camera.rotation.y<<" "<< camera.rotation.z<<std::endl;

	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		
	}
};

VULKAN_EXAMPLE_MAIN()
