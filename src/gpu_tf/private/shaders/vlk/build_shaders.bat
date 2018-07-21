%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vertex -std=450core -mfmt=c default_vs.glsl -o default_vs.h
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=fragment -std=450core -mfmt=c default_ps.glsl -o default_ps.h
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=compute -std=450core -mfmt=c default_cs.glsl -o default_cs.h