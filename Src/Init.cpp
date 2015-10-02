#include <windows.h>
#include "avisynth.h"
#include "ConvertToFloat.h"
#include "ConvertFromFloat.h"
#include "Shader.h"

AVSValue __cdecl Create_Shader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	return new Shader(
		args[0].AsClip(),			// source clip
		args[1].AsString(""),		// shader path
		args[2].AsString("main"),	// entry point
		args[3].AsString(""),		// shader model
		args[4].AsString(""),		// param 1
		args[5].AsString(""),		// param 2
		args[6].AsString(""),		// param 3
		args[7].AsString(""),		// param 4
		args[8].AsString(""),		// param 5
		args[9].AsString(""),		// param 6
		args[10].AsString(""),		// param 7
		args[11].AsString(""),		// param 8
		args[12].AsString(""),		// param 9
		args[13].AsClip(),			// clip 1
		args[14].AsClip(),			// clip 2
		args[15].AsClip(),			// clip 3
		args[16].AsClip(),			// clip 4
		env);						// env is the link to essential informations, always provide it
}

AVSValue __cdecl Create_ConvertToFloat(AVSValue args, void* user_data, IScriptEnvironment* env) {
	PClip input = args[0].AsClip();
	if (input->GetVideoInfo().IsYV12())
		input = env->Invoke("ConvertToYV24", input).AsClip();

	return new ConvertToFloat(
		input,			// source clip
		args[1].AsBool(true),		// whether to convert YUV to RGB on the CPU
		env);						// env is the link to essential informations, always provide it
}

AVSValue __cdecl Create_ConvertFromFloat(AVSValue args, void* user_data, IScriptEnvironment* env) {
	const char* Format = args[1].AsString("YV12");
	bool ConvertYuv = args[2].AsBool(true);
	// Don't convert RGB to YUV when destination format is RGB32.
	if (strcmp(Format, "RGB32") == 0)
		ConvertYuv = false;

	ConvertFromFloat* Result = new ConvertFromFloat(
		args[0].AsClip(),			// source clip
		Format,						// destination format
		ConvertYuv,					// whether to convert RGB to YUV on the CPU
		env);						// env is the link to essential informations, always provide it

	if (strcmp(args[1].AsString("YV12"), "YV12") == 0)
		return env->Invoke("ConvertToYV12", Result).AsClip();
	else
		return Result;
}

const AVS_Linkage *AVS_linkage = 0;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors) {
	AVS_linkage = vectors;
	env->AddFunction("Shader", "c[path]s[entryPoint]s[shaderModel]s[param1]s[param2]s[param3]s[param4]s[param5]s[param6]s[param7]s[param8]s[param9]s[clip1]c[clip2]c[clip3]c[clip4]c", Create_Shader, 0);
	env->AddFunction("ConvertToFloat", "c[convertYuv]b", Create_ConvertToFloat, 0);
	env->AddFunction("ConvertFromFloat", "c[format]s[convertYuv]b", Create_ConvertFromFloat, 0);
	return "Shader plugin";
}
