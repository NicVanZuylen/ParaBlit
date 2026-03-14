#include "EditorMain.h"
#include "AssetTree.h"
#include "DynamicEntityTracker.h"
#include "Engine.Control/GUID.h"
#include "Engine.Control/IDataClass.h"
#include "Engine.Control/IDataFile.h"
#include "Engine.ParaBlit/ParaBlitImplUtil.h"
#include "FieldInterface.h"

#include "Input.h"
#include "RenderDefinition.h"
#include "TimeMain.h"
#include "GameRenderer.h"

#include "Entity/EntityHierarchy.h"
#include "Entity/Component/Transform.h"
#include "Entity/Component/Camera.h"
#include "Entity/Component/StaticEntityTracker.h"

#include "RenderGraphPasses/ImGUIRenderPass.h"
#include "RenderGraphPasses/DebugLinePass.h"

#include <cassert>
#include <sstream>
#include <iostream>

namespace Eng
{
	using namespace Math;

	EditorMain::EditorMain(PB::IRenderer* renderer, CLib::Allocator* allocator, const Ctrl::IDataNode* entityData, EntityHierarchy* hierarchy, GameRenderer* gameRenderer)
		: m_renderer(renderer)
		, m_allocator(allocator)
		, m_entityData(entityData)
		, m_hierarchy(hierarchy)
		, m_gameRenderer(gameRenderer)
		, m_assetTree(allocator->Alloc<AssetTree>(this, "Assets/"))
	{
		m_viewportPos = Vector2f(0.5f, 0.5f);
		m_viewportScale.x = 0.7f;
		m_viewportScale.y = 1.0f;
		m_viewportPos = CenterRectPosition(m_viewportPos, m_viewportScale);

		UpdateResolution();
		BuildDCRefMappingsFromNode(m_entityData);
		BuildDCRefMappingsFromNode(m_assetTree->GetAssetImportsFile()->GetRoot());

		// Requires reference mappings to be built to update entities, so it is called here.
		m_assetTree->FindAndValidateAssetPropertySourceFiles();
	}

	EditorMain::~EditorMain()
	{
		if(m_assetTree)
		{
			m_allocator->Free(m_assetTree);
			m_assetTree = nullptr;
		}

		if (m_editorRenderGraph)
		{
			m_allocator->Free(m_editorRenderGraph);
			m_editorRenderGraph = nullptr;
		}
		if (m_imguiRenderPass)
		{
			m_allocator->Free(m_imguiRenderPass);
			m_imguiRenderPass = nullptr;
		}

		if (m_editorRenderOutput)
		{
			m_renderer->FreeTexture(m_editorRenderOutput);
			m_editorRenderOutput = nullptr;
		}
	}

	void EditorMain::UpdateResolution()
	{
		PB::ISwapChain* swapchain = m_renderer->GetSwapchain();
		m_windowDimensions.x = float(swapchain->GetWidth());
		m_windowDimensions.y = float(swapchain->GetHeight());

		CreateRenderGraph();

		m_updateOutlinerPanelFrames += 2;
		m_updateInspectorPanelFrames += 2;
	}

