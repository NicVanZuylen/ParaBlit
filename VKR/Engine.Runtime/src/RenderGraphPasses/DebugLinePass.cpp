#include "DebugLinePass.h"
#include "RenderGraph/RenderGraph.h"
#include "Resource/Shader.h"

namespace Eng
{
	using namespace Math;

	DebugLinePass::DebugLinePass(PB::IRenderer* renderer, CLib::Allocator* allocator, PB::UniformBufferView mvpView) : RenderGraphBehaviour(renderer, allocator)
	{
		m_mvpView = mvpView;

		PB::BufferObjectDesc lineVertexDesc{};
		lineVertexDesc.m_bufferSize = sizeof(LineVertex) * 2 * MaxLineCount;
		lineVertexDesc.m_options = 0;
		lineVertexDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::VERTEX;
		m_lineVertexBuffer = m_renderer->AllocateBuffer(lineVertexDesc);

		lineVertexDesc.m_options = PB::EBufferOptions::CPU_ACCESSIBLE;
		lineVertexDesc.m_usage = PB::EBufferUsage::COPY_SRC;
		m_localLineVertexBuffer = m_renderer->AllocateBuffer(lineVertexDesc);
	}

	DebugLinePass::~DebugLinePass()
	{
		if (m_localLineVertexBuffer != nullptr)
		{
			m_renderer->FreeBuffer(m_localLineVertexBuffer);
			m_localLineVertexBuffer = nullptr;
		}

		if (m_lineVertexBuffer != nullptr)
		{
			m_renderer->FreeBuffer(m_lineVertexBuffer);
			m_lineVertexBuffer = nullptr;
		}
	}

