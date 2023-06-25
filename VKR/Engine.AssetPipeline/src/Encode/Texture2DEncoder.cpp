#include "Texture2DEncoder.h"
#include "../PipelineShader.h"
#include "Engine.ParaBlit/ICommandContext.h"

#include "../LightningBC7.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT // Required to ensure glm constructors actually initialize vectors/matrices etc.
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

#include <CLib/String.h>

#include <compressonator.h>

namespace AssetPipeline
{
	Texture2DEncoder::Texture2DEncoder(const char* name, const char* dbName, const char* assetDirectory, const char* pipelineDBDir, PB::IRenderer* renderer)
		: AssetEncoder::EncoderBase(name, dbName, assetDirectory)
		, m_renderer(renderer)
	{
		std::string pipelineDBPath = pipelineDBDir;
		pipelineDBPath += "build\\pipelineShaders.adb";
		m_pipelineDBReader = new AssetEncoder::AssetBinaryDatabaseReader(pipelineDBPath.c_str());

		std::vector<AssetEncoder::FileInfo> fileInfos;
		RecursiveSearchDirectoryForExtension(assetDirectory, fileInfos, ".tga");
		RecursiveSearchDirectoryForExtension(assetDirectory, fileInfos, ".hdr");
		RecursiveSearchDirectoryForExtension(assetDirectory, fileInfos, ".png");
		RecursiveSearchDirectoryForExtension(assetDirectory, fileInfos, ".jpg");
		RecursiveSearchDirectoryForExtension(assetDirectory, fileInfos, ".jpeg");
		RecursiveSearchDirectoryForExtension(assetDirectory, fileInfos, ".bmp");

		std::vector<AssetStatus> assetStatus;
		GetAssetStatus("Textures", fileInfos, assetStatus);

		for (auto& asset : assetStatus)
		{
			Ctrl::IConfigFile* propertyFile = Ctrl::IConfigFile::Create(asset.m_propertyFilePath.c_str(), Ctrl::IConfigFile::EOpenMode::OPEN_READ_WRITE);
			auto* data = propertyFile->GetData();
			if(asset.m_hasPropertyFile == false)
			{
				data->SetBooleanValue("Texture.IgnoreHDR", false);
				data->SetBooleanValue("Texture.IsSkybox", false);
				data->SetBooleanValue("Texture.IsEnvironmentMap", false);
				data->SetBooleanValue("Texture.Compress", true);
				data->SetBooleanValue("Texture.SRGB", true);
				data->SetIntegerValue("Texture.MipCount", 1);
				data->SetIntegerValue("Texture.ArrayCount", 1);
				data->SetStringValue("Texture.CompressionFormat", "BC7");

				propertyFile->WriteData();
			}

			if (data->GetBooleanValue("Texture.IsEnvironmentMap") == true)
			{
				// Environment map path.

				std::string skyboxName = asset.m_dbPath + "_sky";
				AssetEncoder::AssetHandle skyboxHandle(skyboxName.c_str());
				auto skyboxInfo = m_dbReader->GetAssetInfo(skyboxHandle);

				std::string irradianceName = asset.m_dbPath + "_env_irradiance";
				AssetEncoder::AssetHandle irradianceHandle(irradianceName.c_str());
				auto irradianceInfo = m_dbReader->GetAssetInfo(irradianceHandle);

				std::string prefilterName = asset.m_dbPath + "_env";
				AssetEncoder::AssetHandle prefilterHandle(prefilterName.c_str());
				auto prefilterInfo = m_dbReader->GetAssetInfo(prefilterHandle);

				bool outdated = irradianceInfo.m_binarySize == 0 
					|| prefilterInfo.m_binarySize == 0 
					|| (skyboxInfo.m_dateModified < asset.m_lastModifiedTime)
					|| (irradianceInfo.m_dateModified < asset.m_lastModifiedTime)
					|| (prefilterInfo.m_dateModified < asset.m_lastModifiedTime);

				if (outdated)
				{
					EncodeEnvironmentMap(asset, data);
					FlagAsModified();
				}
				else
				{
					WriteUnmodifiedAsset(skyboxName.c_str());
					WriteUnmodifiedAsset(irradianceName.c_str());
					WriteUnmodifiedAsset(prefilterName.c_str());
				}
			}
			else if (asset.m_outdated)
			{
				Encode2DTexture(asset, data);
				FlagAsModified();
			}
			else
			{
				WriteUnmodifiedAsset(asset);
			}

			Ctrl::IConfigFile::Destroy(propertyFile);
		}
	}

	Texture2DEncoder::~Texture2DEncoder()
	{
		delete m_pipelineDBReader;

		for (auto& buffer : m_cubeGenConstantsBuffers)
		{
			if (buffer)
			{
				m_renderer->FreeBuffer(buffer);
				buffer = nullptr;
			}
		}

		for (auto& buffer : m_cubeConvolutionMaterialConstantsBuffers)
		{
			if (buffer)
			{
				m_renderer->FreeBuffer(buffer);
				buffer = nullptr;
			}
		}

		for (auto& buffer : m_cubeConvolutionConstantsBuffers)
		{
			if (buffer)
			{
				m_renderer->FreeBuffer(buffer);
				buffer = nullptr;
			}
		}
	}

	PB::ETextureFormat FormatStringToFormat(const char* str, bool srgb)
	{
		if (strcmp(str, "BC3/5") == 0)
			return srgb ? PB::ETextureFormat::BC3_SRGB : PB::ETextureFormat::BC5_UNORM;
		else if (strcmp(str, "BC7") == 0)
			return srgb ? PB::ETextureFormat::BC7_SRGB : PB::ETextureFormat::BC7_UNORM;


		return srgb ? PB::ETextureFormat::BC3_SRGB : PB::ETextureFormat::BC5_UNORM;
	}

