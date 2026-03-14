#include "AssetTree.h"
#include "EditorMain.h"
#include "CLib/Vector.h"
#include "Engine.Control/GUID.h"
#include "Engine.Control/IDataClass.h"
#include "Engine.Control/IDataFile.h"
#include "Engine.AssetPipeline/Asset.h"

#include "Engine.Math/Vector2.h"
#include "imgui.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>

namespace Eng
{
    AssetTree::AssetTree(EditorMain* editorMain, const char* assetDirectory)
        : m_assetDirectory(assetDirectory)
        , m_editorMain(editorMain)
    {
        m_searchTermBuffer.SetCount(512);
        memset(m_searchTermBuffer.Data(), 0, m_searchTermBuffer.Count());

        m_assetImportsFile = Ctrl::IDataFile::Create();
        
        std::string importsFilename = assetDirectory;
        importsFilename += s_importsFileName;

        m_assetImportsFile->Open(importsFilename.c_str(), Ctrl::IDataFile::OPEN_READ_WRITE, true);
        Ctrl::DataClass::InstantiateNodeTree(m_assetImportsFile->GetRoot());
    }

    AssetTree::~AssetTree()
    {
        m_assetImportsFile->Close();

        Ctrl::IDataFile::Destroy(m_assetImportsFile);
        m_assetImportsFile = nullptr;
    }

    void AssetTree::FindAssetFiles(const char* assetTypeName, FileVector& outFiles)
    {
        std::filesystem::path workingDir = std::filesystem::current_path();
		std::filesystem::path searchDir = workingDir;
        searchDir.append(m_assetDirectory);

		if (std::filesystem::is_directory(searchDir))
		{
			for (const auto& entry : std::filesystem::recursive_directory_iterator(searchDir))
			{
				bool validExtension = IsValidAssetExtension(entry.path().extension().c_str());
				if (entry.is_regular_file() && validExtension)
				{
					std::string filePath = entry.path().string().c_str();
                    Ctrl::IDataFile* dataFile = Ctrl::IDataFile::Create();

                    auto openResult = dataFile->Open(filePath.c_str(), Ctrl::IDataFile::EOpenMode::OPEN_READ_ONLY, true);
                    if(openResult == Ctrl::IDataFile::OPENED_SUCCESSFULLY)
                    {
                        CLib::Vector<Ctrl::IDataNode*> childNodes;
                        dataFile->GetRoot()->GetAllChildDataNodes(childNodes);

                        for(auto& child : childNodes)
                        {
                            if(child->GetTypeName() && strcmp(child->GetTypeName(), assetTypeName) == 0)
                            {
                                // File is a data node of the correct type. Output this file.
                                FileInfo& fileInfo = outFiles.PushBack();
                                fileInfo.fileName = filePath;
                                fileInfo.assetClassGUID = child->GetSelfGUID();

                                std::stringstream searchTerm;
                                searchTerm << child->GetString("m_assetName") << " (" << filePath << ')';

                                fileInfo.searchTerm = searchTerm.str();

                                // Only find the first instance of the type we're looking for. It is not expected to find more.
                                break;
                            }
                        }

                        dataFile->Close();
                    }

                    Ctrl::IDataFile::Destroy(dataFile);
				}
			}
		}
		else
		{
			printf("%s - Directory not found.\n", searchDir.c_str());
		}
    }

    bool AssetTree::RunSelectFileDialogue(const Math::Vector2f windowDimensions, Ctrl::ObjectPtr& outObjectPtr, const char* assetTypeName)
    {
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

        int imguiId = 0;
        bool shouldClose = false;

		ImGui::Begin("Select file...", nullptr, windowFlags);

        Math::Vector2f scale = Math::Vector2f(0.35f, 0.5f) * windowDimensions;

		ImGui::SetWindowSize(ImVec2(scale));
		ImGui::SetWindowPos(ImVec2(windowDimensions * 0.5f - scale * 0.5));

        {
            ImGui::InputText("##", m_searchTermBuffer.Data(), m_searchTermBuffer.Count(), ImGuiInputTextFlags_ElideLeft);

            Math::Vector2f contentRegion(ImGui::GetContentRegionAvail());
            Math::Vector2f listRegion = contentRegion;
            listRegion.y *= 0.9f;

            if (ImGui::BeginListBox("##", ImVec2(listRegion)))
            {
                std::string searchLowerCase = m_searchTermBuffer.Data();
                std::transform(searchLowerCase.begin(), searchLowerCase.end(), searchLowerCase.begin(),
                    [](unsigned char c){ return std::tolower(c); });

                for(auto& file : m_selectedFileList)
                {
                    std::string fileSearchLowercase = file.searchTerm;
                    std::transform(fileSearchLowercase.begin(), fileSearchLowercase.end(), fileSearchLowercase.begin(),
                    [](unsigned char c){ return std::tolower(c); });

                    if (strstr(fileSearchLowercase.c_str(), searchLowerCase.c_str()) == nullptr)
                    {
                        continue;
                    }

			    	ImGui::PushID(++imguiId);

                    bool selected = false;
                    ImGui::Selectable(file.searchTerm.c_str(), &selected, ImGuiSelectableFlags_AllowDoubleClick);
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			    	{
                        outObjectPtr = AssignAssetFromFile(file);
                        shouldClose = true;
                    }

                    ImGui::PopID();
                }

                ImGui::EndListBox();
            }

            // Buttons...
            {
                ImVec2 buttonSize = ImVec2(Math::Vector2f(0.1f, 0.09f) * contentRegion);

                if(ImGui::Button("Cancel", buttonSize))
                {
                    shouldClose = true;
                }

                ImGui::SameLine();
                if (assetTypeName != nullptr)
                {
                    std::stringstream buttonText;
                    buttonText << "New " << assetTypeName;
                    if(ImGui::Button(buttonText.str().c_str(), buttonSize))
                    {
                        outObjectPtr = CreateNewAsset(assetTypeName);
                        shouldClose = true;
                    }
                }
            }
        }

        ImGui::End();

        return !shouldClose;
    }