	void EditorMain::EditorUpdate(GLFWwindow* window, Camera* camera)
	{
		Input* input = Input::GetInstance();

		if (m_viewportFocused == true)
		{
			UpdateViewportEditor(camera);

			camera->UpdateFreeCam(g_timeMain.DeltaTimeMain(), input, window);
		}
		else
		{
			camera->Update();
		}

		if (m_selection.entity != nullptr)
		{
			if (input->GetKeyReleased(KEYBOARD_KEY_F))
			{
				Bounds bounds = Bounds::Identity();
				StaticEntityTracker* staticTracker = m_selection.entity->GetComponent<StaticEntityTracker>();
				if (staticTracker)
				{
					bounds = staticTracker->GetBoundingBox();
				}
				else
				{
					DynamicEntityTracker* dynamicTracker = m_selection.entity->GetComponent<DynamicEntityTracker>();
					if (dynamicTracker)
					{
						bounds = dynamicTracker->GetEntityWorldBounds();
					}
				}

				float maxDim = Math::Max(Math::Max(bounds.m_extents.x, bounds.m_extents.y), bounds.m_extents.z);
				Vector3f targetPosition = bounds.Center() + (camera->Forward() * 2.0f * maxDim);

				Transform* camTransform = camera->GetHost()->GetComponent<Transform>();
				camTransform->SetPosition(targetPosition);
			}

			if (!input->GetKey(KEYBOARD_KEY_DELETE, INPUTSTATE_CURRENT) && input->GetKey(KEYBOARD_KEY_DELETE, INPUTSTATE_PREVIOUS))
			{
				printf("Destroying Entity: %s\n", m_selection.entity->GetName());

				m_hierarchy->DestroyEntity(m_selection.entity);
				SelectEntity(nullptr);
			}
		}

		// Draw selected entity
		DebugLinePass* debugLinePass = m_gameRenderer->GetDebugLinePass();
		if (m_selection.entity != nullptr)
		{
			StaticEntityTracker* staticTracker = m_selection.entity->GetComponentPtr<StaticEntityTracker>();
			if (staticTracker)
			{
				Bounds bounds = staticTracker->GetBoundingBox();
				debugLinePass->DrawCube(bounds.m_origin, bounds.m_extents, Vector3f(0.0f, 1.0f, 0.0f));
			}
			else
			{
				DynamicEntityTracker* dynamicTracker = m_selection.entity->GetComponentPtr<DynamicEntityTracker>();
				if (dynamicTracker)
				{
					Bounds bounds = dynamicTracker->GetEntityWorldBounds();
					debugLinePass->DrawCube(bounds.m_origin, bounds.m_extents, Vector3f(0.0f, 1.0f, 0.0f));
				}
			}
		}
	}

	void EditorMain::UpdateGUI(const PB::ImGuiTextureData& worldRenderOutput)
	{
		ImGui::NewFrame();

		// Outliner
		{
			Vector2f outlinerScale = Vector2f(m_viewportPos.x, 1.0f) * m_windowDimensions;

			if (StartPanel("Outliner", Vector2f(0.0f), outlinerScale, m_updateOutlinerPanelFrames > 0, m_outlinerPanelFocused))
			{
				ImGui::BeginListBox("##", ImGui::GetContentRegionAvail());

				auto& entities = m_hierarchy->GetAllEntities();
				uint32_t i = 0;
				for (TObjectPtr<Entity> e : entities)
				{
					bool selected = false;

					std::stringstream label;
					label << e->GetName();
					label << "##" << i;

					if (e.GetPtr() == m_selection.entity)
					{
						selected = true;
					}

					ImGui::Selectable(label.str().c_str(), &selected);
					if (selected == true)
					{
						SelectEntity(e);
					}

					++i;
				}
				ImGui::EndListBox();

				--m_updateOutlinerPanelFrames;
			}
			
			EndPanel();
		}

		// Inspector
		{
			Vector2f inspectorPos = Vector2f((m_viewportPos.x * m_windowDimensions.x) + (m_viewportScale.x * m_windowDimensions.x), 0.0f);
			Vector2f inspectorScale = Vector2f(m_windowDimensions.x - inspectorPos.x, m_windowDimensions.y);

			uint32_t fieldId = 0;
			if (StartPanel("Inspector", inspectorPos, inspectorScale, m_updateInspectorPanelFrames > 0, m_inspectorPanelFocused))
			{
				if (m_selection.entity != nullptr)
				{
					{
						bool selected = true;
						ImGui::Selectable(m_selection.entity->GetName(), &selected);
						ImGui::Spacing();
					}

					auto& components = m_selection.entity->GetAllComponents();
					for (auto& component : components)
					{
						auto& reflection = component->GetReflection();
						auto& fields = reflection.GetReflectionFields();

						bool headerActive = ImGui::CollapsingHeader(reflection.GetTypeName(), fields.Count() > 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0);

						if (headerActive)
						{
							DataTableImGUI(component, reflection, fieldId);
						}
					}
				}
				else if (m_selection.openDataClass.IsValid())
				{
					auto& reflection = m_selection.openDataClass->GetReflection();

					bool selected = true;
					ImGui::Selectable(reflection.GetTypeName(), &selected);
					ImGui::Spacing();

					DataTableImGUI(m_selection.openDataClass, reflection, fieldId);
				}
				else
				{
					ImGui::TextWrapped("Nothing selected. Select an entity to see its contents here.");
				}

				--m_updateInspectorPanelFrames;
			}

			EndPanel();
		}

		if (m_selection.runSelectFileDialogue == true)
		{
			bool isNewAsset;
			Ctrl::ObjectPtr objectPtr{};
			m_selection.runSelectFileDialogue = m_assetTree->RunSelectFileDialogue(m_windowDimensions, objectPtr, m_selection.assignmentAssetTypename.c_str());

			if (m_selection.runSelectFileDialogue == false && objectPtr.IsValid())
			{
				AssignDataClassRef(objectPtr);
			}
		}

		ViewportImGUI(&worldRenderOutput);

		ImGui::Render();
		m_drawData = ImGui::GetDrawData();

	}

