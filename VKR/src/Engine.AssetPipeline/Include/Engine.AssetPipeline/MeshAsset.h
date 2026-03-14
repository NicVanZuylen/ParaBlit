#pragma once
#include "Asset.h"
#include "Engine.Math/Vector3.h"
#include "Engine.Reflectron/ReflectronAPI.h"
#include "MeshAsset_generated.h"
#include "CLib/Reflection.h"
#include "MeshShared.h"

namespace AssetPipeline
{
	using namespace Eng::Math;

	class MeshAsset : public Asset
	{
		REFLECTRON_CLASS()

	public:

		REFLECTRON_GENERATED_MeshAsset()

		MeshAsset() : Asset(this)
		{
		}

		~MeshAsset() = default;

		const Vector3f& GetBoundOrigin() const { return m_boundOrigin; }
		const Vector3f& GetBoundExtents() const { return m_boundExtents; }
		int GetLODCount() const { return m_lodCount; }
		int GetMaxLOD() const { return m_maxLod; }
		int GetSkipLodLevelCount() const { return m_skipLodLevelCount; }
		const bool* GetSkipLodLevels() const { return m_skipLodLevels; }
		bool GetConvertCMToM() const { return m_convertCmToM; }
		bool GetGenerateMeshlets() const { return m_generateMeshlets; }
		bool GetGenerateBLASData() const { return m_generateBLASData; }

		void SetBoundOrigin(const Vector3f& boundOrigin) { m_boundOrigin = boundOrigin; }
		void SetBoundExtents(const Vector3f& boundExtents) { m_boundExtents = boundExtents; }
		void SetLODCount(int lodCount)  { m_lodCount = lodCount; }
		void SetMaxLOD(int maxLod) { m_maxLod = maxLod; }
		void SetSkipLodLevelCount(int skipCount) { m_skipLodLevelCount = skipCount; }
		bool* GetSkipLodLevels() { return m_skipLodLevels; }
		void SetConvertCMToM(bool val) { m_convertCmToM = val; }
		void SetGenerateMeshlets(bool val) { m_generateMeshlets = val; }
		void SetGenerateBLASData(bool val) { m_generateBLASData = val; }

	private:

		REFLECTRON_FIELD()
		Eng::Math::Vector3f m_boundOrigin;
		REFLECTRON_FIELD()
		Eng::Math::Vector3f m_boundExtents;
		REFLECTRON_FIELD()
		int m_lodCount = DefaultLODCount;
		REFLECTRON_FIELD()
		int m_maxLod = DefaultLODCount - 1;
		REFLECTRON_FIELD()
		int m_skipLodLevelCount = MaxLODCount;
		REFLECTRON_FIELD()
		bool m_skipLodLevels[8]{};
		REFLECTRON_FIELD()
		bool m_convertCmToM = false;
		REFLECTRON_FIELD()
		bool m_generateMeshlets = true;
		REFLECTRON_FIELD()
		bool m_generateBLASData = true;
	};
	CLIB_REFLECTABLE_CLASS(MeshAsset)
}