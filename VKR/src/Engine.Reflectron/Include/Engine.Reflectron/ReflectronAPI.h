#ifndef REFLECTRON_INCL_API
#define REFLECTRON_INCL_API

#include <cstdint>
#include <cstddef>
#include <limits>

#define REFLECTRON_TIMESTAMP(...)

/*
Define the class who's body this is implemented in as reflectable by Reflectron.
*/
#define REFLECTRON_CLASS(...)

/*
Define the following field as reflectable by Reflectron. Field must belong to a class where REFLECTRON_CLASS() has been implemented in its body.
NOTE: If the field is an enum the 'enum' flag should be specified as an argument.
*/
#define REFLECTRON_FIELD(...)

#define REFLECTRON_INFINITY std::numeric_limits<double>::infinity()

struct ReflectronFieldData
{
	const char* m_name;
	const char* m_typeName;
	size_t m_offset;
	size_t m_size;
	size_t m_arrayCount;
	double m_editMinValue;
	double m_editMaxValue;
	const char* m_displayName;
};

#endif