#pragma once
#include "Engine.Control/IDataFile.h"
#include "Engine.ParaBlit/ISwapChain.h"
#include "Engine.ParaBlit/IImGUIModule.h"
#include "Engine.Control/IDataClass.h"
#include "Engine.Reflectron/ReflectronAPI.h"
#include "RenderGraph/RenderGraph.h"

#include "AssetTree.h"
#include <unordered_map>
#include <unordered_set>

#define MATH_IMPL_IMGUI
#include "Engine.Math/Vector2.h"
#include "imgui.h"

#include <cstdint>

struct GLFWwindow;

namespace Eng
{
	class EntityHierarchy;
	class DebugLinePass;
	class Camera;
	class GameRenderer;
	class Entity;

	class EditorMain
	{
	public:

		EditorMain(PB::IRenderer* renderer, CLib::Allocator* allocator, const Ctrl::IDataNode* entityData, EntityHierarchy* hierarchy, GameRenderer* gameRenderer);

		~EditorMain();

		void UpdateResolution();

		void EditorUpdate(GLFWwindow* window, Camera* camera);

		void UpdateGUI(const PB::ImGuiTextureData& worldRenderOutput);

		void RenderGUI(PB::ICommandContext* cmdContext);

		void SaveChanges();

		Math::Vector2u GetViewportOrigin();
		Math::Vector2u GetViewportResolution();

		void SelectEntity(Entity* entity);
		Entity* GetSelectedEntity() { return m_selection.entity; }

	private:

		friend class AssetTree;

		void CreateRenderGraph();
		void BuildDCRefMappingsFromNode(const Ctrl::IDataNode* rootNode);
		void UpdateViewportEditor(Camera* camera);
		void ViewportImGUI(const PB::ImGuiTextureData* worldRenderOutput);

		/*
		Description: Draw a panel (e.g. Outliner, Inspector) with or without background to overwrite the previously drawn panel. Returns whether or not the panel contents are cleared and should be re-drawn.
		*/
		bool StartPanel(const char* name, Math::Vector2f position, Math::Vector2f scale, bool update, bool& focus, ImGuiWindowFlags additionalWindowFlags = 0);
		void EndPanel();

		void DataTableImGUI(Ctrl::ObjectPtr& dataClass, const Reflectron::Reflector& reflection, uint32_t& fieldId);

		void SelectDataClass(Ctrl::ObjectPtr dc);
		void RefreshReferences(Ctrl::ObjectPtr& ptr);
		void AssignDataClassRef(const Ctrl::ObjectPtr& srcRef);

		Math::Vector2f CenterRectPosition(const Math::Vector2f position, const Math::Vector2f& dimensions);

		using DCReferences = std::unordered_set<Ctrl::GUID>;
		// Map of all DataClass GUIDs (value) referencing unique DataClass GUID instances (key).
		// Used to find all DataClass instances referencing a specific DataClass instance as a field via TObjectPtr<T>.
		std::unordered_map<Ctrl::GUID, DCReferences> m_dataClassRefMappings;

		PB::IRenderer* m_renderer = nullptr;
		CLib::Allocator* m_allocator = nullptr;

		const Ctrl::IDataNode* m_entityData = nullptr;
		EntityHierarchy* m_hierarchy = nullptr;
		GameRenderer* m_gameRenderer = nullptr;

		RenderGraph* m_editorRenderGraph = nullptr;
		PB::ITexture* m_editorRenderOutput = nullptr;
		class ImGUIRenderPass* m_imguiRenderPass = nullptr;
		ImDrawData* m_drawData = nullptr;

		AssetTree* m_assetTree = nullptr;

		Math::Vector2f m_windowDimensions = Math::Vector2f(1280.0f, 720.0f);

		Math::Vector2f m_viewportPos{};
		Math::Vector2f m_viewportScale{};

		// Selection
		struct
		{
			Entity* entity = nullptr;
			ReflectronFieldData field{};
			Ctrl::ObjectPtr openDataClass;
			Ctrl::ObjectPtr openComponent;
			Ctrl::ObjectPtr* assignmentTarget = nullptr;
			std::string assignmentAssetTypename;
			bool runSelectFileDialogue = false;
		} m_selection{};

		// Viewport editor
		bool m_viewportFocused = false;

		// Panel updates
		uint32_t m_updateOutlinerPanelFrames = 1;
		bool m_outlinerPanelFocused = false;

		uint32_t m_updateInspectorPanelFrames = 1;
		bool m_inspectorPanelFocused = false;
	};
}