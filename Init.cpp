#include <windows.h>
#include "avisynth.h"
#include "Convert.h"
// #include "Shader.h"
//
//AVSValue __cdecl Create_Shader(AVSValue args, void* user_data, IScriptEnvironment* env) {
//	return new Shader(
//		args[0].AsClip(),			// source clip
//		env);						// env is the link to essential informations, always provide it
//}

AVSValue __cdecl Create_ConvertToShader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	return new ConvertToShader(
		args[0].AsClip(),			// source clip
		args[1].AsInt(4),			// precision
		env);						// env is the link to essential informations, always provide it
}

AVSValue __cdecl Create_ConvertFromShader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	return new ConvertFromShader(
		args[0].AsClip(),			// source clip
		args[1].AsInt(4),			// precision
		env);						// env is the link to essential informations, always provide it
}

const AVS_Linkage *AVS_linkage = 0;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors) {
	AVS_linkage = vectors;
	//env->AddFunction("Shader", "c", Create_Shader, 0);
	env->AddFunction("ConvertToShader", "c[precision]i", Create_ConvertToShader, 0);
	env->AddFunction("ConvertFromShader", "c[precision]i", Create_ConvertFromShader, 0);
	return "Shader plugin";
}
