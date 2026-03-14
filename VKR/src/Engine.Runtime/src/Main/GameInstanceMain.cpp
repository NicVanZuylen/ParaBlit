#include "GameInstanceMain.h"

#include "Engine.Control/IDataClass.h"
#include "Engine.Control/IDataFile.h"
#include "TimeMain.h"

#include "CLib/Allocator.h"

#include "Editor/EditorMain.h"

#include "RenderGraphPasses/DebugLinePass.h"
#include "RenderGraphPasses/TextRenderPass.h"

#include "Utility/Input.h"

#include "Resource/FontTexture.h"
#include "Resource/Shader.h"

#include "WorldRender/DrawBatch.h"

#include "Entity/Component/Transform.h"
#include "Entity/Component/StaticEntityTracker.h"

#include <sstream>
#include <iostream>
#include <filesystem>

namespace Eng
{
	GameInstanceMain::GameInstanceMain(Input* input, PB::IRenderer* renderer, CLib::Allocator* allocator)
	{
		m_input = input;
		m_renderer = renderer;
		m_swapchain = m_renderer->GetSwapchain();
		m_allocator = allocator;

		std::string dbDir = std::filesystem::current_path().string();
		std::string databasePath = dbDir + "/Assets/build";
		m_assetStreamer.Init(m_renderer, databasePath.c_str());

		InitResources();
		InitializeEntities();

		if constexpr (ENG_EDITOR)
		{
			m_editor = m_allocator->Alloc<EditorMain>(m_renderer, m_allocator, m_entityRoot, m_hierarchy, &m_gameRenderer);
		}
		else
		{
			m_hierarchy->SetSimulationEnable(true);
		}

		m_gameRenderer.Init(m_renderer, m_allocator, m_hierarchy, m_camera, m_editor);
		m_gameRenderer.InitResources();
		m_gameRenderer.SetCamera(m_camera);

		UpdateResolution(m_swapchain->GetWidth(), m_swapchain->GetHeight());
	}

	GameInstanceMain::~GameInstanceMain()
	{
		m_gameRenderer.DestroyResources();

		DestroyResources();
		m_hierarchy->Destroy();
		m_assetStreamer.Shutdown();

		if (m_editor)
		{
			m_allocator->Free(m_editor);
		}

		// ====================================================================================
		m_entityDataFile->Close();
		Ctrl::IDataFile::Destroy(m_entityDataFile);
		// ====================================================================================
	}

	void GameInstanceMain::Update(GLFWwindow* window, bool updateMetrics)
	{
		float renderStallTime = (g_timeMain.RenderStallTime());

		// Update Text ---------------------------------------------------------------------------------------------------
		{
			TextRenderPass* textPass = m_gameRenderer.GetTextPass();
			if (!m_cpuTimeText)
			{
				m_cpuTimeText = textPass->AddText("CPU Time: 000000ms", m_fontTexture, PB::Float2(0.0f, 0.0f));
				m_fpsText = textPass->AddText("FPS: 000000", m_fontTexture, PB::Float2(0.0f, float(m_fontTexture->GetFontHeight())));

				if constexpr (ENG_EDITOR)
				{
					m_selectedEntityText = textPass->AddText("Selected Entity: None", m_fontTexture, PB::Float2(0.0f, 2.0f * float(m_fontTexture->GetFontHeight())));
				}
			}
			else if (updateMetrics)
			{
				PB::Float2 anchorPos(20.0f, 20.0f);

				Vector2u worldRenderResolution = m_gameRenderer.GetWorldRenderResolution();

				float worldViewportWidthf = static_cast<float>(worldRenderResolution.x);
				float worldViewportHeightf = static_cast<float>(worldRenderResolution.y);
				float fontHeightf = static_cast<float>(m_fontTexture->GetFontHeight());
				float anchorHeight = worldViewportHeightf - fontHeightf - anchorPos.y;

				float mainDeltaTime(g_timeMain.DeltaTimeMain());

				std::ostringstream str;
				str.precision(3);
				str << "CPU Time: " << ((mainDeltaTime * 1000.0f) - renderStallTime) << "ms";

				textPass->TextReplace(m_cpuTimeText, str.str().c_str(), PB::Float2(anchorPos.x, anchorHeight));

				str = std::ostringstream();
				str << "FPS: " << (1.0f / mainDeltaTime);

				textPass->TextReplace(m_fpsText, str.str().c_str(), PB::Float2(anchorPos.x, anchorHeight - fontHeightf));

				if constexpr (ENG_EDITOR)
				{
					str = std::ostringstream();
					str << "Selected Entity: ";
					if (m_editor->GetSelectedEntity() != nullptr)
						str << m_editor->GetSelectedEntity()->GetName();
					else
						str << "None";

					textPass->TextReplace(m_selectedEntityText, str.str().c_str(), PB::Float2(anchorPos.x, anchorHeight - (fontHeightf * 2.0f)));
				}
			}
		}

		// Update Dynamic Entities -------------------------------------------------------------------------------------------------
		m_hierarchy->SimUpdate();
		// -------------------------------------------------------------------------------------------------------------------------
	}

