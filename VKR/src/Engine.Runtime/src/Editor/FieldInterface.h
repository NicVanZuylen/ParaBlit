#pragma once
#include "Engine.Control/IDataFile.h"
#include "Engine.Control/IDataClass.h"
#include "Engine.Reflectron/ReflectronReflector.h"

namespace Eng
{

	class FieldInterface
	{
	public:

		FieldInterface(const Reflectron::Reflector& reflector, const ReflectronFieldData& field);

		~FieldInterface();

		void DrawImGUI(uint32_t& imguiId);

		std::string GetDisplayName();

		Ctrl::ObjectPtr* GetSelectedDataClass() const { return m_selectedDataClass; }

		void GetDataClassAssignmentType(std::string& outTypeStr) const;

		bool FieldHasBeenModified() const { return m_valueModified; }

		bool DataClassAssignmentRequested() const { return m_assignmentRequested == true && m_selectedDataClass != nullptr; }

		bool DataClassNullAssignmentRequested() const { return m_nullAssignmentRequested == true && m_selectedDataClass != nullptr; }

		bool DataClassSelectionRequested() const { return DataClassAssignmentRequested() == false && m_selectedDataClass != nullptr; }

	private:

		bool HandleSpecialType(uint32_t& imguiId);
		void DrawUnknownValue();
		void DrawIntTupleImGUI(uint32_t& imguiId);
		void DrawBoolTupleImGUI(uint32_t& imguiId);
		void DrawFloatTupleImGUI(uint32_t& imguiId);
		void DrawDoubleTupleImGUI(uint32_t& imguiId);
		void DrawStringImGUI(uint32_t& imguiId);
		void DrawDataClassImGUI(uint32_t& imguiId);

		bool ShouldShowField(uint32_t& imguiId, bool allowCollapse);
		void BeginArrayElem(const uint32_t& index);

		Ctrl::EFieldType m_type;
		Ctrl::ObjectPtr* m_selectedDataClass = nullptr;
		size_t m_tupleSize;
		bool m_valueModified = false;
		bool m_assignmentRequested = false;
		bool m_nullAssignmentRequested = false;

		const Reflectron::Reflector& m_reflector;
		const ReflectronFieldData& m_field;
	};
}