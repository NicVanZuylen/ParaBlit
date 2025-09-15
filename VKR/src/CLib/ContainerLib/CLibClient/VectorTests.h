#pragma once
#include "CLib/Vector.h"
#include <iostream>

namespace CLibTest
{
	void VectorTest()
	{
		std::cout << "------------------------ Vector Tests ------------------------" << std::endl;

		CLib::Vector<int, 32> vec;

		std::cout << "Push Back: 0 - 4" << std::endl << std::endl;
		vec.PushBack(0);
		vec.PushBack(1);
		vec.PushBack(2);
		vec.PushBack(3);
		vec.PushBack(4);

		std::cout << "Push Back Unitialized: 5" << std::endl << std::endl;
		vec.PushBack() = 5;

		std::cout << "Push Back Initialized: 6" << std::endl << std::endl;
		vec.PushBackInit(6); // 6 is a constructor parameter in this case.

		std::cout << "Copy vec to vec2:" << std::endl << std::endl;
		CLib::Vector<int, 32> vec2 = vec;
		std::cout << "Move vec2 to vec3:" << std::endl << std::endl;
		CLib::Vector<int, 32> vec3 = std::move(vec2);

		std::cout << "Append vec4 to vec3:" << std::endl << std::endl;
		CLib::Vector<int> vec4 = { 7, 8, 9 };
		vec3 += vec4;

		std::cout << "Reading vec3...:" << std::endl << std::endl;

		std::cout << "Index Loop All Contents:" << std::endl << std::endl;
		for (unsigned int i = 0; i < vec3.Count(); ++i)
			std::cout << vec3[i] << std::endl;
		std::cout << std::endl;

		std::cout << "Auto Loop All Contents:" << std::endl << std::endl;
		for (auto& val : vec3)
			std::cout << val << std::endl;
		std::cout << std::endl;

		std::cout << "Front:" << std::endl << std::endl;
		std::cout << vec3.Front() << std::endl << std::endl;;

		std::cout << "Back:" << std::endl << std::endl;
		std::cout << vec3.Back() << std::endl << std::endl;

		std::cout << "------------------------ Vector Tests Complete ------------------------" << std::endl;
	}
};