#include "GameInstanceMain.h"

#include "WindowHandle.h"

#include "CLib/Allocator.h"

#include "EditorMain.h"

#include "RenderGraphPasses/DebugLinePass.h"
#include "RenderGraphPasses/TextRenderPass.h"

#include "Utility/Input.h"

#include "Resource/FontTexture.h"
#include "Resource/Mesh.h"
#include "Resource/Shader.h"
#include "Resource/Material.h"

#include "WorldRender/DrawBatch.h"
#include "WorldRender/ObjectDispatcher.h"

#include "Entity/Component/Transform.h"
#include "Entity/Component/RenderDefinition.h"
#include "Entity/Component/StaticEntityTracker.h"

#include <sstream>
#include <iostream>
#include <random>
#include <filesystem>

#include "imgui.h"

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
			m_editor = m_allocator->Alloc<EditorMain>(m_renderer, m_allocator);
		}

		m_gameRenderer.Init(m_renderer, m_allocator, &m_hierarchy, m_camera, m_editor);
		m_gameRenderer.InitResources();

		UpdateResolution(m_swapchain->GetWidth(), m_swapchain->GetHeight());
	}

	GameInstanceMain::~GameInstanceMain()
	{
		m_gameRenderer.DestroyResources();

		DestroyResources();
		m_hierarchy.Destroy();
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

	void GameInstanceMain::Update(GLFWwindow* window, float deltaTime, float elapsedTime, float stallTime, bool updateMetrics)
	{
		// Update Text ---------------------------------------------------------------------------------------------------
		{
			TextRenderPass* textPass = m_gameRenderer.GetTextPass();
			if (!m_cpuTimeText)
			{
				m_cpuTimeText = textPass->AddText("CPU Time: 000000ms", m_fontTexture, PB::Float2(0.0f, 0.0f));
				m_fpsText = textPass->AddText("FPS: 000000", m_fontTexture, PB::Float2(0.0f, float(m_fontTexture->GetFontHeight())));
				m_selectedEntityText = textPass->AddText("Selected Entity: None", m_fontTexture, PB::Float2(0.0f, 2.0f * float(m_fontTexture->GetFontHeight())));
			}
			else if (updateMetrics)
			{
				PB::Float2 anchorPos(20.0f, 20.0f);

				Vector2u worldRenderResolution = m_gameRenderer.GetWorldRenderResolution();

				float worldViewportWidthf = static_cast<float>(worldRenderResolution.x);
				float worldViewportHeightf = static_cast<float>(worldRenderResolution.y);
				float fontHeightf = static_cast<float>(m_fontTexture->GetFontHeight());
				float anchorHeight = worldViewportHeightf - fontHeightf - anchorPos.y;

				std::ostringstream str;
				str.precision(3);
				str << "CPU Time: " << ((deltaTime * 1000.0f) - stallTime) << "ms";

				textPass->TextReplace(m_cpuTimeText, str.str().c_str(), PB::Float2(anchorPos.x, anchorHeight));

				str = std::ostringstream();
				str << "FPS: " << (1.0f / deltaTime);

				textPass->TextReplace(m_fpsText, str.str().c_str(), PB::Float2(anchorPos.x, anchorHeight - fontHeightf));

				str = std::ostringstream();
				str << "Selected Entity: ";
				if (m_selectedEntity != nullptr)
					str << m_selectedEntity->GetName();
				else
					str << "None";

				textPass->TextReplace(m_selectedEntityText, str.str().c_str(), PB::Float2(anchorPos.x, anchorHeight - (fontHeightf * 2.0f)));
			}
		}

		DebugLinePass* debugLinePass = m_gameRenderer.GetDebugLinePass();

		debugLinePass->DrawLine(PB::Float4(0.0f, 0.0f, 0.0f, 1.0f), PB::Float4(1.0f, 0.0f, 0.0f, 1.0f), PB::Float4(1.0f, 0.0f, 0.0f, 1.0f));
		debugLinePass->DrawLine(PB::Float4(0.0f, 0.0f, 0.0f, 1.0f), PB::Float4(0.0f, 1.0f, 0.0f, 1.0f), PB::Float4(0.0f, 1.0f, 0.0f, 1.0f));
		debugLinePass->DrawLine(PB::Float4(0.0f, 0.0f, 0.0f, 1.0f), PB::Float4(0.0f, 0.0f, 1.0f, 1.0f), PB::Float4(0.0f, 0.0f, 1.0f, 1.0f));

		if (m_selectedEntity != nullptr)
		{
			StaticEntityTracker* tracker = m_selectedEntity->GetComponent<StaticEntityTracker>();
			if (tracker)
			{
				Bounds bounds = tracker->GetBoundingBox();
				debugLinePass->DrawCube(bounds.m_origin, bounds.m_extents, Vector3f(0.0f, 1.0f, 0.0f));
			}
		}

		if (!m_input->GetKey(GLFW_KEY_PAGE_UP, INPUTSTATE_CURRENT) && m_input->GetKey(GLFW_KEY_PAGE_UP, INPUTSTATE_PREVIOUS) && m_hierarchyTreeDrawDebugDepth < 50)
		{
			++m_hierarchyTreeDrawDebugDepth;
			std::cout << "BVH: Drawing at depth: " << m_hierarchyTreeDrawDebugDepth << "\n";
		}

		if (!m_input->GetKey(GLFW_KEY_PAGE_DOWN, INPUTSTATE_CURRENT) && m_input->GetKey(GLFW_KEY_PAGE_DOWN, INPUTSTATE_PREVIOUS) && m_hierarchyTreeDrawDebugDepth > 0)
		{
			--m_hierarchyTreeDrawDebugDepth;
			std::cout << "BVH: Drawing at depth: " << m_hierarchyTreeDrawDebugDepth << "\n";
		}

		if (!m_input->GetKey(GLFW_KEY_HOME, INPUTSTATE_CURRENT) && m_input->GetKey(GLFW_KEY_HOME, INPUTSTATE_PREVIOUS))
		{
			m_drawEntireHierarchyTree = !m_drawEntireHierarchyTree;
			std::cout << "BVH: Drawing whole tree: " << (m_drawEntireHierarchyTree ? "true" : "false") << "\n";
		}

		if (!m_input->GetKey(GLFW_KEY_INSERT, INPUTSTATE_CURRENT) && m_input->GetKey(GLFW_KEY_INSERT, INPUTSTATE_PREVIOUS))
		{
			m_hierarchyTreeDrawDebugDepth = m_hierarchyTreeDrawDebugDepth == ~uint32_t(0) ? 0 : ~uint32_t(0);
			std::cout << "BVH: Drawing whole tree: " << (m_drawEntireHierarchyTree ? "true" : "false") << "\n";
		}

		if (m_drawEntireHierarchyTree)
			m_hierarchy.GetEntityBoundingVolumeHierarchy().DebugDraw(m_camera, debugLinePass, m_hierarchyTreeDrawDebugDepth, true);

		auto translateEntity = [&](Vector3f translation)
		{
			printf_s("Moving Entity: %s\n", m_selectedEntity->GetName());

			m_selectedEntity->GetComponent<Transform>()->Translate(translation);
		};

		bool entityMoved = false;
		if (m_input->GetKeyReleased(GLFW_KEY_N))
		{
			m_selectedEntity = m_hierarchy.CreateEntity(EEntityUpdateMethod::STATIC, true);
			m_selectedEntity->Rename("CreateNewEntityTest");

			m_selectedEntity->GetComponent<Transform>()->SetPosition(m_camera->Position() + (m_camera->Forward() * -15.0f));
			entityMoved = true;
		}
		if (m_input->GetKey(GLFW_KEY_UP) && m_selectedEntity != nullptr)
		{
			Vector3f translation = Vector3f(0.0f, 0.0f, -5.0f) * deltaTime;
			translateEntity(translation);
			entityMoved = true;
		}
		if (m_input->GetKey(GLFW_KEY_DOWN) && m_selectedEntity != nullptr)
		{
			Vector3f translation = Vector3f(0.0f, 0.0f, 5.0f) * deltaTime;
			translateEntity(translation);
			entityMoved = true;
		}
		if (m_input->GetKey(GLFW_KEY_LEFT) && m_selectedEntity != nullptr)
		{
			Vector3f translation = Vector3f(-5.0f, 0.0f, 0.0f) * deltaTime;
			translateEntity(translation);
			entityMoved = true;
		}
		if (m_input->GetKey(GLFW_KEY_RIGHT) && m_selectedEntity != nullptr)
		{
			Vector3f translation = Vector3f(5.0f, 0.0f, 0.0f) * deltaTime;
			translateEntity(translation);
			entityMoved = true;
		}
		if (m_input->GetKey(GLFW_KEY_Q) && m_selectedEntity != nullptr)
		{
			Vector3f translation = Vector3f(0.0f, 5.0f, 0.0f) * deltaTime;
			translateEntity(translation);
			entityMoved = true;
		}
		if (m_input->GetKey(GLFW_KEY_E) && m_selectedEntity != nullptr)
		{
			Vector3f translation = Vector3f(0.0f, -5.0f, 0.0f) * deltaTime;
			translateEntity(translation);
			entityMoved = true;
		}
		if (m_input->GetKey(GLFW_KEY_R) && m_selectedEntity != nullptr)
		{
			float rotation = 90.0f * deltaTime;
			m_selectedEntity->GetComponent<Transform>()->RotateEulerY(ToRadians(rotation));
			entityMoved = true;
		}

		if (entityMoved)
		{
			m_selectedEntity->GetComponent<StaticEntityTracker>()->UpdateEntity();
			m_hierarchy.UpdateTrees();
		}

		if (!m_input->GetKey(GLFW_KEY_DELETE, INPUTSTATE_CURRENT) && m_input->GetKey(GLFW_KEY_DELETE, INPUTSTATE_PREVIOUS) && m_selectedEntity != nullptr)
		{
			printf_s("Destroying Entity: %s\n", m_selectedEntity->GetName());

			m_hierarchy.DestroyEntity(m_selectedEntity);
			m_selectedEntity = nullptr;
		}

		if (m_input->GetKey(GLFW_KEY_E, INPUTSTATE_CURRENT))
		{
			m_hierarchy.GetEntityBoundingVolumeHierarchy().DebugDraw(m_camera, debugLinePass, m_hierarchyTreeDrawDebugDepth, true);
		}


		if (m_input->GetKey(GLFW_KEY_LEFT_CONTROL, INPUTSTATE_CURRENT) && m_input->GetKeyPressed(GLFW_KEY_S))
		{
			printf_s("Saving Data...");
			m_hierarchy.SaveState(m_entityDataFile, m_entityRoot);
			printf_s(" Saved!\n");
		}

		if (m_input->GetMouseButton(MOUSEBUTTON_LEFT, INPUTSTATE_PREVIOUS) && !m_input->GetMouseButton(MOUSEBUTTON_LEFT, INPUTSTATE_CURRENT))
		{
			Vector2f cursorScreenPos = Vector2f
			(
				m_input->GetCursorX(INPUTSTATE_CURRENT),
				m_input->GetCursorY(INPUTSTATE_CURRENT)
			);

			printf_s("Cursor: [%f, %f]\n", cursorScreenPos.x, cursorScreenPos.y);

			Vector3f cursorFarPlanePos = m_camera->GetCursorFarPlaneWorldPosition(Vector2f(cursorScreenPos.x, cursorScreenPos.y));
			Vector3f cursorNearPlanePos = m_camera->GetCursorNearPlaneWorldPosition(Vector2f(cursorScreenPos.x, cursorScreenPos.y));
			Vector3f rayDirection = cursorFarPlanePos - cursorNearPlanePos;

			auto* selectedEntityData = m_hierarchy.GetEntityBoundingVolumeHierarchy().RaycastGetObjectData(debugLinePass, cursorNearPlanePos, rayDirection);
			if (selectedEntityData)
			{
				m_selectedEntity = reinterpret_cast<const EntityBoundingVolumeHierarchy::ObjectData*>(selectedEntityData)->m_entity;
				printf_s("Selected Entity: %s\n", m_selectedEntity->GetName());
			}
			else
			{
				m_selectedEntity = nullptr;
				printf_s("No Entity Selected.\n");
			}
		}

		m_camera->UpdateFreeCam(deltaTime, m_input, window);

		for(uint32_t i = 0; i < m_dynamicEntitiesTest.Count(); ++i)
		{
			auto entity = m_dynamicEntitiesTest[i];
			auto transform = entity->GetComponent<Transform>();

			transform->SetScale(Vector3f(Math::Abs(Math::Sin(elapsedTime))));
			transform->RotateEulerY(Math::ToRadians(90.0f)* deltaTime);
		}

		// Update Dynamic Entities -------------------------------------------------------------------------------------------------
		m_hierarchy.DynamicUpdate();
		// -------------------------------------------------------------------------------------------------------------------------

		// Editor ---------------------------------------------------------------------------------------------------------
		if constexpr (ENG_EDITOR)
		{
			m_editor->UpdateGUI(m_gameRenderer.GetWorldRenderOutputData());
		}
		// ---------------------------------------------------------------------------------------------------------------

		// ---------------------------------------------------------------------------------------------------------------
		// Render
		{
			m_gameRenderer.EndFrame(deltaTime);
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

			printf_s("Viewport origin: [%u, %u]\n", mouseRegionOriginU.x, mouseRegionOriginU.y);
			printf_s("Viewport resolution: [%u, %u]\n", resolution.x, resolution.y);
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
		m_entityDataFile = Ctrl::IDataFile::Create();
		m_entityDataFile->Open("Assets/entityData.xml", Ctrl::IDataFile::EOpenMode::OPEN_READ_WRITE, true);
		// ====================================================================================

		m_entityRoot = m_entityDataFile->GetRoot()->GetOrAddDataNode("Base");
		m_hierarchy.Init(m_entityRoot, m_allocator, m_renderer, &m_assetStreamer);
		m_hierarchy.BakeTrees();

		const char* meshes[]
		{
			"Meshes/Objects/Stanford/Lucy",
			"Meshes/Objects/Stanford/Bunny",
			"Meshes/Objects/Stanford/mesh_dragon",
			"Meshes/Objects/Stanford/Buddha",
			"Meshes/Primitives/sphere",
		};

		const char* cameraName = "MainCamera";

		m_cameraEntity = m_hierarchy.FindEntity(cameraName);
		if(m_cameraEntity == nullptr)
		{
			m_cameraEntity = m_hierarchy.CreateEntity(EEntityUpdateMethod::DYNAMIC, false);
			m_cameraEntity->Rename(cameraName);

			m_cameraEntity->GetComponent<Transform>()->SetPosition(Vector3f(0.0f, 10.0f, 15.0f));
			m_cameraEntity->GetComponent<Transform>()->SetRotation(Quaternion().RotatedX(-35.0f).RotatedY(0.0f).RotatedZ(0.0f));

			Camera::CreateDesc cameraDesc;
			cameraDesc.m_sensitivity = 0.5f;
			cameraDesc.m_moveSpeed = 100.0f;
			cameraDesc.m_width = 1280.0f;
			cameraDesc.m_height = 720.0f;
			cameraDesc.m_fovY = ToRadians(45.0f);
			cameraDesc.m_zFar = 2000.0f;
			
			m_camera = m_cameraEntity->AddComponent<Camera>(cameraDesc);
			m_camera->SaveToDataNode(m_entityRoot, m_entityRoot->AddDataNode("Camera", "Camera"));
		}
		else
		{
			m_camera = m_cameraEntity->GetComponent<Camera>();
		}
		m_gameRenderer.SetCamera(m_camera);

		for (uint32_t i = 0; i < 264; ++i)
		{
			auto newEntity = m_hierarchy.CreateEntity(EEntityUpdateMethod::DYNAMIC, true, meshes[i % 5]);
			newEntity->Rename("DynamicEntityTest");

			newEntity->GetComponent<Transform>()->Translate(Vector3f((i % 32) * 20.0f, 0.0f, 20.0f + ((i / 32) * 20.0f)));

			m_dynamicEntitiesTest.PushBack(newEntity);
		}
	}
};