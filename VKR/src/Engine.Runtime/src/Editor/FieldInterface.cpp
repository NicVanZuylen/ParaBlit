#include "FieldInterface.h"

#include "CLib/Vector.h"

#include "Engine.Control/IDataClass.h"
#include "Engine.Math/Scalar.h"
#include "Engine.Math/Quaternion.h"

#include "imgui.h"

#include <sstream>
#include <algorithm>

namespace Eng
{
	using namespace Ctrl;

	FieldInterface::FieldInterface(const Reflectron::Reflector& reflector, const ReflectronFieldData& field)
		: m_reflector(reflector)
		, m_field(field)
	{
		Ctrl::DataClass::GetTypeInfoFromField(field, m_type, m_tupleSize);
		m_tupleSize /= m_field.m_arrayCount;
	}

	FieldInterface::~FieldInterface()
	{
	}

	float GetFieldScrollAmount()
	{
		const ImGuiIO& io = ImGui::GetIO();

		if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) && ImGui::IsItemHovered())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

			if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
			{
				return (io.MousePos.x - io.MousePosPrev.x) * 0.1f;
			}
		}
		else if (io.MouseWheel != 0.0f && ImGui::IsItemHovered())
		{
			return io.MouseWheel * 0.5f;
		}

		return 0.0f;
	}

	void FieldInterface::DrawImGUI(uint32_t& imguiId)
	{
		if (HandleSpecialType(imguiId))
			return;

		switch (m_type)
		{
		case Ctrl::EFieldType::UNKNOWN_OR_INVALID:
			DrawUnknownValue();
			break;
		case Ctrl::EFieldType::INT:
			DrawIntTupleImGUI(imguiId);
			break;
		case Ctrl::EFieldType::FLOAT:
			DrawFloatTupleImGUI(imguiId);
			break;
		case Ctrl::EFieldType::DOUBLE:
			DrawDoubleTupleImGUI(imguiId);
			break;
		case Ctrl::EFieldType::BOOL:
			DrawBoolTupleImGUI(imguiId);
			break;
		case Ctrl::EFieldType::STRING:
			DrawStringImGUI(imguiId);
			break;
		case Ctrl::EFieldType::DATA_NODE:
			DrawUnknownValue();
			break;
		case Ctrl::EFieldType::GUID:
			DrawDataClassImGUI(imguiId);
			break;
		default:
			DrawUnknownValue();
			break;
		}
	}

	std::string FieldInterface::GetDisplayName()
	{
		if (strlen(m_field.m_displayName) > 0)
		{
			return m_field.m_displayName;
		}

		std::string name = m_field.m_name;

		// Remove 'm_' prefixes and convert first character after to upper case.
		if (name.size() > 2 && name[0] == 'm' && name[1] == '_')
		{
			auto it = name.begin() + 2;
			*it = std::toupper(*it);

			name.erase(name.begin(), name.begin() + 2);
		}

		// Insert spaces between camel case characters or underscores.
		char prevChar = 0;
		for (auto it = name.begin(); it != name.end(); ++it)
		{
			if (prevChar == '_')
			{
				*it = std::toupper(*it);
			}

			if (std::isupper(*it) || prevChar == '_')
			{
				it = (it != name.begin() ? name.insert(it, ' ') + 1 : it);
			}
			prevChar = *it;
		}

		name.erase(std::remove(name.begin(), name.end(), '_'), name.end()); // Remove underscores.
		return name;
	}

	void FieldInterface::GetDataClassAssignmentType(std::string& outTypeStr) const
	{
		outTypeStr = DataClass::GetDataClassTypeFromField(m_field);
	}

	bool FieldInterface::HandleSpecialType(uint32_t& imguiId)
	{
		if (std::strcmp(m_field.m_typeName, "Quaternion") == 0)
		{
			uint32_t floatCount = uint32_t(m_field.m_arrayCount * m_tupleSize);
			Math::Quaternion* quaternions = m_reflector.GetFieldValue<Math::Quaternion>(m_field);

			constexpr size_t bufSize = 64;

			CLib::Vector<char> buf;
			buf.Reserve(bufSize);

			bool valueChanged = false;
			struct UserData
			{
				float& val;
				bool& valChanged;
			};

			ImGuiInputTextCallback setValue = [](ImGuiInputTextCallbackData* data) -> int
			{
				UserData* d = reinterpret_cast<UserData*>(data->UserData);
				if (d != nullptr)
				{
					d->val = float(std::atof(data->Buf));
					d->valChanged = true;
				}
				return 0;
			};

			std::stringstream ss;
			for (uint32_t i = 0; i < m_field.m_arrayCount; ++i)
			{
				Math::Quaternion& quat = quaternions[i];
				Math::Quaternion deltaQuat = Math::Quaternion::Identity();

				// Convert quaternion to euler angles in degrees for a more user-friendly way to view and modify rotation.
				Math::Vector3f eulerBefore = Math::ToDegrees(quat.ToEuler());
				Math::Vector3f eulerAfter = eulerBefore;
				for (auto& val : eulerAfter.data)
				{
					ss.precision(3);
					ss << val;

					size_t strSize = ss.str().size();
					std::memcpy(buf.Data(), ss.str().data(), strSize);
					buf.SetCount(strSize + 1);
					buf[strSize] = '\0';

					ss.str(std::string());

					ImGui::PushID(++imguiId);
					BeginArrayElem(i);

					if (Reflectron::Reflector::FieldIsBoundedBothEnds(m_field))
					{
						float valBefore = val;
						ImGui::SliderFloat("##", &val, float(m_field.m_editMinValue), float(m_field.m_editMaxValue), "%.3f", ImGuiSliderFlags_AlwaysClamp);
						if (ImGui::IsItemFocused() == false && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
						{
							ImGui::PushID(++imguiId);
							ImGui::SetTooltip("CTRL + Click to type a value instead.");
							ImGui::PopID();
						}

						valueChanged = valueChanged || valBefore != val;
					}
					else
					{
						UserData ud{ val, valueChanged };
						ImGui::InputText("##", buf.Data(), bufSize, ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CharsDecimal, setValue, &ud);
					}

					float scrollAmount = GetFieldScrollAmount();
					if (scrollAmount != 0.0f)
					{
						val += scrollAmount * 10.0f;
						valueChanged = true;
					}

					ImGui::PopID();
				}

				if (valueChanged)
				{
					// Get the delta between the new euler angles and old and apply the difference in radians to the quaternion.
					quat *= Math::Quaternion::FromEuler(Math::ToRadians(eulerAfter - eulerBefore));
				}
			}

			if (valueChanged)
			{
				m_valueModified = true;
			}

			return true;
		}

		return false;
	}

	void FieldInterface::DrawUnknownValue()
	{
		ImGui::Text("[unknown_value]");
	}

	void FieldInterface::DrawIntTupleImGUI(uint32_t& imguiId)
	{
		uint32_t intCount = uint32_t(m_field.m_arrayCount * m_tupleSize);
		int* fieldValues = m_reflector.GetFieldValue<int>(m_field);

		constexpr size_t bufSize = 64;

		CLib::Vector<char> buf;
		buf.Reserve(bufSize);

		struct UserData
		{
			int& val;
			int minVal;
			int maxVal;
			bool valueChanged = false;
		};

		ImGuiInputTextCallback setValue = [](ImGuiInputTextCallbackData* data) -> int
		{
			UserData* ud = reinterpret_cast<UserData*>(data->UserData);
			if (ud != nullptr)
			{
				ud->val = Math::Clamp(std::atoi(data->Buf), ud->minVal, ud->maxVal);
				ud->valueChanged = true;
			}
			return 0;
		};

		if (ShouldShowField(imguiId, intCount > 1))
		{
			std::stringstream ss;
			for (uint32_t i = 0; i < intCount; ++i)
			{
				buf.Clear();

				int& val = fieldValues[i];

				ss.precision(3);
				ss << val;

				size_t strSize = ss.str().size();
				std::memcpy(buf.Data(), ss.str().data(), strSize);
				buf.SetCount(strSize + 1);
				buf[strSize] = '\0';

				ss.str(std::string());

				ImGui::PushID(++imguiId);
				BeginArrayElem(i);

				UserData ud{ .val=val, .minVal=int(m_field.m_editMinValue), .maxVal=int(m_field.m_editMaxValue), .valueChanged =false };

				if (Reflectron::Reflector::FieldIsBoundedBothEnds(m_field))
				{
					int prevVal = val;
					ImGui::SliderInt("##", &val, int(m_field.m_editMinValue), int(m_field.m_editMaxValue), "%d", ImGuiSliderFlags_AlwaysClamp);
					m_valueModified |= (prevVal != val);

					if (ImGui::IsItemFocused() == false && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
					{
						ImGui::PushID(++imguiId);
						ImGui::SetTooltip("CTRL + Click to type a value instead.");
						ImGui::PopID();
					}
				}
				else
				{
					ImGui::InputText("##", buf.Data(), bufSize, ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CharsDecimal, setValue, &ud);
				}

				float scrollAmount = GetFieldScrollAmount();
				if (scrollAmount > 0.0f)
				{
					val = Math::Clamp(val + int(std::fmaxf(1.0f, scrollAmount)), ud.minVal, ud.maxVal);
				}
				else if (scrollAmount < 0.0f)
				{
					val = Math::Clamp(val + int(std::fminf(-1.0f, scrollAmount)), ud.minVal, ud.maxVal);
				}

				m_valueModified |= ud.valueChanged;

				ImGui::PopID();
			}
		}
	}

	void FieldInterface::DrawBoolTupleImGUI(uint32_t& imguiId)
	{
		uint32_t boolCount = uint32_t(m_field.m_arrayCount * m_tupleSize);
		bool* fieldValues = m_reflector.GetFieldValue<bool>(m_field);

		if (ShouldShowField(imguiId, boolCount > 1))
		{
			for (uint32_t i = 0; i < boolCount; ++i)
			{
				ImGui::PushID(++imguiId);
				BeginArrayElem(i);

				bool prevVal = fieldValues[i];
				ImGui::Checkbox("##", &fieldValues[i]);
				m_valueModified = prevVal != fieldValues[i];

				ImGui::PopID();
			}
		}
	}

	void FieldInterface::DrawFloatTupleImGUI(uint32_t& imguiId)
	{
		uint32_t floatCount = uint32_t(m_field.m_arrayCount * m_tupleSize);
		float* fieldValues = m_reflector.GetFieldValue<float>(m_field);

		constexpr size_t bufSize = 64;

		CLib::Vector<char> buf;
		buf.Reserve(bufSize);

		struct UserData
		{
			float& val;
			float minVal;
			float maxVal;
			bool valueChanged;
		};

		ImGuiInputTextCallback setValue = [](ImGuiInputTextCallbackData* data) -> int
		{
			UserData* ud = reinterpret_cast<UserData*>(data->UserData);
			if (ud != nullptr)
			{
				ud->val = Math::Clamp<float>(float(std::atof(data->Buf)), ud->minVal, ud->maxVal);
				ud->valueChanged = true;
			}
			return 0;
		};

		if (ShouldShowField(imguiId, floatCount > 1))
		{
			std::stringstream ss;
			for (uint32_t i = 0; i < floatCount; ++i)
			{
				buf.Clear();

				float& val = fieldValues[i];

				ss.precision(3);
				ss << val;

				size_t strSize = ss.str().size();
				std::memcpy(buf.Data(), ss.str().data(), strSize);
				buf.SetCount(strSize + 1);
				buf[strSize] = '\0';

				ss.str(std::string());


				ImGui::PushID(++imguiId);
				BeginArrayElem(i);

				UserData ud{ .val=val, .minVal=float(m_field.m_editMinValue), .maxVal=float(m_field.m_editMaxValue), .valueChanged=false };
				if (Reflectron::Reflector::FieldIsBoundedBothEnds(m_field))
				{
					float prevVal = val;
					ImGui::SliderFloat("##", &val, float(m_field.m_editMinValue), float(m_field.m_editMaxValue), "%.3f", ImGuiSliderFlags_AlwaysClamp);
					m_valueModified |= (val != prevVal);

					if (ImGui::IsItemFocused() == false && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
					{
						ImGui::PushID(++imguiId);
						ImGui::SetTooltip("CTRL + Click to type a value instead.");
						ImGui::PopID();
					}
				}
				else
				{
					ImGui::InputText("##", buf.Data(), bufSize, ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CharsDecimal, setValue, &ud);

					m_valueModified |= ud.valueChanged;
				}

				float scrollAmount = GetFieldScrollAmount();
				if (scrollAmount != 0.0f)
				{
					val = Math::Clamp(val + scrollAmount, ud.minVal, ud.maxVal);
					m_valueModified = true;
				}

				ImGui::PopID();
			}
		}
	}

	void FieldInterface::DrawDoubleTupleImGUI(uint32_t& imguiId)
	{
		uint32_t floatCount = uint32_t(m_field.m_arrayCount * m_tupleSize);
		double* fieldValues = m_reflector.GetFieldValue<double>(m_field);

		constexpr size_t bufSize = 64;

		CLib::Vector<char> buf;
		buf.Reserve(bufSize);

		struct UserData
		{
			double& val;
			double minVal;
			double maxVal;
			bool valueChanged;
		};

		ImGuiInputTextCallback setValue = [](ImGuiInputTextCallbackData* data) -> int
		{
			UserData* ud = reinterpret_cast<UserData*>(data->UserData);
			if (ud != nullptr)
			{
				ud->val = Math::Clamp(std::atof(data->Buf), ud->minVal, ud->maxVal);
				ud->valueChanged = true;
			}
			return 0;
		};

		if (ShouldShowField(imguiId, floatCount > 1))
		{
			std::stringstream ss;
			for (uint32_t i = 0; i < floatCount; ++i)
			{
				buf.Clear();

				double& val = fieldValues[i];

				ss.precision(3);
				ss << val;

				size_t strSize = ss.str().size();
				std::memcpy(buf.Data(), ss.str().data(), strSize);
				buf.SetCount(strSize + 1);
				buf[strSize] = '\0';

				ss.str(std::string());

				ImGui::PushID(++imguiId);
				BeginArrayElem(i);

				if (Reflectron::Reflector::FieldIsBoundedBothEnds(m_field))
				{
					double prevVal = val;

					float valf = float(val);
					ImGui::SliderFloat("##", &valf, float(m_field.m_editMinValue), float(m_field.m_editMaxValue), "%.3f", ImGuiSliderFlags_AlwaysClamp);
					val = valf;

					m_valueModified |= (val != prevVal);

					if (ImGui::IsItemFocused() == false && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
					{
						ImGui::PushID(++imguiId);
						ImGui::SetTooltip("CTRL + Click to type a value instead.");
						ImGui::PopID();
					}
				}
				else
				{
					UserData ud{ val, m_field.m_editMinValue, m_field.m_editMaxValue };
					ImGui::InputText("##", buf.Data(), bufSize, ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CharsDecimal, setValue, &ud);

					m_valueModified |= ud.valueChanged;
				}

				float scrollAmount = GetFieldScrollAmount();
				if (scrollAmount != 0.0f)
				{
					val = Math::Clamp(val + double(scrollAmount), m_field.m_editMinValue, m_field.m_editMaxValue);
					m_valueModified = true;
				}

				ImGui::PopID();
			}
		}
	}

	void FieldInterface::DrawStringImGUI(uint32_t& imguiId)
	{
		uint32_t stringCount = uint32_t(m_field.m_arrayCount);
		std::string* fieldStrings = m_reflector.GetFieldValue<std::string>(m_field);

		constexpr size_t bufSize = 512;

		CLib::Vector<char> buf;
		buf.Reserve(bufSize);

		ImGuiInputTextCallback setValue = [](ImGuiInputTextCallbackData* data) -> int
		{
			std::string* dstVal = reinterpret_cast<std::string*>(data->UserData);
			*dstVal = data->Buf;
			return 0;
		};

		if (ShouldShowField(imguiId, stringCount > 1))
		{
			std::stringstream ss;
			for (uint32_t i = 0; i < stringCount; ++i)
			{
				buf.Clear();

				std::string& val = fieldStrings[i];

				ss << val;

				std::memcpy(buf.Data(), val.data(), val.size());
				buf.SetCount(val.size() + 1);
				buf[val.size()] = '\0';

				ss.str(std::string());

				ImGui::PushID(++imguiId);
				BeginArrayElem(i);
				ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

				std::string prevVal = val;
				ImGui::InputText("##", buf.Data(), bufSize, ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_ElideLeft, setValue, &val);
				m_valueModified = (prevVal != val);

				ImGui::PopItemWidth();
				if (ImGui::IsItemFocused() == false && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
				{
					ImGui::PushID(++imguiId);
					ImGui::SetTooltip("%s", val.data());
					ImGui::PopID();
				}
				ImGui::PopID();
			}
		}
	}

	void FieldInterface::DrawDataClassImGUI(uint32_t& imguiId)
	{
		uint32_t guidCount = 0;
		void* fieldValue = m_reflector.GetFieldValue(m_field);
		ObjectPtr* ptrs = nullptr;

		if (m_field.m_size == sizeof(ObjectPtrArray))
		{
			ObjectPtrArray* arr = reinterpret_cast<ObjectPtrArray*>(fieldValue);

			ptrs = arr->Data();
			guidCount = arr->Count();
		}
		else
		{
			ptrs = reinterpret_cast<ObjectPtr*>(fieldValue);
			guidCount = m_field.m_size / sizeof(ObjectPtr);
		}

		if (ShouldShowField(imguiId, guidCount > 1))
		{
			std::stringstream ss;
			for (uint32_t i = 0; i < guidCount; ++i)
			{
				ObjectPtr& ptr = ptrs[i];
				DataClass* dc = ptr.GetPtr();

				const char* val = "NULL";
				if(dc != nullptr)
				{
					ss << dc->GetDataClassInstanceName() << ' ';

					val = dc->GetReflection().GetTypeName();
				}

				ss << '[';
				ss << val;
				ss << ']';

				ImGui::PushID(++imguiId);
				BeginArrayElem(i);

				bool selected = false;
				ImGui::Selectable(ss.str().c_str(), &selected, ImGuiSelectableFlags_AllowDoubleClick);
				ImGui::Spacing();
				if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				{
					// Trigger data class selection...
					m_assignmentRequested = true;
					m_selectedDataClass = &ptr;
				}
				else if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					if (dc == nullptr || ImGui::IsMouseClicked(ImGuiMouseButton_Right))
					{
						// Trigger data class selection...
						m_assignmentRequested = true;
					}
					m_selectedDataClass = &ptr;
				}
				else if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
				{
					m_nullAssignmentRequested = true;
					m_selectedDataClass = &ptr;
				}
				if (ImGui::IsItemFocused() == false && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
				{
					ImGui::PushID(++imguiId);
					if(dc != nullptr)
					{
						ss << "\n\nDouble-click to view and modify data. Right-click to assign a new reference. Middle-click to erase the reference.";

						ImGui::SetTooltip("%s", ss.str().c_str());
					}
					else 
					{
						ImGui::SetTooltip("Null reference. Double-click to assign a reference.");
					}
					ImGui::PopID();
				}
				ImGui::PopID();

				ss.str(std::string());
			}
		}
	}

	bool FieldInterface::ShouldShowField(uint32_t& imguiId, bool allowCollapse)
	{
		if (allowCollapse == false)
			return true;

		ImGui::PushID(++imguiId);
		ImGui::PushStyleColor(ImGuiCol_Header, 0);

		bool expand = true;
		expand = ImGui::CollapsingHeader("##", ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_DefaultOpen);

		ImGui::PopStyleColor();
		ImGui::PopID();

		return expand;
	}

	void FieldInterface::BeginArrayElem(const uint32_t& index)
	{
		if (m_field.m_arrayCount > 1)
		{
			ImGui::TableNextColumn();
			ImGui::Text("[%i]", index);
			ImGui::TableNextColumn();
		}
	}
}
