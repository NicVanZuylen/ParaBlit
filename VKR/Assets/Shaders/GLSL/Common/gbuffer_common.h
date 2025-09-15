
void PackNormal(inout vec3 normal)
{
    normal = normal * 0.5 + 0.5;
}

void UnpackNormal(inout vec3 normal)
{
    normal = normal * 2.0 - 1.0;
}