    Ctrl::ObjectPtr AssetTree::CreateNewAsset(const char* assetTypeName)
    {
        auto* asset = reinterpret_cast<AssetPipeline::Asset*>(CLib::Reflection::InstantiateClass(assetTypeName));
        if (asset == nullptr)
            return Ctrl::ObjectPtr();

        static constexpr const char* s_assetExtension = s_validAssetExtensions[0];

        std::stringstream assetFilename;
        assetFilename << m_assetDirectory << "new_" << assetTypeName << s_assetExtension;
        
        int attemptsMade = -1;
        auto fileExists = [&]() -> bool
        {
            if(std::filesystem::exists(assetFilename.str().c_str()))
            {
                ++attemptsMade;
                return true;
            }
            
            return false;
        };
        
        // If the file opens successfully, then the file probably already exists. Change the name until it doesn't.
        while (fileExists())
        {
            assetFilename = std::stringstream();
            assetFilename << m_assetDirectory << "new_" << assetTypeName << "_" << attemptsMade << s_assetExtension;
        }

        std::string dataName = std::filesystem::path(assetFilename.str().c_str()).replace_extension(); // Filename without extension.
        std::replace(dataName.begin(), dataName.end(), '/', '_');
        
        asset->SetAssetName(dataName);
        asset->SetAssetGUID(Ctrl::GenerateGUID());

        // Create asset file...
        Ctrl::ObjectPtr ptr(asset);
        Ctrl::IDataFile* assetFile = Ctrl::IDataFile::Create();
        if (assetFile->Open(assetFilename.str().c_str(), Ctrl::IDataFile::EOpenMode::OPEN_READ_WRITE, true) == Ctrl::IDataFile::EFileStatus::OPENED_SUCCESSFULLY)
        {

        }
        else
        {
            auto* root = assetFile->GetRoot();
            auto* assetNode = root->AddDataNode(dataName.c_str(), assetTypeName);
            ptr->SaveToDataNode(assetNode);

            assetFile->WriteData();
            assetFile->Close();

            m_importPropertySourceFileMap[ptr.GetAssignedGUID()] = assetFilename.str();
        }
        Ctrl::IDataFile::Destroy(assetFile);

        // Add to asset imports...
        ptr->SaveToDataNode(m_assetImportsFile->GetRoot()->AddDataNode(assetTypeName, assetTypeName));

        return ptr;
    }

    void AssetTree::SaveAssetImports()
    {
        Ctrl::DataClass::SaveNodeTree(m_assetImportsFile->GetRoot());

        // Save all changes made to asset property source files.
        for (auto& [guid, fileName] : m_importPropertySourceFileMap)
        {
            Ctrl::TObjectPtr<AssetPipeline::Asset> ptr(guid);
            printf(fileName.c_str());
            printf("\n");

            Ctrl::IDataFile* assetFile = Ctrl::IDataFile::Create();
            if(ptr.IsInstantiated() && assetFile->Open(fileName.c_str(), Ctrl::IDataFile::OPEN_READ_ONLY) == Ctrl::IDataFile::EFileStatus::OPENED_SUCCESSFULLY)
            {
                CLib::Vector<Ctrl::IDataNode*> childNodes;
                assetFile->GetRoot()->GetAllChildDataNodes(childNodes);

                bool nameChanged = false;
                std::filesystem::path oldFilename;
                if (childNodes.Count() > 0 && ptr->FieldsMatchDataNode(childNodes[0]) == false)
                {
                    const char* liveAssetName = ptr->GetAssetName().c_str();
                    const char* propAssetName = childNodes[0]->GetString("m_assetName");
                    nameChanged = strcmp(propAssetName, liveAssetName) != 0;

                    ptr->SaveToDataNode(childNodes[0]);

                    // If the asset's name has changed, rename the property file too.
                    if(nameChanged == true)
                    {
                        oldFilename = fileName;
                        std::filesystem::path newFilename = oldFilename;
                        newFilename.replace_filename(liveAssetName);
                        newFilename.replace_extension(s_validAssetExtensions[0]);

                        assetFile->WriteData(newFilename.c_str());

                        fileName = newFilename;
                    }
                    else
                    {

                        assetFile->WriteData();
                    }
                }

                assetFile->Close();

                if (nameChanged)
                {
                    // Remove old file.
                    std::filesystem::remove(oldFilename);
                }
            }
            Ctrl::IDataFile::Destroy(assetFile);
        }

        m_assetImportsFile->WriteData();
    }