	void EditorMain::RenderGUI(PB::ICommandContext* cmdContext)
	{
		if (m_editorRenderGraph)
		{
			PB::u32 swapChainIdx = m_renderer->GetCurrentSwapchainImageIndex();
			auto* swapChainTex = m_renderer->GetSwapchain()->GetImage(swapChainIdx);

			m_imguiRenderPass->SetDrawData(m_drawData);
			m_imguiRenderPass->SetOutputTexture(swapChainTex);

			m_editorRenderGraph->Execute(cmdContext);
		}
	}

	void EditorMain::SaveChanges()
	{
		m_assetTree->SaveAssetImports();
	}

	Math::Vector2u EditorMain::GetViewportOrigin()
	{
		const uint32_t titleHeight = 19; // #1 problem with ImGui, it does not have an API to calculate window sizes in advance before creating them. So we have to guess the title bar height...

		Vector2f viewPortOriginScaled = m_viewportPos * m_windowDimensions;
		return Vector2u(uint32_t(viewPortOriginScaled.x), uint32_t(viewPortOriginScaled.y) + titleHeight);
	}

	Math::Vector2u EditorMain::GetViewportResolution()
	{
		ImGuiStyle style{};
		
		// Account for border/padding and title bar.
		Vector2f windowContentDimensions = m_windowDimensions;
		windowContentDimensions -= 2 * style.WindowBorderSize;

		const float heightReduction = 25.0f; // Sigh... Once again, we can't calculate window math in advance with ImGui, so here we are guessing it again.
		windowContentDimensions.y -= heightReduction;

		Vector2f resolution = m_viewportScale * windowContentDimensions;
		return Vector2u(uint32_t(resolution.x), uint32_t(resolution.y));
	}

	void EditorMain::SelectEntity(Entity* entity)
	{
		m_selection.openDataClass.Invalidate();

		if (m_selection.entity != entity)
		{
			m_selection.entity = entity;

			++m_updateOutlinerPanelFrames;
			++m_updateInspectorPanelFrames;
		}
	}

	void EditorMain::CreateRenderGraph()
	{
		if (m_editorRenderGraph)
		{
			m_allocator->Free(m_editorRenderGraph);
		}

		RenderGraphBuilder rgBuilder(m_renderer, m_allocator);

		// ImGUI Pass
		{
			if (!m_imguiRenderPass)
				m_imguiRenderPass = m_allocator->Alloc<ImGUIRenderPass>(m_renderer, m_allocator);
			m_imguiRenderPass->AddToRenderGraph(&rgBuilder);
		}

		if (m_editorRenderOutput != nullptr)
		{
			m_renderer->FreeTexture(m_editorRenderOutput);
		}

		PB::ISwapChain* swapchain = m_renderer->GetSwapchain();

		constexpr const char* OutputRGName = "MergedOutput";
		const auto& outputData = rgBuilder.GetTextureData(OutputRGName);

		PB::TextureDesc editorRenderOutputDesc{};
		editorRenderOutputDesc.m_name = "EditorRender_Output";
		editorRenderOutputDesc.m_format = PB::Util::FormatToUnorm(swapchain->GetImageFormat());
		editorRenderOutputDesc.m_width = swapchain->GetWidth();
		editorRenderOutputDesc.m_height = swapchain->GetHeight();
		editorRenderOutputDesc.m_usageStates = outputData->m_usage | PB::ETextureState::COPY_SRC;
		editorRenderOutputDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_NONE;
		m_editorRenderOutput = m_renderer->AllocateTexture(editorRenderOutputDesc);
		rgBuilder.RegisterUserTexture("MergedOutput", m_editorRenderOutput);

		m_editorRenderGraph = rgBuilder.Build(false);
	}

