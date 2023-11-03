%VULKAN_SDK%\Bin\glslc.exe -fshader-stage=vert vertex.glsl -o x64\Release\Shaders\vert.spv
%VULKAN_SDK%\Bin\glslc.exe -fshader-stage=frag fragment.glsl -o x64\Release\Shaders\frag.spv
@pause