	void GameInstanceMain::Render(GLFWwindow* window, const float interpT)
	{
		// Editor ---------------------------------------------------------------------------------------------------------
		if constexpr (ENG_EDITOR)
		{
			m_editor->EditorUpdate(window, m_camera);
			m_editor->UpdateGUI(m_gameRenderer.GetWorldRenderOutputData());

			if (m_input->GetKey(KEYBOARD_KEY_LEFT_CONTROL, INPUTSTATE_CURRENT) && m_input->GetKeyPressed(KEYBOARD_KEY_S))
			{
				printf("Saving Data...\n");
				m_editor->SaveChanges();
				m_hierarchy->SaveState(m_entityDataFile, m_entityRoot);
				printf(" Saved!\n");
			}
		}
		// ---------------------------------------------------------------------------------------------------------------

		// ---------------------------------------------------------------------------------------------------------------
		// Debug Render
		DebugLinePass* debugLinePass = m_gameRenderer.GetDebugLinePass();

		if (!m_input->GetKey(KEYBOARD_KEY_PAGE_UP, INPUTSTATE_CURRENT) && m_input->GetKey(KEYBOARD_KEY_PAGE_UP, INPUTSTATE_PREVIOUS) && m_hierarchyTreeDrawDebugDepth < 50)
		{
			++m_hierarchyTreeDrawDebugDepth;
			std::cout << "BVH: Drawing at depth: " << m_hierarchyTreeDrawDebugDepth << "\n";
		}

		if (!m_input->GetKey(KEYBOARD_KEY_PAGE_DOWN, INPUTSTATE_CURRENT) && m_input->GetKey(KEYBOARD_KEY_PAGE_DOWN, INPUTSTATE_PREVIOUS) && m_hierarchyTreeDrawDebugDepth > 0)
		{
			--m_hierarchyTreeDrawDebugDepth;
			std::cout << "BVH: Drawing at depth: " << m_hierarchyTreeDrawDebugDepth << "\n";
		}

		if (!m_input->GetKey(KEYBOARD_KEY_HOME, INPUTSTATE_CURRENT) && m_input->GetKey(KEYBOARD_KEY_HOME, INPUTSTATE_PREVIOUS))
		{
			m_drawEntireHierarchyTree = !m_drawEntireHierarchyTree;
			std::cout << "BVH: Drawing whole tree: " << (m_drawEntireHierarchyTree ? "true" : "false") << "\n";
		}

		if (!m_input->GetKey(KEYBOARD_KEY_INSERT, INPUTSTATE_CURRENT) && m_input->GetKey(KEYBOARD_KEY_INSERT, INPUTSTATE_PREVIOUS))
		{
			m_hierarchyTreeDrawDebugDepth = m_hierarchyTreeDrawDebugDepth == ~uint32_t(0) ? 0 : ~uint32_t(0);
			std::cout << "BVH: Drawing whole tree: " << (m_drawEntireHierarchyTree ? "true" : "false") << "\n";
		}

		if (m_drawEntireHierarchyTree)
		{
			m_hierarchy->GetEntityBoundingVolumeHierarchy().DebugDraw(m_camera, debugLinePass, m_hierarchyTreeDrawDebugDepth, true);
			m_hierarchy->GetEntitySpatialHashTable().DebugDrawCells(debugLinePass);
		}

		if (m_input->GetKey(KEYBOARD_KEY_E, INPUTSTATE_CURRENT))
		{
			m_hierarchy->GetEntityBoundingVolumeHierarchy().DebugDraw(m_camera, debugLinePass, m_hierarchyTreeDrawDebugDepth, true);
		}
		// ---------------------------------------------------------------------------------------------------------------

		// ---------------------------------------------------------------------------------------------------------------
		// Camera Updates
		if constexpr (ENG_EDITOR == 0)
		{
			m_camera->UpdateFreeCam(g_timeMain.DeltaTimeMain(), m_input, window);
		}
		// ---------------------------------------------------------------------------------------------------------------

		// ---------------------------------------------------------------------------------------------------------------
		// Render
		{
			m_hierarchy->RenderUpdate(interpT);
			m_gameRenderer.EndFrame(g_timeMain.DeltaTimeMain());
		}
		// ---------------------------------------------------------------------------------------------------------------

		m_assetStreamer.NextFrame();
	}