	CMP_FORMAT PBFormatToCMPFormat(PB::ETextureFormat fmt)
	{
		switch (fmt)
		{
		case PB::ETextureFormat::BC5_UNORM:
			return CMP_FORMAT_BC5;
		case PB::ETextureFormat::BC3_SRGB:
			return CMP_FORMAT_BC3;
		case PB::ETextureFormat::BC6H_RGB_U16F:
			return CMP_FORMAT_BC6H;
		case PB::ETextureFormat::BC6H_RGB_S16F:
			return CMP_FORMAT_BC6H_SF;
		case PB::ETextureFormat::BC7_UNORM:
		case PB::ETextureFormat::BC7_SRGB:
			return CMP_FORMAT_BC7;
		default:
			break;
		}
		return CMP_FORMAT_BC5;
	}

	void Texture2DEncoder::Encode2DTexture(const AssetStatus& asset, const Ctrl::IDataContainer* properties)
	{
		// Uncompressed texture encoding path. Encodes RGBA channels in SDR or HDR if supported.

		void* data = nullptr;
		bool isHdr = stbi_is_hdr(asset.m_fullPath.c_str()) > 0 && properties->GetBooleanValue("Texture.IgnoreHDR") == false;
		bool srgb = properties->GetBooleanValue("Texture.SRGB");
		int channelCount = 0;
		int width = 0;
		int height = 0;
		if (isHdr)
		{
			data = stbi_loadf(asset.m_fullPath.c_str(), &width, &height, &channelCount, STBI_rgb_alpha);
		}
		else
		{
			data = stbi_load(asset.m_fullPath.c_str(), &width, &height, &channelCount, STBI_rgb_alpha);
		}

		if (data == nullptr)
		{
			printf("%s: Failed to load image: %s\n", m_name.c_str(), asset.m_fullPath.c_str());
			return;
		}

		printf("%s: Successfully loaded image: %s\n", m_name.c_str(), asset.m_fullPath.c_str());

		TextureMetadata metaData;
		metaData.m_width = uint32_t(width);
		metaData.m_height = uint32_t(height);
		metaData.m_mipCount = 1;
		metaData.m_arraySize = 1;
		metaData.m_isHdr = isHdr;

		uint32_t textureDataSize = isHdr ? (width * height * sizeof(float) * channelCount) : (width * height * sizeof(uint32_t));
		CMP_Texture srcTex{};
		srcTex.dwSize = sizeof(CMP_Texture);
		srcTex.pData = reinterpret_cast<CMP_BYTE*>(data);
		srcTex.dwDataSize = textureDataSize;
		srcTex.dwWidth = width;
		srcTex.dwHeight = height;
		srcTex.dwPitch = 0;
		srcTex.format = CMP_FORMAT_RGBA_8888;

		char* userData = nullptr;
		if (properties->GetBooleanValue("Texture.Compress"))
		{
			size_t compressedTextureDataSize = LightningBC7::CalcBC7TextureSizeBytes(width, height);
			metaData.m_format = FormatStringToFormat(properties->GetStringValue("Texture.CompressionFormat"), srgb);

			uint8_t* outputData = reinterpret_cast<uint8_t*>(m_dbWriter->AllocateAsset(asset.m_dbPath.c_str(), sizeof(TextureMetadata), compressedTextureDataSize, asset.m_lastModifiedTime, &userData));
			LightningBC7::CompressBC7(data, outputData, width, height);
		}
		else
		{
			metaData.m_format = srgb ? PB::ETextureFormat::R8G8B8A8_SRGB : PB::ETextureFormat::R8G8B8A8_UNORM;

			uint8_t* outputData = reinterpret_cast<uint8_t*>(m_dbWriter->AllocateAsset(asset.m_dbPath.c_str(), sizeof(TextureMetadata), textureDataSize, asset.m_lastModifiedTime, &userData));
			memcpy(outputData, data, textureDataSize);
		}
		memcpy(userData, &metaData, sizeof(TextureMetadata));

		stbi_image_free(data);

		printf("%s: Encoded image: %s\n", m_name.c_str(), asset.m_dbPath.c_str());
	}

