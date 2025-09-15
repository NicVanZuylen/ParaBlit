#include "ReflectionGenerator.h"

#include <chrono>
#include <cassert>
#include <fstream>

namespace Reflectron
{
	ReflectionGenerator::ReflectionGenerator(const char* name, const char* dbName, const char* sourceDirectory)
		: EncoderBase(name, dbName, sourceDirectory)
	{
		std::vector<AssetEncoder::FileInfo> fileInfos;
		RecursiveSearchDirectoryForExtension(sourceDirectory, fileInfos, ".h");

		for (auto& fileInfo : fileInfos)
		{
			std::filesystem::path fileDir = fileInfo.m_fileName;
			std::filesystem::path fileName = fileDir.filename();
			std::filesystem::path generatedFileName = fileName.replace_extension().string() + "_generated.h";

			std::filesystem::path generatedFileDir = fileDir;
			generatedFileDir.replace_filename(generatedFileName);

			std::ios::openmode genOpenFlags = std::ios::in | std::ios::ate;
			std::fstream generatedFile(generatedFileDir, genOpenFlags);
			uint64_t generatedTimestamp = ~uint64_t(0);
			bool generatedFileExists = generatedFile.good();
			if (generatedFileExists)
			{
				size_t size = generatedFile.tellg();
				generatedFile.seekg(std::ios::beg);

				// Generated file already exists. Check timestamp.
				std::string fileSource;
				fileSource.resize(size);
				generatedFile.read(fileSource.data(), size);
				generatedFile.close();

				generatedTimestamp = GetTimestamp(fileSource);
			}

			bool needToGenerate = generatedFileExists == false || generatedTimestamp < fileInfo.m_lastModifiedTime;
			if (needToGenerate)
			{
				std::string generatedCode;
				std::ios::openmode openFlags = std::ios::_Nocreate | std::ios::in | std::ios::ate;
				std::fstream file(fileInfo.m_fileName, openFlags);
				bool needsReflectionGenerated = false;
				if (file.good())
				{
					size_t size = file.tellg();
					file.seekg(std::ios::beg);

					std::string fileSource;
					fileSource.resize(size);
					file.read(fileSource.data(), size);
					file.close();

					needsReflectionGenerated = FileNeedsReflection(fileSource);
					if (needsReflectionGenerated == true)
					{
						InsertGeneratedHeader(fileSource, generatedFileName.string());

						file.open(fileInfo.m_fileName, std::ios::out | std::ios::trunc);
						file << fileSource.c_str();
						file.close();

						std::filesystem::directory_entry fileEntry;
						fileEntry.assign(fileInfo.m_fileName);
						uint64_t newTimestamp = std::chrono::duration_cast<std::chrono::seconds>(fileEntry.last_write_time().time_since_epoch()).count();
						GenerateReflectionCode(generatedCode, fileSource, fileName.string(), generatedFileName.string(), newTimestamp);
					}
				}

				if (needsReflectionGenerated)
				{
					generatedFile.open(generatedFileDir, std::ios::out | std::ios::trunc);

					generatedFile.write(generatedCode.c_str(), generatedCode.size());
					generatedFile.close();
				}
			}
		}
	}

	ReflectionGenerator::~ReflectionGenerator()
	{
	}

	struct FieldInfo
	{
		std::string m_typeName;
		std::string m_name;
		size_t m_arrayCount;
	};

	uint64_t ReflectionGenerator::GetTimestamp(std::string& src)
	{
		size_t timestampPos = src.find("REFLECTRON_TIMESTAMP");
		if (timestampPos == std::string::npos)
			return 0;

		timestampPos = src.find_first_of('(', timestampPos) + 1;
		size_t timestampEnd = src.find_first_of(')');

		std::string timestamp = src.substr(timestampPos, timestampEnd - timestampPos);
		return std::strtoull(timestamp.c_str(), nullptr, 0);
	}

	bool IsPosCommented(const size_t& pos, const std::string& src)
	{
		size_t multilineCommentBeforeStart = src.rfind("/*", pos);
		size_t multilineCommentBeforeEnd = src.rfind("*/", pos);
		size_t multilineCommentAfterStart = src.find("/*", pos);
		size_t multilineCommentAfterEnd = src.find("*/", pos);

		bool isCommented = multilineCommentBeforeStart != std::string::npos
			&& (multilineCommentBeforeEnd == std::string::npos || multilineCommentBeforeStart > multilineCommentBeforeEnd)
			&& multilineCommentAfterEnd != std::string::npos
			&& (multilineCommentAfterStart == std::string::npos || multilineCommentAfterEnd < multilineCommentAfterStart);

		// Search for a double-slash '//' on the same line as 'pos' before 'pos' to indicate the code on the line of 'pos' is commented out.
		size_t commentPos = src.rfind("//", pos);
		isCommented |= (commentPos != std::string::npos
			&& src.find_first_of('\n', commentPos) == src.find_first_of('\n', pos)); // Check if comment slashes are on the same line.

		return isCommented;
	};

