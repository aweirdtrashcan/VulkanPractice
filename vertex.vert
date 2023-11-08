#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_spirv_intrinsics : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;

layout(set = 0, binding = 0) uniform GlobalUniform 
{
	mat4 projection;
	mat4 view;
	mat4 tempModel;
} globalUniform;

//layout(set = 0, binding = 1) uniform PerObjectUniform
//{
//	mat4 mvp;
//} perObject;

layout(location = 0) out vec4 outColor;

void main()
{
	gl_Position = globalUniform.projection * globalUniform.view * globalUniform.tempModel * vec4(inPos.xyz, 1.0);
	debugPrintfEXT("inPos %f %f %f\n", inPos.x, inPos.y, inPos.z);
	debugPrintfEXT("inColor %f %f %f %f\n", inColor.x, inColor.y, inColor.z, inColor.w);
	debugPrintfEXT("projection %f %f %f %f\n", globalUniform.projection[0], globalUniform.projection[1], globalUniform.projection[2], globalUniform.projection[3]);
	outColor = inColor;
}