	void Texture2DEncoder::GenerateEnvironmentMap(PB::ICommandContext* cmdContext, PB::ITexture* srcCube, PB::ITexture*& envMap, EConvolutedMapType mapType)
	{
		assert(envMap == nullptr);

		uint32_t convolutedMapDim = mapType == EConvolutedMapType::IRRADIANCE ? IrradianceMapDimensions : PrefilterMapDimensions;

		PB::TextureDesc convolutedSkyboxDesc{};
		convolutedSkyboxDesc.m_dimension = PB::ETextureDimension::DIMENSION_CUBE;
		convolutedSkyboxDesc.m_format = PB::ETextureFormat::R16G16B16A16_FLOAT;
		convolutedSkyboxDesc.m_width = convolutedMapDim;
		convolutedSkyboxDesc.m_height = convolutedMapDim;
		convolutedSkyboxDesc.m_usageStates = PB::ETextureState::COLORTARGET | PB::ETextureState::COPY_SRC;
		convolutedSkyboxDesc.m_mipCount = mapType == EConvolutedMapType::IRRADIANCE ? 1 : ConvolutionMipmapCount;
		envMap = m_renderer->AllocateTexture(convolutedSkyboxDesc);

		PB::RenderPass convolutionRp = nullptr;
		{
		    PB::RenderPassDesc convolutionRpDesc{};
		    convolutionRpDesc.m_attachmentCount = 1;
		    convolutionRpDesc.m_subpassCount = 1;

		    auto& attach = convolutionRpDesc.m_attachments[0];
		    attach.m_expectedState = PB::ETextureState::COLORTARGET;
		    attach.m_finalState = PB::ETextureState::COPY_SRC;
		    attach.m_format = convolutedSkyboxDesc.m_format;
		    attach.m_loadAction = PB::EAttachmentAction::NONE;
		    attach.m_keepContents = true;

		    auto& subpass = convolutionRpDesc.m_subpasses[0];
		    subpass.m_attachments[0].m_attachmentFormat = convolutedSkyboxDesc.m_format;
		    subpass.m_attachments[0].m_attachmentIdx = 0;
		    subpass.m_attachments[0].m_usage = PB::EAttachmentUsage::COLOR;

		    convolutionRp = m_renderer->GetRenderPassCache()->GetRenderPass(convolutionRpDesc);
		}

		CLib::Vector<PB::Framebuffer, 6 * ConvolutionMipmapCount> framebuffers{};
		{
		    for (uint32_t mip = 0; mip < convolutedSkyboxDesc.m_mipCount; ++mip)
		    {
		        for (uint32_t face = 0; face < 6; ++face)
		        {
		            PB::TextureViewDesc rtView{};
		            rtView.m_expectedState = PB::ETextureState::COLORTARGET;
		            rtView.m_format = convolutedSkyboxDesc.m_format;
		            rtView.m_subresources.m_baseMip = mip;
		            rtView.m_subresources.m_firstArrayElement = face;
		            rtView.m_texture = envMap;
		            rtView.m_type = PB::ETextureViewType::VIEW_TYPE_2D;

		            PB::FramebufferDesc convolutionFBDesc{};
		            convolutionFBDesc.m_attachmentViews[0] = envMap->GetRenderTargetView(rtView);
		            convolutionFBDesc.m_width = (convolutedSkyboxDesc.m_width >> mip);
		            convolutionFBDesc.m_height = (convolutedSkyboxDesc.m_height >> mip);
		            convolutionFBDesc.m_renderPass = convolutionRp;

		            framebuffers.PushBack(m_renderer->GetFramebufferCache()->GetFramebuffer(convolutionFBDesc));
		        }
		    }
		}

		PB::Pipeline convolutionPipeline = 0;
		{
		    if(!m_irradianceFSModule)
				m_irradianceFSModule = AssetPipeline::Shader(m_renderer, m_pipelineDBReader, "Shaders/fs_convolute_env_irradiance_cube").GetModule();

			if (!m_prefilterFSModule)
				m_prefilterFSModule = AssetPipeline::Shader(m_renderer, m_pipelineDBReader, "Shaders/fs_convolute_env_prefilter_cube").GetModule();

			PB::ShaderModule fragmentShaderModule = mapType == EConvolutedMapType::IRRADIANCE ? m_irradianceFSModule : m_prefilterFSModule;

			if (!m_cubeGenVertexModule)
				m_cubeGenVertexModule = AssetPipeline::Shader(m_renderer, m_pipelineDBReader, "Shaders/vs_cubegen").GetModule();

		    PB::GraphicsPipelineDesc convolutionPipelineDesc{};
		    convolutionPipelineDesc.m_attachmentCount = 1;
		    convolutionPipelineDesc.m_depthCompareOP = PB::ECompareOP::ALWAYS;
		    convolutionPipelineDesc.m_renderArea = { 0, 0, 0, 0 };
		    convolutionPipelineDesc.m_stencilTestEnable = false;
		    convolutionPipelineDesc.m_cullMode = PB::EFaceCullMode::NONE;
		    convolutionPipelineDesc.m_subpass = 0;
		    convolutionPipelineDesc.m_renderPass = convolutionRp;
			convolutionPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = m_cubeGenVertexModule;
			convolutionPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = fragmentShaderModule;
		    convolutionPipelineDesc.m_colorBlendStates[0].m_enableBlending = false;

		    convolutionPipeline = m_renderer->GetPipelineCache()->GetPipeline(convolutionPipelineDesc);
		}

		struct ConvolutionConstants
		{
		    glm::mat4 m_proj;
		    glm::mat4 m_view;
		};

		// We will be projecting the sphere map onto each cube face via a render pass for each face.
		// Since each face is in a different direction, we need a view matrix for each face.
		glm::mat4 cubeConvolutionViewMatrices[] =
		{
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), -glm::vec3(0.0f, -1.0f,  0.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), -glm::vec3(0.0f, -1.0f,  0.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), -glm::vec3(0.0f,  0.0f, 1.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), -glm::vec3(0.0f,  0.0f, -1.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), -glm::vec3(0.0f, -1.0f,  0.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), -glm::vec3(0.0f, -1.0f,  0.0f))
		};

		CLib::Vector<PB::UniformBufferView, 6> cubeConvolutionConstantsViews{};

		PB::BufferObjectDesc constantsDesc{};
		constantsDesc.m_bufferSize = sizeof(ConvolutionConstants);
		constantsDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;

		for (uint32_t face = 0; face < 6; ++face)
		{
		    PB::IBufferObject*& cubeConvolutionConstants = m_cubeConvolutionConstantsBuffers[face];
		    if (cubeConvolutionConstants == nullptr)
		    {
		        cubeConvolutionConstants = m_renderer->AllocateBuffer(constantsDesc);

		        ConvolutionConstants* constantsData = reinterpret_cast<ConvolutionConstants*>(cubeConvolutionConstants->BeginPopulate());
		        constantsData->m_proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		        constantsData->m_view = cubeConvolutionViewMatrices[face];
		        cubeConvolutionConstants->EndPopulate();
		    }

		    cubeConvolutionConstantsViews.PushBack(cubeConvolutionConstants->GetViewAsUniformBuffer());
		}

		struct ConvolutionMaterialConstants
		{
		    float m_roughness;
		    PB::Float3 m_pad0;
		};
		constantsDesc.m_bufferSize = sizeof(ConvolutionMaterialConstants);

		CLib::Vector<PB::UniformBufferView, ConvolutionMipmapCount> cubeConvolutionMaterialConstantsViews{};
		for (uint32_t mip = 0; mip < ConvolutionMipmapCount; ++mip)
		{
		    PB::IBufferObject*& cubeConvolutionMaterialConstants = m_cubeConvolutionMaterialConstantsBuffers[mip];
		    if (cubeConvolutionMaterialConstants == nullptr)
		    {
		        cubeConvolutionMaterialConstants = m_renderer->AllocateBuffer(constantsDesc);

		        ConvolutionMaterialConstants* constantsData = reinterpret_cast<ConvolutionMaterialConstants*>(cubeConvolutionMaterialConstants->BeginPopulate());
		        constantsData->m_roughness = float(mip) / (ConvolutionMipmapCount - 1);
		        constantsData->m_pad0 = {};
		        cubeConvolutionMaterialConstants->EndPopulate();
		    }

		    cubeConvolutionMaterialConstantsViews.PushBack(cubeConvolutionMaterialConstants->GetViewAsUniformBuffer());
		}

		{
		    PB::TextureViewDesc srcViewDesc{};
			srcViewDesc.m_texture = srcCube;
		    srcViewDesc.m_expectedState = PB::ETextureState::SAMPLED;
		    srcViewDesc.m_format = PB::ETextureFormat::R16G16B16A16_FLOAT;
		    srcViewDesc.m_type = PB::ETextureViewType::VIEW_TYPE_CUBE;
		    srcViewDesc.m_subresources.m_arrayCount = 1;
		    srcViewDesc.m_subresources.m_mipCount = 1;

		    PB::SamplerDesc srcSamplerDesc{};
		    srcSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
		    srcSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::REPEAT;
		    PB::ResourceView srcSampler = m_renderer->GetSampler(srcSamplerDesc);

		    PB::ResourceView resources[]
		    {
		        srcCube->GetView(srcViewDesc),
		        srcSampler
		    };

		    PB::SubresourceRange subresources{};
		    subresources.m_baseMip = 0;
		    subresources.m_mipCount = convolutedSkyboxDesc.m_mipCount;
		    subresources.m_firstArrayElement = 0;
		    subresources.m_arrayCount = 6;

		    cmdContext->CmdTransitionTexture(envMap, PB::ETextureState::NONE, PB::ETextureState::COLORTARGET, subresources);

		    // Render each face for each mip level.
		    for (uint32_t mip = 0; mip < convolutedSkyboxDesc.m_mipCount; ++mip)
		    {
		        uint32_t mipWidth = (convolutedSkyboxDesc.m_width >> mip);
		        uint32_t mipHeight = (convolutedSkyboxDesc.m_height >> mip);

		        for (uint32_t face = 0; face < 6; ++face)
		        {
		            uint32_t fbIndex = (mip * 6) + face;
		            cmdContext->CmdBeginRenderPass(convolutionRp, mipWidth, mipHeight, framebuffers[fbIndex], nullptr, 0, false);

		            cmdContext->CmdBindPipeline(convolutionPipeline);
		            cmdContext->CmdSetViewport({ 0, 0, mipWidth, mipHeight }, 0.0f, 1.0f);
		            cmdContext->CmdSetScissor({ 0, 0, mipWidth, mipHeight });

		            PB::UniformBufferView uniformViews[]
		            {
		                cubeConvolutionConstantsViews[face],
		                cubeConvolutionMaterialConstantsViews[mip]
		            };

		            PB::BindingLayout bindings{};
		            bindings.m_uniformBufferCount = mapType == EConvolutedMapType::IRRADIANCE ? 1 : 2;
		            bindings.m_uniformBuffers = uniformViews;
		            bindings.m_resourceCount = _countof(resources);
		            bindings.m_resourceViews = resources;

		            cmdContext->CmdBindResources(bindings);
		            cmdContext->CmdDraw(36, 1);

		            cmdContext->CmdEndRenderPass();
		        }
		    }
		}

	}

	PB::ITexture* Texture2DEncoder::GetAsCube(PB::ICommandContext* cmdContext, PB::ITexture* srcTexture, bool isHdr)
	{
		PB::TextureDesc cubeDesc{};
		cubeDesc.m_dimension = PB::ETextureDimension::DIMENSION_CUBE;
		cubeDesc.m_usageStates = PB::ETextureState::COLORTARGET | PB::ETextureState::SAMPLED | PB::ETextureState::COPY_SRC;
		cubeDesc.m_format = isHdr ? PB::ETextureFormat::R16G16B16A16_FLOAT : PB::ETextureFormat::R8G8B8A8_SRGB;
		cubeDesc.m_width = SkyboxDimensions;
		cubeDesc.m_height = SkyboxDimensions;

		PB::ITexture* cubeTex = m_renderer->AllocateTexture(cubeDesc);

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
				cubeGenFBDesc.m_width = SkyboxDimensions;
				cubeGenFBDesc.m_height = SkyboxDimensions;
				cubeGenFBDesc.m_renderPass = cubeGenRp;

				framebuffers.PushBack(m_renderer->GetFramebufferCache()->GetFramebuffer(cubeGenFBDesc));
			}
		}

		PB::Pipeline cubeGenPipeline = 0;
		{
			if (!m_cubeGenVertexModule)
				m_cubeGenVertexModule = Shader(m_renderer, m_pipelineDBReader, "Shaders/vs_cubegen").GetModule();

			if (!m_cubeGenFragmentModule)
				m_cubeGenFragmentModule = Shader(m_renderer, m_pipelineDBReader, "Shaders/fs_cubegen").GetModule();

			PB::GraphicsPipelineDesc cubeGenPipelineDesc{};
			cubeGenPipelineDesc.m_attachmentCount = 1;
			cubeGenPipelineDesc.m_depthCompareOP = PB::ECompareOP::ALWAYS;
			cubeGenPipelineDesc.m_renderArea = { 0, 0, 0, 0 };
			cubeGenPipelineDesc.m_stencilTestEnable = false;
			cubeGenPipelineDesc.m_cullMode = PB::EFaceCullMode::NONE;
			cubeGenPipelineDesc.m_subpass = 0;
			cubeGenPipelineDesc.m_renderPass = cubeGenRp;
			cubeGenPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = m_cubeGenVertexModule;
			cubeGenPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = m_cubeGenFragmentModule;
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
			if (!m_cubeGenConstantsBuffers[face])
				m_cubeGenConstantsBuffers[face] = m_renderer->AllocateBuffer(constantsDesc);

			PB::IBufferObject* cubeGenConstants = m_cubeGenConstantsBuffers[face];
			PB::UniformBufferView cubeGenConstantsView = cubeGenConstantsViews.PushBack() = cubeGenConstants->GetViewAsUniformBuffer();

			CubeGenConstants* constantsData = reinterpret_cast<CubeGenConstants*>(cubeGenConstants->BeginPopulate());
			constantsData->m_proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
			constantsData->m_view = cubeGenViewMatrices[face];
			cubeGenConstants->EndPopulate();
		}

		PB::SamplerDesc cubeGenSrcSamplerDesc{};
		cubeGenSrcSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
		PB::ResourceView cubeGenSrcSampler = m_renderer->GetSampler(cubeGenSrcSamplerDesc);

		{
			cmdContext->CmdTransitionTexture(srcTexture, PB::ETextureState::COPY_DST, PB::ETextureState::SAMPLED);
			cmdContext->CmdTransitionTexture(cubeTex, PB::ETextureState::NONE, PB::ETextureState::COLORTARGET);

			PB::ResourceView resources[]
			{
				srcTexture->GetDefaultSRV(),
				cubeGenSrcSampler
			};

			// Render each face.
			for (uint32_t face = 0; face < framebuffers.Count(); ++face)
			{
				cmdContext->CmdBeginRenderPass(cubeGenRp, SkyboxDimensions, SkyboxDimensions, framebuffers[face], nullptr, 0, false);

				cmdContext->CmdBindPipeline(cubeGenPipeline);
				cmdContext->CmdSetViewport({ 0, 0, SkyboxDimensions, SkyboxDimensions }, 0.0f, 1.0f);
				cmdContext->CmdSetScissor({ 0, 0, SkyboxDimensions, SkyboxDimensions });

				PB::BindingLayout bindings{};
				bindings.m_uniformBufferCount = 1;
				bindings.m_uniformBuffers = &cubeGenConstantsViews[face];
				bindings.m_resourceCount = _countof(resources);
				bindings.m_resourceViews = resources;

				cmdContext->CmdBindResources(bindings);
				cmdContext->CmdDraw(36, 1);

				cmdContext->CmdEndRenderPass();
			}
		}

		return cubeTex;
	}

	void Texture2DEncoder::EncodeEnvironmentMap(const AssetStatus& asset, const Ctrl::IDataContainer* properties)
	{
		printf_s("%s: Encoding environment map: %s\n", m_name.c_str(), asset.m_dbPath.c_str());

		void* data = nullptr;
		int channelCount = 0;
		int width = 0;
		int height = 0;
		data = stbi_loadf(asset.m_fullPath.c_str(), &width, &height, &channelCount, STBI_rgb_alpha); // File must be HDR.
		assert(data != nullptr);

		PB::TextureDataDesc srcDataDesc{};
		srcDataDesc.m_data = data;
		srcDataDesc.m_size = width * height * sizeof(float) * 4;
		srcDataDesc.m_next = nullptr;

		PB::TextureDesc srcMapDesc{};
		srcMapDesc.m_data = &srcDataDesc;
		srcMapDesc.m_usageStates = PB::ETextureState::SAMPLED | PB::ETextureState::COPY_DST;
		srcMapDesc.m_format = PB::ETextureFormat::R32G32B32A32_FLOAT;
		srcMapDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA;
		srcMapDesc.m_width = width;
		srcMapDesc.m_height = height;
		PB::ITexture* srcMap = m_renderer->AllocateTexture(srcMapDesc);
		stbi_image_free(data);

		PB::ITexture* irradianceMap = nullptr;
		PB::ITexture* prefilterMap = nullptr;

		CLib::Vector<PB::ITexture*, 6> skyReadbackTextures;
		CLib::Vector<PB::ITexture*, 6> irradianceReadbackTextures;
		CLib::Vector<PB::ITexture*, 6 * ConvolutionMipmapCount> prefilterReadbackTextures;

		for (uint32_t layer = 0; layer < 6; ++layer)
		{
			PB::TextureDesc readbackDesc{};
			readbackDesc.m_dimension = PB::ETextureDimension::DIMENSION_2D;
			readbackDesc.m_memoryType = PB::EMemoryType::HOST_VISIBLE;
			readbackDesc.m_usageStates = PB::ETextureState::READBACK | PB::ETextureState::SAMPLED;
			readbackDesc.m_format = PB::ETextureFormat::R16G16B16A16_FLOAT;
			readbackDesc.m_width = SkyboxDimensions;
			readbackDesc.m_height = SkyboxDimensions;

			skyReadbackTextures.PushBack(m_renderer->AllocateTexture(readbackDesc));

			readbackDesc.m_width = IrradianceMapDimensions;
			readbackDesc.m_height = IrradianceMapDimensions;
			irradianceReadbackTextures.PushBack(m_renderer->AllocateTexture(readbackDesc));

			for (uint32_t mip = 0; mip < ConvolutionMipmapCount; ++mip)
			{
				readbackDesc.m_width = PrefilterMapDimensions >> mip;
				readbackDesc.m_height = PrefilterMapDimensions >> mip;

				prefilterReadbackTextures.PushBack(m_renderer->AllocateTexture(readbackDesc));
			}
		}

		PB::ITexture* srcCube = nullptr;

		// Readback commands
		{
			PB::CommandContextDesc cmdContextDesc{};
			cmdContextDesc.m_usage = PB::ECommandContextUsage::GRAPHICS;
			cmdContextDesc.m_renderer = m_renderer;
			PB::SCommandContext cmdContext(m_renderer);
			cmdContext->Init(cmdContextDesc);
			cmdContext->Begin();

			srcCube = GetAsCube(cmdContext.GetContext(), srcMap, true);
			GenerateEnvironmentMap(cmdContext.GetContext(), srcCube, irradianceMap, EConvolutedMapType::IRRADIANCE);
			GenerateEnvironmentMap(cmdContext.GetContext(), srcCube, prefilterMap, EConvolutedMapType::PREFILTER);

			cmdContext->CmdTransitionTexture(srcCube, PB::ETextureState::SAMPLED, PB::ETextureState::COPY_SRC);

			for (uint32_t layer = 0; layer < 6; ++layer)
			{
				PB::ITexture* skyReadback = skyReadbackTextures[layer];
				cmdContext->CmdTransitionTexture(skyReadback, PB::ETextureState::NONE, PB::ETextureState::COPY_DST);
				cmdContext->CmdCopyTextureSubresource(srcCube, skyReadback, 0, layer);
				cmdContext->CmdTransitionTexture(skyReadback, PB::ETextureState::COPY_DST, PB::ETextureState::READBACK);

				PB::ITexture* irradianceReadback = irradianceReadbackTextures[layer];
				cmdContext->CmdTransitionTexture(irradianceReadback, PB::ETextureState::NONE, PB::ETextureState::COPY_DST);
				cmdContext->CmdCopyTextureSubresource(irradianceMap, irradianceReadback, 0, layer);
				cmdContext->CmdTransitionTexture(irradianceReadback, PB::ETextureState::COPY_DST, PB::ETextureState::READBACK);

				for (uint32_t mip = 0; mip < ConvolutionMipmapCount; ++mip)
				{
					PB::ITexture* prefilterReadback = prefilterReadbackTextures[(layer * ConvolutionMipmapCount) + mip];
					cmdContext->CmdTransitionTexture(prefilterReadback, PB::ETextureState::NONE, PB::ETextureState::COPY_DST);
					cmdContext->CmdCopyTextureSubresource(prefilterMap, prefilterReadback, mip, layer);
					cmdContext->CmdTransitionTexture(prefilterReadback, PB::ETextureState::COPY_DST, PB::ETextureState::READBACK);
				}
			}

			if (m_renderer->HasValidSwapchain()) 
			{
				PB::RenderPass previewRP = nullptr;
				{
					PB::AttachmentDesc attach{};
					attach.m_expectedState = PB::ETextureState::COLORTARGET;
					attach.m_finalState = PB::ETextureState::PRESENT;
					attach.m_format = PB::ETextureFormat::B8G8R8A8_UNORM;
					attach.m_loadAction = PB::EAttachmentAction::CLEAR;
					attach.m_keepContents = true;

					PB::AttachmentUsageDesc attachUsage{};
					attachUsage.m_attachmentFormat = attach.m_format;
					attachUsage.m_attachmentIdx = 0;
					attachUsage.m_usage = PB::EAttachmentUsage::COLOR;

					PB::SubpassDesc subpass{};
					subpass.m_attachments[0] = attachUsage;

					PB::RenderPassDesc previewRPDesc{};
					previewRPDesc.m_attachmentCount = 1;
					previewRPDesc.m_attachments[0] = attach;
					previewRPDesc.m_subpassCount = 1;
					previewRPDesc.m_subpasses[0] = subpass;

					previewRP = m_renderer->GetRenderPassCache()->GetRenderPass(previewRPDesc);
				}

				PB::ISwapChain* swapchain = m_renderer->GetSwapchain();
				PB::ITexture* swapchainImg = swapchain->GetImage(m_renderer->GetCurrentSwapchainImageIndex());
				uint32_t swapchainWidth = swapchain->GetWidth();
				uint32_t swapchainHeight = swapchain->GetHeight();

				PB::Framebuffer previewFB = nullptr;
				{
					PB::FramebufferDesc fbDesc{};
					fbDesc.m_attachmentViews[0] = swapchainImg->GetDefaultRTV();
					fbDesc.m_width = swapchainWidth;
					fbDesc.m_height = swapchainHeight;
					fbDesc.m_renderPass = previewRP;

					previewFB = m_renderer->GetFramebufferCache()->GetFramebuffer(fbDesc);
				}

				PB::Pipeline previewPipeline = 0;
				{
					if (!m_previewVSModule)
						m_previewVSModule = Shader(m_renderer, m_pipelineDBReader, "Shaders/vs_screenQuad").GetModule();

					if (!m_previewFSModule)
						m_previewFSModule = Shader(m_renderer, m_pipelineDBReader, "Shaders/fs_screenTexture").GetModule();

					PB::GraphicsPipelineDesc previewPipelineDesc{};
					previewPipelineDesc.m_attachmentCount = 1;
					previewPipelineDesc.m_depthCompareOP = PB::ECompareOP::ALWAYS;
					previewPipelineDesc.m_renderArea = { 0, 0, 0, 0 };
					previewPipelineDesc.m_stencilTestEnable = false;
					previewPipelineDesc.m_cullMode = PB::EFaceCullMode::NONE;
					previewPipelineDesc.m_subpass = 0;
					previewPipelineDesc.m_renderPass = previewRP;
					previewPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = m_previewVSModule;
					previewPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = m_previewFSModule;
					previewPipelineDesc.m_colorBlendStates[0].m_enableBlending = false;

					previewPipeline = m_renderer->GetPipelineCache()->GetPipeline(previewPipelineDesc);
				}

				PB::ITexture* previewTex = irradianceReadbackTextures[0];

				PB::TextureViewDesc previewViewDesc{};
				previewViewDesc.m_texture = previewTex;
				previewViewDesc.m_format = PB::ETextureFormat::R16G16B16A16_FLOAT;
				previewViewDesc.m_expectedState = PB::ETextureState::SAMPLED;
				previewViewDesc.m_type = PB::ETextureViewType::VIEW_TYPE_2D;
				previewViewDesc.m_subresources.m_firstArrayElement = 0;
				previewViewDesc.m_subresources.m_baseMip = 0;
				PB::ResourceView previewView = previewTex->GetView(previewViewDesc);

				// Preview
				{
					cmdContext->CmdTransitionTexture(swapchainImg, PB::ETextureState::PRESENT, PB::ETextureState::COLORTARGET);
					cmdContext->CmdTransitionTexture(previewTex, PB::ETextureState::READBACK, PB::ETextureState::SAMPLED);

					PB::Float4 clearColor{ 0.0f, 0.0f, 0.0f, 1.0f };
					cmdContext->CmdBeginRenderPass(previewRP, swapchainWidth, swapchainHeight, previewFB, &clearColor, 1);

					cmdContext->CmdBindPipeline(previewPipeline);
					cmdContext->CmdSetViewport({ 0, 0, swapchainWidth, swapchainHeight }, 0.0f, 1.0f);
					cmdContext->CmdSetScissor({ 0, 0, swapchainWidth, swapchainHeight });

					PB::BindingLayout bindings{};
					bindings.m_uniformBufferCount = 0;
					bindings.m_uniformBuffers = nullptr;
					bindings.m_resourceCount = 1;
					bindings.m_resourceViews = &previewView;
					cmdContext->CmdBindResources(bindings);
					cmdContext->CmdDraw(6, 1);

					cmdContext->CmdEndRenderPass();

					cmdContext->CmdTransitionTexture(previewTex, PB::ETextureState::SAMPLED, PB::ETextureState::READBACK);
				}
			}

			cmdContext->End();
			cmdContext->Return();
		}

		float sTime = 0.0f;
		m_renderer->EndFrame(sTime);
		m_renderer->WaitIdle();

		m_renderer->FreeTexture(srcMap);
		m_renderer->FreeTexture(srcCube);
		m_renderer->FreeTexture(irradianceMap);
		m_renderer->FreeTexture(prefilterMap);

		// Copy readback data and write assets...

		bool compress = properties->GetBooleanValue("Texture.Compress");

		// Skybox asset:
		{
			std::string skyboxName = asset.m_dbPath + "_sky";

			// All faces will have the same size and align, so just retrieve from the first.
			PB::u32 readbackFaceSize;
			PB::u32 readbackFaceAlign;
			skyReadbackTextures[0]->GetMemorySizeAndAlign(readbackFaceSize, readbackFaceAlign);

			CMP_Texture srcFace{};
			srcFace.dwSize = sizeof(CMP_Texture);
			srcFace.dwDataSize = readbackFaceSize;
			srcFace.dwWidth = SkyboxDimensions;
			srcFace.dwHeight = SkyboxDimensions;
			srcFace.dwPitch = 0;
			srcFace.format = CMP_FORMAT_ARGB_16F;

			PB::u32 compressedFaceDataSize = 0;
			CMP_Texture compressedFaceTex{};
			compressedFaceTex.dwSize = sizeof(CMP_Texture);
			compressedFaceTex.dwWidth = SkyboxDimensions;
			compressedFaceTex.dwHeight = SkyboxDimensions;
			compressedFaceTex.dwPitch = 0;
			compressedFaceTex.format = PBFormatToCMPFormat(PB::ETextureFormat::BC6H_RGB_U16F);
			compressedFaceTex.dwDataSize = compressedFaceDataSize = CMP_CalculateBufferSize(&compressedFaceTex);

			readbackFaceSize = compress ? compressedFaceDataSize : readbackFaceSize;

			const uint32_t skyboxFaceAlign = 1024;
			const uint32_t roundedSize = readbackFaceSize + (readbackFaceSize % skyboxFaceAlign);
			EnvironmentMapMetadata* metaData = nullptr;
			PB::u8* outData = reinterpret_cast<PB::u8*>(m_dbWriter->AllocateAsset(skyboxName.c_str(), sizeof(EnvironmentMapMetadata), roundedSize * 6, asset.m_lastModifiedTime, reinterpret_cast<char**>(&metaData)));

			memset(metaData->m_mipmapAlignedSizes, 0, sizeof(EnvironmentMapMetadata::m_mipmapAlignedSizes));
			metaData->m_mipmapAlignedSizes[0] = roundedSize;
			metaData->m_width = SkyboxDimensions;
			metaData->m_height = SkyboxDimensions;
			metaData->m_mipCount = 1;
			metaData->m_mapType = EConvolutedMapType::SKY;
			metaData->m_arrayLayerSize = readbackFaceSize;
			metaData->m_compressed = compress;

			CMP_CompressOptions compressOptions{};
			compressOptions.dwSize = sizeof(CMP_CompressOptions);
			compressOptions.fquality = 0.05f;
			compressOptions.dwnumThreads = 4;
			compressOptions.nCompressionSpeed = CMP_Speed_SuperFast;

			for (uint32_t face = 0; face < 6; ++face)
			{
				PB::ITexture* readback = skyReadbackTextures[face];
				PB::u8* mappedData = readback->MapReadback();

				if (compress)
				{
					printf_s("%s:	> Compressing Skybox face: %u...\n", m_name.c_str(), face);

					srcFace.pData = mappedData;
					compressedFaceTex.pData = &outData[face * roundedSize];
					CMP_ConvertTexture(&srcFace, &compressedFaceTex, &compressOptions, {});
				}
				else
				{
					memcpy(&outData[face * roundedSize], mappedData, readbackFaceSize);
				}

				readback->UnmapReadback();
			}
		}

		// Irradiance map asset:
		{
			std::string irradianceMapName = asset.m_dbPath + "_env_irradiance";

			// All faces will have the same size and align, so just retrieve from the first.
			// Don't bother compressing the irradiance map, it should already be very small.
			PB::u32 readbackFaceSize;
			PB::u32 readbackFaceAlign;
			irradianceReadbackTextures[0]->GetMemorySizeAndAlign(readbackFaceSize, readbackFaceAlign);

			const uint32_t irradianceFaceAlign = 1024;
			const uint32_t roundedSize = readbackFaceSize + (readbackFaceSize % irradianceFaceAlign);
			EnvironmentMapMetadata* metaData = nullptr;
			PB::u8* outData = reinterpret_cast<PB::u8*>(m_dbWriter->AllocateAsset(irradianceMapName.c_str(), sizeof(EnvironmentMapMetadata), roundedSize * 6, asset.m_lastModifiedTime, reinterpret_cast<char**>(&metaData)));

			memset(metaData->m_mipmapAlignedSizes, 0, sizeof(EnvironmentMapMetadata::m_mipmapAlignedSizes));
			metaData->m_mipmapAlignedSizes[0] = roundedSize;
			metaData->m_width = IrradianceMapDimensions;
			metaData->m_height = IrradianceMapDimensions;
			metaData->m_mipCount = 1;
			metaData->m_mapType = EConvolutedMapType::IRRADIANCE;
			metaData->m_arrayLayerSize = readbackFaceSize;
			metaData->m_compressed = false;

			for (uint32_t face = 0; face < 6; ++face)
			{
				PB::ITexture* readback = irradianceReadbackTextures[face];
				PB::u8* mappedData = readback->MapReadback();
				memcpy(&outData[face * roundedSize], mappedData, readbackFaceSize);
				readback->UnmapReadback();
			}
		}

		// Prefilter map asset:
		{
			const uint32_t prefilterSubresourceAlign = 4096;

			CMP_Texture srcFace{};
			srcFace.dwSize = sizeof(CMP_Texture);
			srcFace.dwDataSize = 0;
			srcFace.dwWidth = 0;
			srcFace.dwHeight = 0;
			srcFace.dwPitch = 0;
			srcFace.format = CMP_FORMAT_ARGB_16F;

			PB::u32 prefilterTotalSize = 0;
			PB::u32 mipChainSize = 0;
			PB::u32 mipSizes[ConvolutionMipmapCount];
			PB::u32 mipAlignedSizes[ConvolutionMipmapCount];

			CMP_Texture subresourceTex{};
			subresourceTex.dwSize = sizeof(CMP_Texture);
			subresourceTex.format = PBFormatToCMPFormat(PB::ETextureFormat::BC6H_RGB_U16F);
			subresourceTex.dwWidth = 0;
			subresourceTex.dwHeight = 0;
			subresourceTex.dwPitch = 0;
			for (uint32_t layer = 0; layer < 6; ++layer)
			{
				for (uint32_t mip = 0; mip < ConvolutionMipmapCount; ++mip)
				{
					PB::ITexture* readback = prefilterReadbackTextures[(layer * ConvolutionMipmapCount) + mip];

					PB::u32 readbackSize = 0;
					PB::u32 readbackAlign = 0;
					readback->GetMemorySizeAndAlign(readbackSize, readbackAlign);

					if (layer == 0)
					{
						mipSizes[mip] = readbackSize;
					}

					if (compress)
					{
						subresourceTex.dwWidth = PrefilterMapDimensions >> mip;
						subresourceTex.dwHeight = PrefilterMapDimensions >> mip;

						readbackSize = CMP_CalculateBufferSize(&subresourceTex);
					}

					const uint32_t roundedSize = readbackSize + (readbackSize % prefilterSubresourceAlign);
					prefilterTotalSize += roundedSize;

					if (layer == 0)
					{
						mipAlignedSizes[mip] = roundedSize;
						mipChainSize += roundedSize;
					}
				}
			}

			std::string prefilterMapName = asset.m_dbPath + "_env";
			EnvironmentMapMetadata* metaData = nullptr;
			PB::u8* outData = reinterpret_cast<PB::u8*>(m_dbWriter->AllocateAsset(prefilterMapName.c_str(), sizeof(EnvironmentMapMetadata), prefilterTotalSize, asset.m_lastModifiedTime, reinterpret_cast<char**>(&metaData)));

			memcpy(metaData->m_mipmapAlignedSizes, mipAlignedSizes, sizeof(uint32_t) * ConvolutionMipmapCount);
			metaData->m_width = PrefilterMapDimensions;
			metaData->m_height = PrefilterMapDimensions;
			metaData->m_arrayLayerSize = mipChainSize;
			metaData->m_mipCount = ConvolutionMipmapCount;
			metaData->m_mapType = EConvolutedMapType::PREFILTER;
			metaData->m_compressed = compress;

			CMP_CompressOptions compressOptions{};
			compressOptions.dwSize = sizeof(CMP_CompressOptions);
			compressOptions.fquality = 0.05f;
			compressOptions.dwnumThreads = 16;

			for (uint32_t layer = 0; layer < 6; ++layer)
			{
				PB::u32 mipOffset = 0;
				for (uint32_t mip = 0; mip < ConvolutionMipmapCount; ++mip)
				{
					PB::ITexture* readback = prefilterReadbackTextures[(layer * ConvolutionMipmapCount) + mip];
					PB::u8* mappedData = readback->MapReadback();

					PB::u32 offset = (layer * mipChainSize) + mipOffset;

					if (compress)
					{
						printf_s("%s:	> Compressing Prefilter subresource [Mip:%u , Layer:%u]...\n", m_name.c_str(), mip, layer);

						srcFace.dwWidth = PrefilterMapDimensions >> mip;
						srcFace.dwHeight = PrefilterMapDimensions >> mip;
						srcFace.dwDataSize = mipSizes[mip];
						srcFace.pData = mappedData;

						subresourceTex.dwWidth = srcFace.dwWidth;
						subresourceTex.dwHeight = srcFace.dwHeight;
						subresourceTex.dwDataSize = CMP_CalculateBufferSize(&subresourceTex);
						subresourceTex.pData = &outData[offset];

						CMP_ConvertTexture(&srcFace, &subresourceTex, &compressOptions, {});
					}
					else
					{
						memcpy(&outData[offset], mappedData, mipSizes[mip]);
					}

					readback->UnmapReadback();

					mipOffset += mipAlignedSizes[mip];
				}
			}
		}

		for (auto& skyReadback : skyReadbackTextures)
		{
			m_renderer->FreeTexture(skyReadback);
			skyReadback = nullptr;
		}
		skyReadbackTextures.Clear();

		for (auto& irradianceReadback : irradianceReadbackTextures)
		{
			m_renderer->FreeTexture(irradianceReadback);
			irradianceReadback = nullptr;
		}
		irradianceReadbackTextures.Clear();

		for (auto& prefilterReadback : prefilterReadbackTextures)
		{
			m_renderer->FreeTexture(prefilterReadback);
			prefilterReadback = nullptr;
		}
		prefilterReadbackTextures.Clear();

		printf("%s: Encoded environment map: %s\n", m_name.c_str(), asset.m_dbPath.c_str());
	}
};
