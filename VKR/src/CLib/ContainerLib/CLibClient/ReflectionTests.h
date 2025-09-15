#pragma once
#include <iostream>

#include "CLib/Reflection.h"

class ReflectionTestClass
{
public:

	ReflectionTestClass()
	{
		m_testInt = 64;
		m_testFloat = 64.0f;
		m_testString = "REFLECTION TEST SUCCESS!";
		m_testArray[0] = 1.0f;
		m_testArray[1] = 2.0f;
		m_testArray[2] = 3.0f;
		m_testArray[3] = 4.0f;

		std::cout << "CONSTRUCT: ReflectionTestClass" << std::endl;
	}

	~ReflectionTestClass()
	{
		std::cout << "DESTRUCT: ReflectionTestClass" << std::endl;
	}

	virtual void VTablePtrDummy() {};

private:

	CLIB_REFLECTABLE
	(
		ReflectionTestClass,
		(int) m_testInt,
		(float) m_testFloat,
		(const char*) m_testString,
		CLIB_ARRAY(float, 4) m_testArray
	)
};
CLIB_REFLECTABLE_CLASS(ReflectionTestClass)

namespace CLibTest
{
	void ReflectionTest()
	{
		std::cout << "------------------------ Reflection Tests ------------------------\n\n";

		CLib::Reflection::TReflectableClass<ReflectionTestClass> testClass;
		CLib::Reflection::PlacedConstructClass("ReflectionTestClass", testClass.Get());

		CLib::Reflection::Reflector testReflector(&testClass);

		float* testArrValues = testReflector.GetFieldWithName<float>("m_testArray");
		for (uint32_t i = 0; i < 4; ++i)
		{
			std::cout << "Test Array Value " << i << ": " << testArrValues[i] << "\n";
		}

		auto& fieldData = testReflector.GetFieldDataWithName("m_testInt");
		if (fieldData.m_typeHash == typeid(int).hash_code())
		{
			std::cout << "m_testInt is an int!" << "\n";
		}

		const char** testStringValue = testReflector.GetFieldWithName<const char*>("m_testString");
		std::cout << *testStringValue << "\n";

		CLib::Reflection::PlacedDestructClass("ReflectionTestClass", testClass.Get());
		CLib::Reflection::PlacedConstructClass("ReflectionTestClass", testClass.Get());

		std::cout << "------------------------ Reflection Tests Complete ------------------------\n";
	}
}