	void EditorMain::BuildDCRefMappingsFromNode(const Ctrl::IDataNode* rootNode)
	{
		auto peekField = [&](const std::string& name, const Field& field)
		{
			if (field.m_type == EFieldType::GUID)
			{
				uint32_t guidCount;
				const Ctrl::GUID* guids = rootNode->GetGUID(field, guidCount);

				const GUID& parentGUID = rootNode->GetSelfGUID();
				if (parentGUID.IsValid())
				{
					for (uint32_t i = 0; i < guidCount; ++i)
					{
						m_dataClassRefMappings[guids[i]].insert(parentGUID);
					}
				}
			}
			else if (field.m_type == EFieldType::DATA_NODE)
			{
				uint32_t nodeCount;
				auto nodes = rootNode->GetDataNode(field, nodeCount);
				for (uint32_t i = 0; i < nodeCount; ++i)
				{
					const Ctrl::IDataNode* node = nodes[i];

					BuildDCRefMappingsFromNode(node);
				}
			}
		};

		rootNode->PeekFields(peekField);
	}

	void EditorMain::UpdateViewportEditor(Camera* camera)
	{
		Input* input = Input::GetInstance();

		float deltaTime(g_timeMain.DeltaTimeMain());

		auto translateEntity = [&](Vector3f translation)
		{
			printf("Moving Entity: %s\n", m_selection.entity->GetName());

			m_selection.entity->GetComponent<Transform>()->Translate(translation);
		};

		bool entityMoved = false;
		if (input->GetKeyReleased(KEYBOARD_KEY_N))
		{
			SelectEntity(m_hierarchy->CreateEntity(EEntityUpdateMethod::STATIC, true));
			m_selection.entity->Rename("CreateNewEntityTest");

			m_selection.entity->GetComponent<Transform>()->SetPosition(camera->Position() + (camera->Forward() * -15.0f));
			entityMoved = true;
		}
		if (input->GetKey(KEYBOARD_KEY_UP) && m_selection.entity != nullptr)
		{
			Vector3f translation = Vector3f(0.0f, 0.0f, -5.0f) * deltaTime;
			translateEntity(translation);
			entityMoved = true;
		}
		if (input->GetKey(KEYBOARD_KEY_DOWN) && m_selection.entity != nullptr)
		{
			Vector3f translation = Vector3f(0.0f, 0.0f, 5.0f) * deltaTime;
			translateEntity(translation);
			entityMoved = true;
		}
		if (input->GetKey(KEYBOARD_KEY_LEFT) && m_selection.entity != nullptr)
		{
			Vector3f translation = Vector3f(-5.0f, 0.0f, 0.0f) * deltaTime;
			translateEntity(translation);
			entityMoved = true;
		}
		if (input->GetKey(KEYBOARD_KEY_RIGHT) && m_selection.entity != nullptr)
		{
			Vector3f translation = Vector3f(5.0f, 0.0f, 0.0f) * deltaTime;
			translateEntity(translation);
			entityMoved = true;
		}
		if (input->GetKey(KEYBOARD_KEY_Q) && m_selection.entity != nullptr)
		{
			Vector3f translation = Vector3f(0.0f, 5.0f, 0.0f) * deltaTime;
			translateEntity(translation);
			entityMoved = true;
		}
		if (input->GetKey(KEYBOARD_KEY_E) && m_selection.entity != nullptr)
		{
			Vector3f translation = Vector3f(0.0f, -5.0f, 0.0f) * deltaTime;
			translateEntity(translation);
			entityMoved = true;
		}
		if (input->GetKey(KEYBOARD_KEY_R) && m_selection.entity != nullptr)
		{
			float rotation = 90.0f * deltaTime;
			m_selection.entity->GetComponent<Transform>()->RotateEulerY(ToRadians(rotation));
			entityMoved = true;
		}

		if (entityMoved)
		{
			StaticEntityTracker* tracker = m_selection.entity->GetComponent<StaticEntityTracker>();
			if (tracker)
			{
				tracker->UpdateEntity();
				m_hierarchy->UpdateTrees();
			}
		}

		if (input->GetMouseButton(MOUSEBUTTON_LEFT, INPUTSTATE_PREVIOUS) && !input->GetMouseButton(MOUSEBUTTON_LEFT, INPUTSTATE_CURRENT))
		{
			Vector2f cursorScreenPos = Vector2f
			(
				input->GetCursorX(INPUTSTATE_CURRENT),
				input->GetCursorY(INPUTSTATE_CURRENT)
			);

			printf("Cursor: [%f, %f]\n", cursorScreenPos.x, cursorScreenPos.y);

			Vector3f cursorFarPlanePos = camera->GetCursorFarPlaneWorldPosition(Vector2f(cursorScreenPos.x, cursorScreenPos.y));
			Vector3f cursorNearPlanePos = camera->GetCursorNearPlaneWorldPosition(Vector2f(cursorScreenPos.x, cursorScreenPos.y));
			Vector3f rayDirection = cursorFarPlanePos - cursorNearPlanePos;

			Vector3f rayDirNormalized = Math::Normalize(rayDirection);
			Entity* hitEntity = m_hierarchy->GetEntitySpatialHashTable().RaycastGetEntity(cursorNearPlanePos, rayDirNormalized, 2000.0f);
			if (hitEntity == nullptr)
			{
				auto* selectedEntityData = m_hierarchy->GetEntityBoundingVolumeHierarchy().RaycastGetObjectData(m_gameRenderer->GetDebugLinePass(), cursorNearPlanePos, rayDirection);
				if (selectedEntityData)
				{
					hitEntity = reinterpret_cast<const EntityBoundingVolumeHierarchy::ObjectData*>(selectedEntityData)->m_entity;
				}
			}

			if (hitEntity)
			{
				SelectEntity(hitEntity);
				printf("Selected Entity: %s\n", m_selection.entity->GetName());
			}
			else
			{
				SelectEntity(nullptr);
				printf("No Entity Selected.\n");
			}
		}
	}

