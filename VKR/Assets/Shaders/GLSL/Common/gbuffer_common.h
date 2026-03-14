#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

void PackNormal(inout vec3 normal)
{
    normal = normal * 0.5 + 0.5;
}

void UnpackNormal(inout vec3 normal)
{
    normal = normal * 2.0 - 1.0;
}

// Convert to FP16 and pack into specified bits of precision.
u16vec3 PackColorUnsigned(vec3 colorF32, const uint precisionBits)
{
    f16vec3 colorF16 = f16vec3(colorF32);
    uvec3 f16asUint = float16BitsToUint16(colorF16);
    
    return u16vec3((f16asUint << precisionBits) / 0x7C00);
}

uint16_t QuantizeColor(uint16_t unquantizedColor, const uint quantBits)
{
	const uint16_t quantRange = uint16_t(0xFFFF) >> (16 - quantBits);

	float quantf = round((float(unquantizedColor) / 4095.0) * float(quantRange));

	return uint16_t(quantf) & quantRange;
}

u16vec3 EncodeColor(vec3 colorF32, const uint precisionBits)
{
    u16vec3 packed = PackColorUnsigned(colorF32, precisionBits);
    u16vec3 quantized = u16vec3
    (
        QuantizeColor(packed.r, precisionBits),
        QuantizeColor(packed.g, precisionBits),
        QuantizeColor(packed.b, precisionBits)
    );
    return quantized;
}

vec3 EncodeColorF(vec3 colorF32, const uint precisionBits)
{
    const uint16_t valueRange = uint16_t(0xFFFF) >> (16 - precisionBits);

    u16vec3 encoded = EncodeColor(colorF32, precisionBits);

    return vec3(encoded) / valueRange;
}

// Inverse of PackColorUnsigned().
f16vec3 UnpackColorUnsigned(uvec3 colorPacked, const uint precisionBits)
{
	uvec3 unpacked = (colorPacked * uint(0x7C00)) >> precisionBits;
    
    return uint16BitsToHalf(u16vec3(unpacked));
}

uint16_t UnquantizeColor(uint16_t quantizedColor, const uint quantBits)
{
    const uint16_t quantRange = uint16_t(0xFFFF) >> (16 - quantBits);

    uint16_t uQuant = uint16_t(round((float(quantizedColor) / quantRange) * 4095.0));
    return uQuant;
}

f16vec3 DecodeColor(u16vec3 encodedColorF16, const uint precisionBits)
{
    u16vec3 unquantized = u16vec3
    (
        UnquantizeColor(encodedColorF16.r, precisionBits),
        UnquantizeColor(encodedColorF16.g, precisionBits),
        UnquantizeColor(encodedColorF16.b, precisionBits)
    );
    f16vec3 unpacked = UnpackColorUnsigned(unquantized, precisionBits);

    return unpacked;
}

vec3 DecodeColorF(vec3 encodedColorF16, const uint precisionBits)
{
    const uint16_t valueRange = uint16_t(0xFFFF) >> (16 - precisionBits);
    u16vec3 asInt = u16vec3(encodedColorF16 * valueRange);
    return DecodeColor(asInt, precisionBits);
}