	void GameInstanceMain::UpdateResolution(uint32_t width, uint32_t height)
	{
		Vector2f mouseRegionOrigin(0.0f);
		Vector2u resolution(width, height);
		if (m_editor)
		{
			m_editor->UpdateResolution();
			resolution = m_editor->GetViewportResolution();

			Vector2u mouseRegionOriginU = m_editor->GetViewportOrigin();
			mouseRegionOrigin.x = float(mouseRegionOriginU.x);
			mouseRegionOrigin.y = float(mouseRegionOriginU.y);

			printf("Viewport origin: [%u, %u]\n", mouseRegionOriginU.x, mouseRegionOriginU.y);
			printf("Viewport resolution: [%u, %u]\n", resolution.x, resolution.y);
		}

		m_input->SetMouseRegion(mouseRegionOrigin, Vector2f(float(resolution.x), float(resolution.y)));

		m_camera->SetWidth(float(resolution.x));
		m_camera->SetHeight(float(resolution.y));

		m_gameRenderer.UpdateResolution(resolution.x, resolution.y);
	}

	void GameInstanceMain::InitResources()
	{
		m_fontTexture = m_allocator->Alloc<Eng::FontTexture>(m_renderer, "Assets/Fonts/arial.ttf", 24);
	}

	void GameInstanceMain::DestroyResources()
	{
		if (m_fontTexture != nullptr)
		{
			m_allocator->Free(m_fontTexture);
			m_fontTexture = nullptr;
		}
	}

	void GameInstanceMain::InitializeEntities()
	{
		// ====================================================================================
		{
			Ctrl::IDataFile* assetImportsFile = Ctrl::IDataFile::Create();
			bool allowFail = (ENG_EDITOR != 0);

			if(assetImportsFile->Open("Assets/assetImports.exml", Ctrl::IDataFile::EOpenMode::OPEN_READ_ONLY, allowFail))
			{
				Ctrl::DataClass::InstantiateNodeTree(assetImportsFile->GetRoot());
				
				assetImportsFile->Close();
			}
			Ctrl::IDataFile::Destroy(assetImportsFile);
		}
		// ====================================================================================
		m_entityDataFile = Ctrl::IDataFile::Create();
		auto entityFileStatus = m_entityDataFile->Open("Assets/entityData.exml", Ctrl::IDataFile::EOpenMode::OPEN_READ_WRITE, true);
		if(entityFileStatus != Ctrl::IDataFile::EFileStatus::OPENED_SUCCESSFULLY)
		{
			assert(false && "Failed to open entityData.exml file. Level will not be loaded!");
		}
		// ====================================================================================

		m_entityRoot = m_entityDataFile->GetRoot()->GetOrAddDataNode("Base");
		DataClass::InstantiateNodeTree(m_entityRoot);

		Ctrl::IDataNode* hierarchyNode = m_entityRoot->GetDataNode("EntityHierarchy");
		if (hierarchyNode == nullptr)
		{
			m_hierarchy = CLib::Reflection::InstantiateClass<Entity>("EntityHierarchy");
		}
		else
		{
			m_hierarchy = TObjectPtr<EntityHierarchy>(hierarchyNode->GetSelfGUID());
		}

		m_hierarchy->Init(m_entityRoot, m_allocator, m_renderer, &m_assetStreamer);
		m_hierarchy->BakeTrees();

		if (hierarchyNode == nullptr)
		{
			m_hierarchy->SaveToDataNode(m_entityRoot->AddDataNode("EntityHierarchy", "EntityHierarchy"));
		}

		const char* cameraName = "MainCamera";

		m_cameraEntity = m_hierarchy->FindEntity(cameraName);
		if(m_cameraEntity == nullptr)
		{
			m_cameraEntity = m_hierarchy->CreateEntity(EEntityUpdateMethod::DYNAMIC, false);
			m_cameraEntity->Rename(cameraName);

			m_cameraEntity->GetComponent<Transform>()->SetPosition(Vector3f(0.0f, 10.0f, 15.0f));
			m_cameraEntity->GetComponent<Transform>()->SetRotation(Quaternion().RotatedX(-35.0f).RotatedY(0.0f).RotatedZ(0.0f));

			Camera::CreateDesc cameraDesc;
			cameraDesc.m_sensitivity = 0.5f;
			cameraDesc.m_moveSpeed = 100.0f;
			cameraDesc.m_width = 1280.0f;
			cameraDesc.m_height = 720.0f;
			cameraDesc.m_fovY = 45.0f;
			cameraDesc.m_zFar = 2000.0f;
			
			m_camera = m_cameraEntity->AddComponent<Camera>(cameraDesc);
			m_camera->SaveToDataNode(m_entityRoot->AddDataNode("Camera", "Camera"));
		}
		else
		{
			m_camera = m_cameraEntity->GetComponent<Camera>();
		}
	}
};