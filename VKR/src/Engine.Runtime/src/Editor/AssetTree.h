#pragma once
#include "CLib/Vector.h"
#include "Engine.Control/GUID.h"
#include "Engine.Control/IDataClass.h"
#include "Engine.Control/IDataFile.h"

#define MATH_IMPL_IMGUI
#include "Engine.Math/Vector2.h"

#include <string>

namespace Eng
{
    class EditorMain;

    class AssetTree
    {
    public:

        struct FileInfo
        {
            FileInfo()
                : assetClassGUID(Ctrl::nullGUID)
                , fileName()
                , searchTerm()
            {
            }

            FileInfo(const FileInfo& other)
            {
                fileName = other.fileName;
                assetClassGUID = other.assetClassGUID;
                searchTerm = other.searchTerm;
            }

            ~FileInfo() = default;

            Ctrl::GUID assetClassGUID;
            std::string fileName;
            std::string searchTerm;
        };

        using FileVector = CLib::Vector<FileInfo, 0, 16, true>;

        AssetTree(EditorMain* editorMain, const char* assetDirectory);

        ~AssetTree();

        void FindAssetFiles(const char* assetTypeName, FileVector& outFiles);
        
        void SelectFileList(const FileVector& files) { m_selectedFileList = files; };
        
        bool RunSelectFileDialogue(const Math::Vector2f windowDimensions, Ctrl::ObjectPtr& outObjectPtr, const char* assetTypeName = nullptr);
        
        Ctrl::ObjectPtr CreateNewAsset(const char* assetTypeName);
        
        void SaveAssetImports();
        
        const Ctrl::IDataFile* GetAssetImportsFile() const { return m_assetImportsFile; }

        /*
        Description: Find property source files and ensure imported asset properties (in assetImports file) match their source properties file.
        */
        void FindAndValidateAssetPropertySourceFiles();
        
private:
        
        static constexpr const char* s_validAssetExtensions[]
        {
            ".xml"
        };
        
        static constexpr const char* s_importsFileName = "assetImports.exml";
        
        bool IsValidAssetExtension(const char* extension);
        Ctrl::ObjectPtr AssignAssetFromFile(const FileInfo& file);

        void ValidateAssetImport(Ctrl::IDataNode* propertySourceNode, const Ctrl::GUID& assetGUID, const std::string& propertySourceFilePath);

        EditorMain* m_editorMain = nullptr;
        FileVector m_selectedFileList{};
        std::unordered_map<Ctrl::GUID, std::string> m_importPropertySourceFileMap;
        CLib::Vector<char> m_searchTermBuffer{};
        std::string m_assetDirectory;
        Ctrl::IDataFile* m_assetImportsFile = nullptr;
    };
}