    bool AssetTree::IsValidAssetExtension(const char* extension)
    {
        for(const char* validExt : s_validAssetExtensions)
        {
            if(strcmp(validExt, extension) == 0)
                return true;
        }

        return false;
    }

    Ctrl::ObjectPtr AssetTree::AssignAssetFromFile(const FileInfo& file)
    {
        Ctrl::GUID assetDataGUID = file.assetClassGUID;

        // Instantiate the asset data class if it doesn't exist.
        Ctrl::ObjectPtr assetPtr(assetDataGUID);
        if(assetPtr == nullptr)
        {
            Ctrl::IDataFile* assetFile = Ctrl::IDataFile::Create();
            if(assetFile->Open(file.fileName.c_str(), Ctrl::IDataFile::OPEN_READ_ONLY) == Ctrl::IDataFile::EFileStatus::OPENED_SUCCESSFULLY)
            {
                Ctrl::DataClass::InstantiateNodeTree(assetFile->GetRoot());
                assetFile->Close();

                if (assetPtr == nullptr)
                {
                    printf("[AssetTree] Could not instantiate asset data class from file: %s", file.fileName.c_str());
                }
                else
                {
                    // Add asset to runtime imports.
                    Ctrl::DataClass* dc = assetPtr.GetPtr();
                    auto& reflection = dc->GetReflection();

                    dc->SaveToDataNode(m_assetImportsFile->GetRoot()->AddDataNode(reflection.GetTypeName(), reflection.GetTypeName()));
                }
            }
            Ctrl::IDataFile::Destroy(assetFile);
        }

        return Ctrl::ObjectPtr(assetPtr);
    }

    void AssetTree::FindAndValidateAssetPropertySourceFiles()
    {
        std::filesystem::path workingDir = std::filesystem::current_path();
		std::filesystem::path searchDir = workingDir;
        searchDir.append(m_assetDirectory);

		if (std::filesystem::is_directory(searchDir))
		{
			for (const auto& entry : std::filesystem::recursive_directory_iterator(searchDir))
			{
				bool validExtension = IsValidAssetExtension(entry.path().extension().c_str());
				if (entry.is_regular_file() && validExtension)
				{
					std::string filePath = entry.path().string().c_str();
                    Ctrl::IDataFile* dataFile = Ctrl::IDataFile::Create();

                    auto openResult = dataFile->Open(filePath.c_str(), Ctrl::IDataFile::EOpenMode::OPEN_READ_ONLY, true);
                    if(openResult == Ctrl::IDataFile::OPENED_SUCCESSFULLY)
                    {
                        CLib::Vector<Ctrl::IDataNode*> childNodes;
                        dataFile->GetRoot()->GetAllChildDataNodes(childNodes);

                        for(auto& child : childNodes)
                        {
                            const Ctrl::GUID& assetGUID = child->GetSelfGUID();
                            m_importPropertySourceFileMap[assetGUID] = filePath;

                            ValidateAssetImport(child, assetGUID, filePath);
                        }

                        dataFile->Close();
                    }

                    Ctrl::IDataFile::Destroy(dataFile);
				}
			}
		}
		else
		{
			printf("%s - Directory not found.\n", searchDir.c_str());
		}
    }

    void AssetTree::ValidateAssetImport(Ctrl::IDataNode* propertySourceNode, const Ctrl::GUID& assetGUID, const std::string& propertySourceFilePath)
    {
        Ctrl::TObjectPtr<AssetPipeline::Asset> ptr(assetGUID);
        if(ptr.IsInstantiated() == false)
        {
            return; // If asset has not been instantiated it must not be included in the imports, no validation needed.
        }

        // Ensure import's properties match the source.
        if (ptr->FieldsMatchDataNode(propertySourceNode) == false)
        {
            printf
            (
                "[AssetTree] WARNING: Fields for imported asset [%s] do not match the asset's property source file (%s). Using the properties from the property source.\n",
                ptr->GetAssetName().c_str(),
                propertySourceFilePath.c_str()
            );

            ptr->FillFromDataNode(propertySourceNode);

            // Trigger appropriate field/reference change updates.
            auto& reflection = ptr->GetReflection();
            for (auto& field : reflection.GetReflectionFields())
            {
                ptr->OnFieldChanged(field);

                size_t arrayCount = 0;
                Ctrl::EFieldType fieldType;
                Ctrl::DataClass::GetTypeInfoFromField(field, fieldType, arrayCount);
                if (fieldType == Ctrl::EFieldType::GUID)
                {
                    m_editorMain->RefreshReferences(ptr);
                }
            }
        }
    }
}