	void EditorMain::ViewportImGUI(const PB::ImGuiTextureData* worldRenderOutput)
	{
		ImVec2 windowSize = (ImVec2)(m_viewportScale * m_windowDimensions);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);

		ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
		{
			ImGui::SetWindowSize(windowSize);
			ImGui::SetWindowPos((ImVec2)(m_viewportPos * m_windowDimensions));
			m_viewportFocused = ImGui::IsWindowFocused();

			if (worldRenderOutput)
			{
				ImGui::Image(worldRenderOutput->ref, ImVec2(float(worldRenderOutput->width), float(worldRenderOutput->height)));
			}
		}
		ImGui::End();

		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
	}

	bool EditorMain::StartPanel(const char* name, Vector2f position, Vector2f scale, bool update, bool& focus, ImGuiWindowFlags additionalWindowFlags)
	{
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | additionalWindowFlags;
		bool drawPanel = update == true || focus == true;
		if (drawPanel == false)
		{
			windowFlags |= ImGuiWindowFlags_NoBackground;
		}

		ImGui::Begin(name, nullptr, windowFlags);

		ImGui::SetWindowSize(ImVec2(scale));
		ImGui::SetWindowPos(ImVec2(position));
		focus = ImGui::IsWindowFocused();

		return drawPanel;
	}

	void EditorMain::EndPanel()
	{
		ImGui::End();
	}

	void EditorMain::DataTableImGUI(Ctrl::ObjectPtr& dataClass, const Reflectron::Reflector& reflection, uint32_t& fieldId)
	{
		auto& fields = reflection.GetReflectionFields();

		bool tableActive = ImGui::BeginTable("##", 2);
		if (tableActive)
		{
			if (fields.Count() > 0)
			{
				for (auto& field : fields)
				{
					ImGui::TableNextRow();

					FieldInterface fieldInterface(reflection, field);

					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%s", fieldInterface.GetDisplayName().c_str());

					ImGui::TableSetColumnIndex(1);
					fieldInterface.DrawImGUI(fieldId);

					if (fieldInterface.DataClassAssignmentRequested())
					{
						fieldInterface.GetDataClassAssignmentType(m_selection.assignmentAssetTypename);

						AssetTree::FileVector foundFiles{};
						m_assetTree->FindAssetFiles(m_selection.assignmentAssetTypename.c_str(), foundFiles);
						if (foundFiles.Count() > 0)
						{
							m_assetTree->SelectFileList(foundFiles);

							m_selection.field = field;
							m_selection.openComponent = dataClass;
							m_selection.assignmentTarget = fieldInterface.GetSelectedDataClass();
							m_selection.runSelectFileDialogue = true;

							for (auto& file : foundFiles)
							{
								printf("Found file: [%s], GUID: [%s]\n", file.fileName.c_str(), file.assetClassGUID.AsCString());
							}
						}
					}
					else if (fieldInterface.DataClassNullAssignmentRequested())
					{
						m_selection.field = field;
						m_selection.openComponent = dataClass;
						m_selection.assignmentTarget = fieldInterface.GetSelectedDataClass();
						
						AssignDataClassRef(ObjectPtr());
					}
					else if (fieldInterface.DataClassSelectionRequested())
					{
						SelectDataClass(*fieldInterface.GetSelectedDataClass());
					}
					else if (fieldInterface.FieldHasBeenModified())
					{
						dataClass->OnFieldChanged(field);
					}
				}
			}
			else
			{
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::Text("No values here.");
			}

			ImGui::EndTable();
		}
	}

	void EditorMain::SelectDataClass(Ctrl::ObjectPtr dc)
	{
		m_selection.entity = nullptr;

		if (m_selection.openDataClass != dc)
		{
			m_selection.openDataClass = dc;

			++m_updateOutlinerPanelFrames;
			++m_updateInspectorPanelFrames;
		}
	}
	
	void EditorMain::RefreshReferences(Ctrl::ObjectPtr& target)
	{
		if(target.IsValid())
		{
			const GUID& guid = target.GetAssignedGUID();

			auto it = m_dataClassRefMappings.find(guid);
			if (it != m_dataClassRefMappings.end())
			{
				auto refs = it->second;
				for(auto& ref : refs)
				{	
					ObjectPtr dc(ref);
					assert(dc.IsValid());

					dc->OnReferenceChanged(target);
				}
			}
		}
	}

	void EditorMain::AssignDataClassRef(const Ctrl::ObjectPtr& srcRef)
	{
		if(m_selection.assignmentTarget == nullptr)
		{
			return;
		}

		Ctrl::ObjectPtr prevDC{}; 
		if (m_selection.assignmentTarget->IsValid())
		{
			prevDC = *m_selection.assignmentTarget;
		}
		
		*m_selection.assignmentTarget = srcRef;
		m_selection.assignmentTarget = nullptr;
		
		ObjectPtr& openDc = m_selection.openDataClass.IsValid() ? m_selection.openDataClass : m_selection.openComponent;

		// Find and remove the reference mapping to the previously assigned DataClass.
		if (prevDC.IsValid() && openDc.IsValid())
		{
			const GUID& prevGuid = prevDC.GetAssignedGUID();
			const GUID& refererGuid = openDc.GetAssignedGUID();

			auto it = m_dataClassRefMappings.find(prevGuid);
			if (it != m_dataClassRefMappings.end())
			{
				auto& refs = it->second;
				if (refs.contains(refererGuid))
				{
					refs.erase(refererGuid);
				}
			}
		}

		// Add new reference mapping.
		if (srcRef.IsValid() && openDc.IsValid())
		{
			const GUID& srcGuid = srcRef.GetAssignedGUID();
			const GUID& refererGuid = openDc.GetAssignedGUID();

			// auto it = m_dataClassRefMappings.find(srcGuid);
			// if (it != m_dataClassRefMappings.end())
			// {
			// 	it->second.insert(refererGuid);
			// }
			m_dataClassRefMappings[srcGuid].insert(refererGuid);
		}

		// Find all references to the DataClass of the field we just changed and signal for a hot reload if necessary.
		if (openDc.IsValid())
		{
			openDc->OnFieldChanged(m_selection.field);
			RefreshReferences(openDc);
		}
	}

	Vector2f EditorMain::CenterRectPosition(const Vector2f position, const Vector2f& dimensions)
	{
		return (position - (dimensions * 0.5));
	}
}
