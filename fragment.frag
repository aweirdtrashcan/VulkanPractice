#version 450
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_spirv_intrinsics : enable

layout(location = 0) in vec4 inColor;
layout(location = 0) out vec4 outColor;

void main()
{
	outColor = inColor;
	debugPrintfEXT("frag: %f %f %f %f\n", inColor.x, inColor.y, inColor.z, inColor.z);
}