#include <windows.h>
#include "avisynth.h"
#include "ConvertToShader.h"
#include "ConvertFromShader.h"
#include "Shader.h"
#include "ExecuteShader.h"

const int DefaultConvertYuv = false;

AVSValue __cdecl Create_ConvertToShader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	PClip input = args[0].AsClip();
	if (input->GetVideoInfo().IsYV12()) {
		if (args[2].AsBool(false)) // Stack16
			input = env->Invoke("Eval", AVSValue("Dither_resize16(Width(),Height()/2, csp=\"YV24\")")).AsClip();
		else
			input = env->Invoke("ConvertToYV24", input).AsClip();
	}

	return new ConvertToShader(
		input,					// source clip
		args[1].AsInt(2),		// precision, 1 for RGB32, 2 for UINT16 and 3 for half-float data.
		args[2].AsBool(false),	// lsb / Stack16
		env);					// env is the link to essential informations, always provide it
}

AVSValue __cdecl Create_ConvertFromShader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	ConvertFromShader* Result = new ConvertFromShader(
		args[0].AsClip(),			// source clip
		args[1].AsInt(2),			// precision, 1 for RGB32, 2 for UINT16 and 3 for half-float data.
		args[2].AsString("YV12"),	// destination format
		args[3].AsBool(false),		// lsb / Stack16
		env);						// env is the link to essential informations, always provide it

	if (strcmp(args[2].AsString("YV12"), "YV12") == 0) {
		if (args[3].AsBool(false)) {// Stack16
			AVSValue sargs[4] = { Result, Result->GetVideoInfo().width, Result->GetVideoInfo().height / 2, "YV12" };
			const char *nargs[4] = { 0, 0, 0, "csp" };
			return env->Invoke("Dither_resize16", AVSValue(sargs, 4), nargs).AsClip();
		}
		else
			return env->Invoke("ConvertToYV12", Result).AsClip();
	}
	else
		return Result;
}

AVSValue __cdecl Create_Shader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	return new Shader(
		args[0].AsClip(),			// source clip
		args[1].AsString(""),		// shader path
		args[2].AsString("main"),	// entry point
		args[3].AsString(""),		// shader model
		args[4].AsString(""),		// param 0
		args[5].AsString(""),		// param 1
		args[6].AsString(""),		// param 2
		args[7].AsString(""),		// param 3
		args[8].AsString(""),		// param 4
		args[9].AsString(""),		// param 5
		args[10].AsString(""),		// param 6
		args[11].AsString(""),		// param 7
		args[12].AsString(""),		// param 8
		args[13].AsInt(1),			// clip 1
		args[14].AsInt(0),			// clip 2
		args[15].AsInt(0),			// clip 3
		args[16].AsInt(0),			// clip 4
		args[17].AsInt(0),			// clip 5
		args[18].AsInt(0),			// clip 6
		args[19].AsInt(0),			// clip 7
		args[20].AsInt(0),			// clip 8
		args[21].AsInt(0),			// clip 9
		args[22].AsInt(1),			// output clip
		args[23].AsInt(0),			// width
		args[24].AsInt(0),			// height
		env);						// env is the link to essential informations, always provide it
}

AVSValue __cdecl Create_ExecuteShader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	int ParamClipPrecision[9];
	int CurrentPrecision = 2;
	for (int i = 0; i < 9; i++) {
		CurrentPrecision = args[i + 10].AsInt(CurrentPrecision);
		ParamClipPrecision[i] = CurrentPrecision;
	}

	return new ExecuteShader(
		args[0].AsClip(),			// source clip containing commands
		args[1].AsClip(),			// Clip1
		args[2].AsClip(),			// Clip2
		args[3].AsClip(),			// Clip3
		args[4].AsClip(),			// Clip4
		args[5].AsClip(),			// Clip5
		args[6].AsClip(),			// Clip6
		args[7].AsClip(),			// Clip7
		args[8].AsClip(),			// Clip8
		args[9].AsClip(),			// Clip9
		ParamClipPrecision,			// ClipPrecision, 10-18
		args[19].AsInt(2),			// precision
		args[20].AsInt(2),			// precisionOut
		env);
}

const AVS_Linkage *AVS_linkage = 0;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors) {
	AVS_linkage = vectors;
	env->AddFunction("ConvertToShader", "c[Precision]i[lsb]b", Create_ConvertToShader, 0);
	env->AddFunction("ConvertFromShader", "c[Precision]i[Format]s[lsb]b", Create_ConvertFromShader, 0);
	env->AddFunction("Shader", "c[Path]s[EntryPoint]s[ShaderModel]s[Param0]s[Param1]s[Param2]s[Param3]s[Param4]s[Param5]s[Param6]s[Param7]s[Param8]s[Clip1]i[Clip2]i[Clip3]i[Clip4]i[Clip5]i[Clip6]i[Clip7]i[Clip8]i[Clip9]i[Output]i[Width]i[Height]i", Create_Shader, 0);
	env->AddFunction("ExecuteShader", "c[Clip1]c[Clip2]c[Clip3]c[Clip4]c[Clip5]c[Clip6]c[Clip7]c[Clip8]c[Clip9]c[Clip1Precision]i[Clip2Precision]i[Clip3Precision]i[Clip4Precision]i[Clip5Precision]i[Clip6Precision]i[Clip7Precision]i[Clip8Precision]i[Clip9Precision]i[Precision]i[OutputPrecision]i", Create_ExecuteShader, 0);
	return "Shader plugin";
}