	void DebugLinePass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		if (m_mappedLocalLineVertexBuffer && m_activeLineCount > 0)
		{
			PB::CopyRegion region{};
			region.m_srcOffset = 0;
			region.m_dstOffset = 0;
			region.m_size = sizeof(LineVertex) * 2 * m_activeLineCount;

			info.m_commandContext->CmdCopyBufferToBuffer(m_localLineVertexBuffer, m_lineVertexBuffer, &region, 1);
			m_localLineVertexBuffer->Unmap();
			m_mappedLocalLineVertexBuffer = nullptr;
		}
	}

	void DebugLinePass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		if (!m_linePipeline)
		{
			PB::GraphicsPipelineDesc linePipelineDesc{};
			linePipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = Eng::Shader(m_renderer, "Shaders/GLSL/vs_debug_line", m_allocator, true).GetModule();
			linePipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = Eng::Shader(m_renderer, "Shaders/GLSL/fs_debug_line", m_allocator, true).GetModule();
			linePipelineDesc.m_attachmentCount = 1;
			linePipelineDesc.m_cullMode = PB::EFaceCullMode::NONE;
			linePipelineDesc.m_depthCompareOP = PB::ECompareOP::LEQUAL;
			linePipelineDesc.m_topology = PB::EPrimitiveTopologyType::LINE_LIST;
			linePipelineDesc.m_vertexBuffers[0].m_type = PB::EVertexBufferType::VERTEX;
			linePipelineDesc.m_vertexBuffers[0].m_vertexSize = sizeof(LineVertex);
			linePipelineDesc.m_vertexDesc.vertexAttributes[0].m_attribute = PB::EVertexAttributeType::FLOAT4;
			linePipelineDesc.m_vertexDesc.vertexAttributes[0].m_buffer = 0;
			linePipelineDesc.m_vertexDesc.vertexAttributes[1].m_attribute = PB::EVertexAttributeType::FLOAT4;
			linePipelineDesc.m_vertexDesc.vertexAttributes[1].m_buffer = 0;
			linePipelineDesc.m_depthWriteEnable = false;
			linePipelineDesc.m_stencilTestEnable = false;
			linePipelineDesc.m_renderArea = { 0, 0, 0, 0 };
			linePipelineDesc.m_renderPass = info.m_renderPass;
			linePipelineDesc.m_lineThickness = 2.0f;

			m_linePipeline = m_renderer->GetPipelineCache()->GetPipeline(linePipelineDesc);
		}

		if (m_activeLineCount)
		{
			PB::ISwapChain* swapchain = info.m_renderer->GetSwapchain();
			uint32_t renderWidth = swapchain->GetWidth();
			uint32_t renderHeight = swapchain->GetHeight();

			info.m_commandContext->CmdBindPipeline(m_linePipeline);
			info.m_commandContext->CmdSetViewport({ 0, 0, renderWidth, renderHeight }, 0.0f, 1.0f);
			info.m_commandContext->CmdSetScissor({ 0, 0, renderWidth, renderHeight });

			PB::BindingLayout bindings{};
			bindings.m_uniformBufferCount = 1;
			bindings.m_uniformBuffers = &m_mvpView;
			info.m_commandContext->CmdBindResources(bindings);

			info.m_commandContext->CmdBindVertexBuffer(m_lineVertexBuffer, nullptr, PB::EIndexType::PB_INDEX_TYPE_UINT32);
			info.m_commandContext->CmdDraw(m_activeLineCount * 2, 1);
			m_activeLineCount = 0;
		}
	}

	void DebugLinePass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
	}

	void DebugLinePass::AddToRenderGraph(RenderGraphBuilder* builder)
	{
		NodeDesc nodeDesc{};
		nodeDesc.m_behaviour = this;
		nodeDesc.m_useReusableCommandLists = false;
		nodeDesc.m_computeOnlyPass = false;
		nodeDesc.m_renderWidth = m_renderer->GetSwapchain()->GetWidth();
		nodeDesc.m_renderHeight = m_renderer->GetSwapchain()->GetHeight();

		AttachmentDesc& targetDesc = nodeDesc.m_attachments.PushBackInit();
		targetDesc.m_format = m_renderer->GetSwapchain()->GetImageFormat();
		targetDesc.m_width = nodeDesc.m_renderWidth;
		targetDesc.m_height = nodeDesc.m_renderHeight;
		targetDesc.m_name = "MergedOutput";
		targetDesc.m_usage = PB::EAttachmentUsage::COLOR;
		targetDesc.m_flags = EAttachmentFlags::NONE;

		AttachmentDesc& depthDesc = nodeDesc.m_attachments.PushBackInit();
		depthDesc.m_format = PB::ETextureFormat::D24_UNORM_S8_UINT;
		depthDesc.m_width = nodeDesc.m_renderWidth;
		depthDesc.m_height = nodeDesc.m_renderHeight;
		depthDesc.m_name = "G_Depth";
		depthDesc.m_usage = PB::EAttachmentUsage::READ_ONLY_DEPTHSTENCIL;
		depthDesc.m_flags = EAttachmentFlags::NONE;

		builder->AddNode(nodeDesc);
	}

	void DebugLinePass::DrawLine(PB::Float3 startPoint, PB::Float3 endPoint, PB::Float3 color)
	{
		DrawLine
		(
			PB::Float4(startPoint.x, startPoint.y, startPoint.z, 1.0f),
			PB::Float4(endPoint.x, endPoint.y, endPoint.z, 1.0f),
			PB::Float4(color.r, color.g, color.b, 1.0f)
		);
	}

	void DebugLinePass::DrawLine(Vector3f startPoint, Vector3f endPoint, Vector3f color)
	{
		DrawLine
		(
			PB::Float4(startPoint.x, startPoint.y, startPoint.z, 1.0f),
			PB::Float4(endPoint.x, endPoint.y, endPoint.z, 1.0f),
			PB::Float4(color.r, color.g, color.b, 1.0f)
		);
	}

	void DebugLinePass::DrawLine(PB::Float4 startPoint, PB::Float4 endPoint, PB::Float4 color)
	{
		if (m_mappedLocalLineVertexBuffer == nullptr)
			m_mappedLocalLineVertexBuffer = reinterpret_cast<LineVertex*>(m_localLineVertexBuffer->Map(0, sizeof(LineVertex) * 2 * MaxLineCount));

		uint32_t idx = m_activeLineCount * 2;
		m_mappedLocalLineVertexBuffer[idx] =
		{
			startPoint,
			color
		};
		m_mappedLocalLineVertexBuffer[idx + 1] =
		{
			endPoint,
			color
		};
		++m_activeLineCount;
	}
};