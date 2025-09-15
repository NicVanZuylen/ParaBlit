#pragma once
#include "GameRenderer.h"

#include "Entity/Entity.h"
#include "Entity/EntityHierarchy.h"

#include "Resource/AssetStreamer.h"

#include <Engine.Math/Scalar.h>
#include <Engine.Math/Vectors.h>
#include <Engine.Math/Matrix4.h>
#include <Engine.Math/Quaternion.h>

struct GLFWwindow;

namespace CLib
{
	class Allocator;
}

namespace Eng
{
	class Texture;
	class Mesh;
	class Shader;
	class FontTexture;
	class Material;
	class Input;
	class RenderGraph;

	class GameInstanceMain
	{
	public:

		GameInstanceMain(Input* input, PB::IRenderer* renderer, CLib::Allocator* allocator);
		~GameInstanceMain();

		void Update(GLFWwindow* window, float deltaTime, float elapsedTime, float stallTime, bool updateMetrics);

		void UpdateResolution(uint32_t width, uint32_t height);

	private:

		// -------------------------------------------------------------------------
		// Helper Functions
		inline void InitResources();
		inline void DestroyResources();

		inline void InitializeEntities();
		// -------------------------------------------------------------------------

		// -------------------------------------------------------------------------
		// Essential
		Input* m_input = nullptr;
		PB::IRenderer* m_renderer = nullptr;
		PB::ISwapChain* m_swapchain = nullptr;
		CLib::Allocator* m_allocator = nullptr;
		GameRenderer m_gameRenderer{};

		class EditorMain* m_editor = nullptr;

		// -------------------------------------------------------------------------
		// Resources

		AssetStreamer m_assetStreamer;

		Eng::FontTexture* m_fontTexture = nullptr;
		void* m_cpuTimeText = nullptr;
		void* m_fpsText = nullptr;
		void* m_selectedEntityText = nullptr;

		uint32_t m_hierarchyTreeDrawDebugDepth = 0;
		bool m_drawEntireHierarchyTree = false;

		// -------------------------------------------------------------------------
		// Entities

		Ctrl::IDataFile* m_entityDataFile = nullptr;
		Ctrl::IDataNode* m_entityRoot = nullptr;
		EntityHierarchy m_hierarchy;
		Entity* m_selectedEntity = nullptr;
		Entity* m_cameraEntity = nullptr;
		TObjectPtr<Camera> m_camera;

		CLib::Vector<Entity*> m_dynamicEntitiesTest;

		// -------------------------------------------------------------------------
	};

};