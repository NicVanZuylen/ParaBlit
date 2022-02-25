#include "Texture.h"
#include "Shader.h"

#include "ICommandContext.h"
#include "CLib/Vector.h"

#include <iostream>
#include <string>

#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT // Required to ensure glm constructors actually initialize vectors/matrices etc.
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable : 26451 6262 26819) // Warnings coming from stb_image.h
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace PBClient
{
	Texture::Texture(PB::IRenderer* renderer, CLib::Allocator* allocator, const char* filePath, bool srgb, bool convertToCube)
	{
		m_renderer = renderer;
		m_allocator = allocator;

		std::string pathStr = filePath;
		bool isHdr = (pathStr.find(".hdr", 0) != std::string::npos);

		// Load image...
		void* data = nullptr;
		if (isHdr == true)
		{
			data = stbi_loadf(filePath, &m_width, &m_height, &m_channelCount, STBI_rgb_alpha);
		}
		else
		{
			data = stbi_load(filePath, &m_width, &m_height, &m_channelCount, STBI_rgb_alpha);
		}

		if (data)
		{
			std::cout << "Texture: Successfully loaded image: " << filePath << std::endl;

			PB::TextureDataDesc dataDesc{};
			dataDesc.m_data = data;

			PB::TextureDesc textureDesc{};
			textureDesc.m_data = &dataDesc;
			textureDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA;
			textureDesc.m_usageStates = PB::ETextureState::SAMPLED;
			textureDesc.m_width = m_width;
			textureDesc.m_height = m_height;

			if (isHdr)
			{
				textureDesc.m_format = PB::ETextureFormat::R32G32B32A32_FLOAT;
				dataDesc.m_size = m_width * m_height * sizeof(float) * 4;
			}
			else
			{
				textureDesc.m_format = srgb ? PB::ETextureFormat::R8G8B8A8_SRGB : PB::ETextureFormat::R8G8B8A8_UNORM;
				dataDesc.m_size = m_width * m_height * sizeof(uint32_t);
			}

			m_texture = m_renderer->AllocateTexture(textureDesc);
			m_ownsTexture = true;

			// Data is no longer needed here.
			stbi_image_free(data);
		}
		else
		{
			std::cout << "Texture: Failed to load image: " << filePath << std::endl;
		}

		if (convertToCube == true)
		{
			ConvertToCube(isHdr);
		}
	}

	Texture::Texture(PB::IRenderer* renderer, CLib::Allocator* allocator, const char** filePaths, bool srgb, PB::u32 mipCount)
	{
		m_renderer = renderer;
		m_allocator = allocator;

		bool isHdr = false;
		auto loadFace = [&](const char* path) -> void*
		{
			std::string pathStr = path;
			isHdr |= (pathStr.find(".hdr", 0) != std::string::npos);

			if (isHdr)
			{
				return stbi_loadf(path, &m_width, &m_height, &m_channelCount, STBI_rgb_alpha);
			}
			else
			{
				return stbi_load(path, &m_width, &m_height, &m_channelCount, STBI_rgb_alpha);
			}
		};

		// Load image...
		void* data[6]
		{
			loadFace(filePaths[0]),
			loadFace(filePaths[1]),
			loadFace(filePaths[2]),
			loadFace(filePaths[3]),
			loadFace(filePaths[4]),
			loadFace(filePaths[5])
		};


		PB::TextureDataDesc dataDescs[6];
		PB::TextureDesc textureDesc{};
		textureDesc.m_dimension = PB::ETextureDimension::DIMENSION_CUBE;
		textureDesc.m_usageStates = PB::ETextureState::SAMPLED;
		textureDesc.m_width = m_width;
		textureDesc.m_height = m_height;
		textureDesc.m_mipCount = mipCount;

		uint32_t faceSizeBytes; 
		if (isHdr)
		{
			textureDesc.m_format = PB::ETextureFormat::R32G32B32A32_FLOAT;
			faceSizeBytes = m_width * m_height * sizeof(float) * 4;
		}
		else
		{
			textureDesc.m_format = srgb ? PB::ETextureFormat::R8G8B8A8_SRGB : PB::ETextureFormat::R8G8B8A8_UNORM;
			faceSizeBytes = m_width * m_height * sizeof(uint32_t);
		}

		PB::TextureDataDesc* firstValidFace = nullptr;
		PB::TextureDataDesc* prevValidFace = nullptr;
		for (uint32_t face = 0; face < 6; ++face)
		{
			PB::TextureDataDesc& faceDataDesc = dataDescs[face];

			if (data[face] != nullptr)
			{
				std::cout << "Texture: Successfully loaded image for cube face: " << face << " from: " << filePaths[face] << std::endl;

				faceDataDesc.m_data = data[face];
				faceDataDesc.m_size = faceSizeBytes;
				faceDataDesc.m_arrayLayer = face;

				if (firstValidFace == nullptr)
					firstValidFace = &faceDataDesc;

				if(prevValidFace != nullptr)
					prevValidFace->m_next = &faceDataDesc;
				prevValidFace = &faceDataDesc;
			}
			else
			{
				std::cout << "Texture: Failed to load image for cube face: " << face << " from: " << filePaths[face] << std::endl;
			}
		}
		
		if (firstValidFace)
		{
			textureDesc.m_data = firstValidFace;
			textureDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA;
			if(mipCount > 1)
				textureDesc.m_initOptions |= PB::ETextureInitOptions::PB_TEXTURE_INIT_GEN_MIPMAPS;
		}
		m_texture = m_renderer->AllocateTexture(textureDesc);
		m_ownsTexture = true;

		for (auto& ptr : data)
		{
			if (ptr)
				stbi_image_free(ptr);
		}
	}

	Texture::~Texture()
	{
		if (m_ownsTexture && m_texture)
			m_renderer->FreeTexture(m_texture);
		if(m_ownsTexture && m_cubeGenSrcTexture)
			m_renderer->FreeTexture(m_cubeGenSrcTexture);
		m_texture = nullptr;
		m_cubeGenSrcTexture = nullptr;

		for (auto& buf : m_cubeGenConstantsBuffers)
		{
			if (buf != nullptr)
			{
				m_renderer->FreeBuffer(buf);
				buf = nullptr;
			}
		}

		m_ownsTexture = false;
	}

	PB::ITexture* Texture::GetTexture()
	{
		return m_texture;
	}

	int Texture::GetWidth() const
	{
		return m_width;
	}

	int Texture::GetHeight() const
	{
		return m_height;
	}

	void Texture::ConvertToCube(bool isHdr)
	{
		int cubeDim;
		m_width = m_height = cubeDim = 1024;

		PB::TextureDesc cubeDesc{};
		cubeDesc.m_dimension = PB::ETextureDimension::DIMENSION_CUBE;
		cubeDesc.m_usageStates = PB::ETextureState::COLORTARGET | PB::ETextureState::SAMPLED;
		cubeDesc.m_format = isHdr ? PB::ETextureFormat::R16G16B16A16_FLOAT : PB::ETextureFormat::R8G8B8A8_SRGB;
		cubeDesc.m_width = cubeDim;
		cubeDesc.m_height = cubeDim;

		PB::ITexture* cubeTex = m_renderer->AllocateTexture(cubeDesc);
		m_cubeGenSrcTexture = m_texture;
		m_texture = cubeTex;

		PB::RenderPass cubeGenRp = nullptr;
		{
			PB::RenderPassDesc genCubeRpDesc{};
			genCubeRpDesc.m_attachmentCount = 1;
			genCubeRpDesc.m_subpassCount = 1;

			auto& attach = genCubeRpDesc.m_attachments[0];
			attach.m_expectedState = PB::ETextureState::COLORTARGET;
			attach.m_finalState = PB::ETextureState::SAMPLED;
			attach.m_format = cubeDesc.m_format;
			attach.m_loadAction = PB::EAttachmentAction::NONE;
			attach.m_keepContents = true;

			auto& subpass = genCubeRpDesc.m_subpasses[0];
			subpass.m_attachments[0].m_attachmentFormat = cubeDesc.m_format;
			subpass.m_attachments[0].m_attachmentIdx = 0;
			subpass.m_attachments[0].m_usage = PB::EAttachmentUsage::COLOR;

			cubeGenRp = m_renderer->GetRenderPassCache()->GetRenderPass(genCubeRpDesc);
		}

		CLib::Vector<PB::Framebuffer, 6> framebuffers{};
		{
			for (uint32_t i = 0; i < 6; ++i)
			{
				PB::TextureViewDesc rtView{};
				rtView.m_expectedState = PB::ETextureState::COLORTARGET;
				rtView.m_format = cubeDesc.m_format;
				rtView.m_subresources.m_firstArrayElement = i;
				rtView.m_texture = cubeTex;
				rtView.m_type = PB::ETextureViewType::VIEW_TYPE_2D;

				PB::FramebufferDesc cubeGenFBDesc{};
				cubeGenFBDesc.m_attachmentViews[0] = cubeTex->GetRenderTargetView(rtView);
				cubeGenFBDesc.m_width = cubeDim;
				cubeGenFBDesc.m_height = cubeDim;
				cubeGenFBDesc.m_renderPass = cubeGenRp;

				framebuffers.PushBack(m_renderer->GetFramebufferCache()->GetFramebuffer(cubeGenFBDesc));
			}
		}

		PB::Pipeline cubeGenPipeline = 0;
		{
			PB::GraphicsPipelineDesc cubeGenPipelineDesc{};
			cubeGenPipelineDesc.m_attachmentCount = 1;
			cubeGenPipelineDesc.m_depthCompareOP = PB::ECompareOP::ALWAYS;
			cubeGenPipelineDesc.m_renderArea = { 0, 0, 0, 0 };
			cubeGenPipelineDesc.m_stencilTestEnable = false;
			cubeGenPipelineDesc.m_cullMode = PB::EFaceCullMode::NONE;
			cubeGenPipelineDesc.m_subpass = 0;
			cubeGenPipelineDesc.m_renderPass = cubeGenRp;
			cubeGenPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = PBClient::Shader(m_renderer, "Shaders/GLSL/vs_cubegen", m_allocator, true).GetModule();
			cubeGenPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = PBClient::Shader(m_renderer, "Shaders/GLSL/fs_cubegen", m_allocator, true).GetModule();
			cubeGenPipelineDesc.m_colorBlendStates[0].m_enableBlending = false;

			cubeGenPipeline = m_renderer->GetPipelineCache()->GetPipeline(cubeGenPipelineDesc);
		}

		struct CubeGenConstants
		{
			glm::mat4 m_proj;
			glm::mat4 m_view;
		};

		// We will be projecting the sphere map onto each cube face via a render pass for each face.
		// Since each face is in a different direction, we need a view matrix for each face.
		glm::mat4 cubeGenViewMatrices[] =
		{
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), -glm::vec3(0.0f, -1.0f,  0.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), -glm::vec3(0.0f, -1.0f,  0.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), -glm::vec3(0.0f,  0.0f, 1.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), -glm::vec3(0.0f,  0.0f, -1.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), -glm::vec3(0.0f, -1.0f,  0.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), -glm::vec3(0.0f, -1.0f,  0.0f))
		};

		CLib::Vector<PB::UniformBufferView, 6> cubeGenConstantsViews;

		PB::BufferObjectDesc constantsDesc{};
		constantsDesc.m_bufferSize = sizeof(CubeGenConstants);
		constantsDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;

		for (uint32_t face = 0; face < 6; ++face)
		{
			PB::IBufferObject* cubeGenConstants = m_cubeGenConstantsBuffers[face] = m_renderer->AllocateBuffer(constantsDesc);
			PB::UniformBufferView cubeGenConstantsView = cubeGenConstantsViews.PushBack() = cubeGenConstants->GetViewAsUniformBuffer();

			CubeGenConstants* constantsData = reinterpret_cast<CubeGenConstants*>(cubeGenConstants->BeginPopulate());
			constantsData->m_proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
			constantsData->m_view = cubeGenViewMatrices[face];
			cubeGenConstants->EndPopulate();
		}

		PB::SamplerDesc cubeGenSrcSamplerDesc{};
		cubeGenSrcSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
		PB::ResourceView cubeGenSrcSampler = m_renderer->GetSampler(cubeGenSrcSamplerDesc);

		PB::CommandContextDesc contextDesc{};
		contextDesc.m_renderer = m_renderer;
		contextDesc.m_flags = PB::ECommandContextFlags::PRIORITY;
		contextDesc.m_usage = PB::ECommandContextUsage::GRAPHICS;

		PB::SCommandContext scopedContext(m_renderer);
		scopedContext->Init(contextDesc);
		scopedContext->Begin();

		{
			PB::SubresourceRange subresources{};
			subresources.m_arrayCount = 6;
			
			scopedContext->CmdTransitionTexture(cubeTex, PB::ETextureState::NONE, PB::ETextureState::COLORTARGET, subresources);

			uint32_t uCubeDim = static_cast<uint32_t>(cubeDim);

			PB::ResourceView resources[]
			{
				m_cubeGenSrcTexture->GetDefaultSRV(),
				cubeGenSrcSampler
			};

			// Render each face.
			for (uint32_t face = 0; face < framebuffers.Count(); ++face)
			{
				scopedContext->CmdBeginRenderPass(cubeGenRp, uCubeDim, uCubeDim, framebuffers[face], nullptr, 0, false);

				scopedContext->CmdBindPipeline(cubeGenPipeline);
				scopedContext->SetViewport({ 0, 0, uCubeDim, uCubeDim }, 0.0f, 1.0f);
				scopedContext->SetScissor({ 0, 0, uCubeDim, uCubeDim });

				PB::BindingLayout bindings{};
				bindings.m_uniformBufferCount = 1;
				bindings.m_uniformBuffers = &cubeGenConstantsViews[face];
				bindings.m_resourceCount = _countof(resources);
				bindings.m_resourceViews = resources;

				scopedContext->CmdBindResources(bindings);
				scopedContext->CmdDraw(36, 1);

				scopedContext->CmdEndRenderPass();
			}
		}

		scopedContext->End();
		scopedContext->Return();
	}
}

#pragma warning(default : 26451 6262 26819) // Warnings coming from stb_image.h
#pragma warning(pop)