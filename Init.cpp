#include <windows.h>
#include "avisynth.h"
#include "Convert.h"
 #include "Shader.h"

AVSValue __cdecl Create_Shader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	return new Shader(
		args[0].AsClip(),			// source clip
		args[1].AsString(""),		// shader path
		args[2].AsString("main"),	// entry point
		args[3].AsString(""),		// shader model
		args[4].AsInt(4),			// precision
		args[5].AsString(""),		// param 1
		args[6].AsString(""),		// param 2
		args[7].AsString(""),		// param 3
		args[8].AsString(""),		// param 4
		args[9].AsString(""),		// param 5
		args[10].AsString(""),		// param 6
		args[11].AsString(""),		// param 7
		args[12].AsString(""),		// param 8
		args[13].AsString(""),		// param 9
		args[14].AsClip(),			// clip 1
		args[15].AsClip(),			// clip 2
		args[16].AsClip(),			// clip 3
		args[17].AsClip(),			// clip 4
		env);						// env is the link to essential informations, always provide it
}

AVSValue __cdecl Create_ConvertToShader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	return new ConvertToShader(
		args[0].AsClip(),			// source clip
		args[1].AsInt(4),			// precision
		env);						// env is the link to essential informations, always provide it
}

AVSValue __cdecl Create_ConvertFromShader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	PClip input = args[0].AsClip();
	//if (input->GetVideoInfo().IsYV12())
	//	input = env->Invoke("ConvertToYV24", input).AsClip();

	return new ConvertFromShader(
		input,						// source clip
		args[1].AsInt(4),			// precision
		env);						// env is the link to essential informations, always provide it
}

const AVS_Linkage *AVS_linkage = 0;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors) {
	AVS_linkage = vectors;
	env->AddFunction("Shader", "c[path]s[entryPoint]s[shaderModel]s[precision]i[param1]s[param2]s[param3]s[param4]s[param5]s[param6]s[param7]s[param8]s[param9]s[clip1]c[clip2]c[clip3]c[clip4]c", Create_Shader, 0);
	env->AddFunction("ConvertToShader", "c[precision]i", Create_ConvertToShader, 0);
	env->AddFunction("ConvertFromShader", "c[precision]i", Create_ConvertFromShader, 0);
	return "Shader plugin";
}