	bool ReflectionGenerator::FileNeedsReflection(const std::string& src)
	{
		size_t pos = src.find("REFLECTRON_CLASS");
		return pos != std::string::npos && IsPosCommented(pos, src) == false;
	}

	void RemoveSpaces(std::string& str)
	{
		str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
	}

	void RemoveWhitespace(std::string& str)
	{
		str.erase(std::remove(str.begin(), str.end(), '\t'), str.end());
		str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
		str.erase(std::remove(str.begin(), str.end(), '\v'), str.end());
		str.erase(std::remove(str.begin(), str.end(), '\f'), str.end());
		str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
	}

	void ReflectionGenerator::GenerateReflectionCode(std::string& outfileStr, const std::string& src, const std::string& fileName, const std::string& generatedFilename, uint64_t timestamp)
	{
		const std::string whiteSpace = " \t\n\v\f\r";
		std::string genSrc = "#pragma once\n#include \"" + fileName + ".h\"";

		std::string className;
		std::vector<std::stringstream> macros;

		// Find an implementation of REFLECTRON_CLASS marker.
		size_t markerPos = 0;
		markerPos = src.find("REFLECTRON_CLASS", markerPos);
		while (markerPos != std::string::npos)
		{
			if (IsPosCommented(markerPos, src))
			{
				markerPos = src.find("REFLECTRON_CLASS", markerPos + 1);
				continue;
			}

			// Find most recent class declaration.
			size_t classKeyword = 0;
			{
				const std::string keyword = "class";
				classKeyword = src.rfind(keyword, markerPos) + keyword.size();
				assert(classKeyword != std::string::npos && "Expected class declaration before REFLECTRON_CLASS marker.");
				size_t classnameBeg = std::min<size_t>(src.find_first_not_of(' ', classKeyword), src.find_first_not_of('\n', classKeyword)) + 1;
				size_t classnameEnd = std::min<size_t>(src.find_first_of(' ', classnameBeg), src.find_first_of('\n', classnameBeg));
				className = src.substr(classnameBeg, classnameEnd - classnameBeg);
			}

			// Find beginning and end of class definition.
			size_t classBegin, classEnd = 0;
			{
				size_t openCount = 1;
				classBegin = src.find_first_of('{', classKeyword);

				size_t curPos = classBegin + 1;
				classEnd = curPos + 1;
				while (openCount > 0)
				{
					size_t findOpen = src.find_first_of('{', classEnd);
					size_t findClosed = src.find_first_of('}', classEnd);

					if (findOpen != std::string::npos && findOpen < findClosed)
					{
						classEnd = findOpen + 1;
						openCount++;
					}
					else if(findClosed != std::string::npos)
					{
						classEnd = findClosed + 1;
						openCount--;
					}
					else
					{
						classEnd++;
					}
				}

				classEnd++;
			}
			std::string classSrc = src.substr(classBegin, classEnd - classBegin);

			// Find fields marked with REFLECTRON_FIELD()
			std::vector<FieldInfo> fields;
			{
				size_t pos = 0;
				while (pos != std::string::npos)
				{
					std::vector<std::string> fieldArgs;

					pos = classSrc.find("REFLECTRON_FIELD", pos);
					GetReflectionFieldArgs(classSrc, pos, fieldArgs);

					bool isEnum = false; // Enums must be declared as an argument. Otherwise there is no way to determine if the type is an enum or not.
					for (auto& arg : fieldArgs)
					{
						if (arg == "enum")
						{
							isEnum = true;
						}
					}

					if (pos != std::string::npos)
					{
						size_t declStart = classSrc.find_first_of(whiteSpace, pos);
						assert(declStart != std::string::npos);

						size_t beginTypeName = classSrc.find_first_not_of(whiteSpace, declStart);

						if (IsPosCommented(pos, classSrc) || IsPosCommented(beginTypeName, classSrc) == true)
						{
							pos = pos + 1;
							continue;
						}
						fields.emplace_back();
						FieldInfo& fieldInfo = fields.back();

						size_t endTypeName = classSrc.find_first_of(whiteSpace, beginTypeName);
						if (isEnum == false)
						{
							fieldInfo.m_typeName = classSrc.substr(beginTypeName, endTypeName - beginTypeName);
						}
						else
						{
							fieldInfo.m_typeName = "int"; // Enums are a special case where the name may be anything but the underlying type is an int.
						}
						RemoveWhitespace(fieldInfo.m_typeName);

						size_t beginVarName = classSrc.find_first_not_of(whiteSpace, endTypeName);
						size_t expressionTerminator = classSrc.find_first_of(';', beginVarName);
						size_t endVarName = std::min<size_t>(classSrc.find_first_of(whiteSpace, beginVarName), expressionTerminator);
						endVarName = std::min<size_t>(endVarName, classSrc.find_first_of('[', beginVarName));
						fieldInfo.m_name = classSrc.substr(beginVarName, endVarName - beginVarName);
						RemoveWhitespace(fieldInfo.m_name);

						fieldInfo.m_arrayCount = 1;
						size_t beginArrayCount = classSrc.find_first_of('[', endVarName);
						if (beginArrayCount < expressionTerminator)
						{
							size_t endArrayCount = classSrc.find_first_of(']', beginArrayCount);
							beginArrayCount += 1;
							std::string arrayCountStr = classSrc.substr(beginArrayCount, endArrayCount - beginArrayCount);
							fieldInfo.m_arrayCount = std::atoi(arrayCountStr.c_str());
						}

						assert(fieldInfo.m_typeName.empty() == false && fieldInfo.m_name.empty() == false);
						pos = endVarName;
					}
				}
			}

			// Generate reflection macro code.
			std::stringstream& macro = macros.emplace_back();
			{
				const char* macroEndl = "\t\\\n";
				std::string reflectionStructName = "Reflectron_Generated";
				macro << "#define REFLECTRON_GENERATED_" << className << "(...)" << macroEndl;
				macro << "struct " << reflectionStructName << macroEndl << "{" << macroEndl;
				macro << "\tstatic constexpr size_t FieldCount = " << (fields.size() > 0 ? fields.size() : 1) << ";" << macroEndl;
				macro << "\tstatic constexpr const char* ClassName = \"" << className << "\";" << macroEndl;
				macro << "\tconst ReflectronFieldData m_fieldData[FieldCount] =" << macroEndl;
				macro << "\t{" << macroEndl;

				for (auto& fieldInfo : fields)
				{
					macro << "\t\t{ ";
					macro << "\"" << fieldInfo.m_name << "\", ";
					macro << "\"" << fieldInfo.m_typeName << "\", ";
					macro << "offsetof(" + className << ", " + fieldInfo.m_name << "), ";
					macro << "sizeof(" << fieldInfo.m_name << "), ";
					macro << fieldInfo.m_arrayCount;
					macro << " }," << macroEndl;
				}
				macro << "\t};" << macroEndl;
				macro << "};" << macroEndl;
			}

			markerPos = src.find("REFLECTRON_CLASS", markerPos + 1);
		}

		// Finalize file contents.
		std::stringstream fileStrStream;
		{
			fileStrStream.clear();

			fileStrStream << "/* GENERATED CODE: This code was generated by Reflectron. */" << std::endl;
			fileStrStream << "#pragma once" << std::endl;
			fileStrStream << "#include \"Engine.Reflectron/ReflectronAPI.h\"" << std::endl;
			fileStrStream << "REFLECTRON_TIMESTAMP(" << timestamp << ")" << std::endl;
			fileStrStream << std::endl;

			for (auto& macro : macros)
			{
				fileStrStream << macro.str() << std::endl;
			}
		}

		outfileStr = fileStrStream.str();
	}

	void ReflectionGenerator::InsertGeneratedHeader(std::string& src, const std::string& generatedFilename)
	{
		size_t inclBegin = src.find("#pragma once");
		if (inclBegin == std::string::npos)
			inclBegin = 0;
		inclBegin = src.find_first_of('\n', inclBegin) + 1;

		std::string includeName = "\"" + generatedFilename + "\"";
		size_t includeNamePos = src.find(includeName);
		if (includeNamePos == std::string::npos)
		{
			std::string includeStatement = "#include " + includeName + "\n";
			src.insert(inclBegin, includeStatement);
		}
	}

	void ReflectionGenerator::GetReflectionFieldArgs(std::string& src, size_t pos, std::vector<std::string>& outArgs)
	{
		size_t start = src.find_first_of('(', pos) + 1;
		size_t end = src.find_first_of(')', pos);
		if (end == start)
		{
			return;
		}

		std::string argsStr = src.substr(start, end - start + 1);
		RemoveSpaces(argsStr);
		RemoveWhitespace(argsStr);

		size_t prevSeparator = pos;
		size_t separator = pos;



		size_t argPos = 0;
		size_t argEnd = argsStr.find_first_of(')');
		while (separator != std::string::npos)
		{
			separator = argsStr.find_first_of(',', argPos);

			std::string arg = argsStr.substr(argPos, std::min(argEnd, separator) - argPos);
			outArgs.push_back(arg);

			argPos = separator + 1;
		}
	}
}