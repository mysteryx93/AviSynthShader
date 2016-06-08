#include "ConvertShader.h"
#include "Shader.h"
#include "ExecuteShader.h"

const int DefaultConvertYuv = false;

AVSValue __cdecl Create_ConvertToShader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	PClip input = args[0].AsClip();
	if (input->GetVideoInfo().IsYV12()) {
		if (args[2].AsBool(false)) { // Stack16
			AVSValue sargs[5] = { input, input->GetVideoInfo().width, input->GetVideoInfo().height / 2, "Spline36", "YV24" };
			const char *nargs[5] = { 0, 0, 0, "kernel", "csp" };
			input = env->Invoke("Dither_resize16nr", AVSValue(sargs, 5), nargs).AsClip();
		}
		else
			input = env->Invoke("ConvertToYV24", input).AsClip();
	}

	return new ConvertToShader(
		input,					// source clip
		args[1].AsInt(2),		// precision, 1 for RGB32, 2 for UINT16 and 3 for half-float data.
		args[2].AsBool(false),	// lsb / Stack16
        args[3].AsInt(-1),      // 0 for C++ only, 1 for use SSE2 and others for use F16C.
		env);					// env is the link to essential informations, always provide it
}

AVSValue __cdecl Create_ConvertFromShader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	ConvertFromShader* Result = new ConvertFromShader(
		args[0].AsClip(),			// source clip
		args[1].AsInt(2),			// precision, 1 for RGB32, 2 for UINT16 and 3 for half-float data.
		args[2].AsString("YV12"),	// destination format
		args[3].AsBool(false),		// lsb / Stack16
        args[4].AsInt(-1),          // 0 for C++ only, 1 for use SSE2 and others for use F16C.
		env);						// env is the link to essential informations, always provide it

	if (strcmp(args[2].AsString("YV12"), "YV12") == 0) {
		if (args[3].AsBool(false)) {// Stack16
			AVSValue sargs[6] = { Result, Result->GetVideoInfo().width, Result->GetVideoInfo().height / 2, "Spline36", "YV12", true };
			const char *nargs[6] = { 0, 0, 0, "kernel", "csp", "invks"};
			return env->Invoke("Dither_resize16nr", AVSValue(sargs, 6), nargs).AsClip();
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

	// P.F. IsClip() pre-check, because avisynth debug version give assert if AsClip() is not a clip
	// preventing happy debugging
	return new ExecuteShader(
		args[0].AsClip(),			// source clip containing commands
		args[1].IsClip() ? args[1].AsClip() : nullptr,			// Clip1
		args[2].IsClip() ? args[2].AsClip() : nullptr,			// Clip2
		args[3].IsClip() ? args[3].AsClip() : nullptr,			// Clip3
		args[4].IsClip() ? args[4].AsClip() : nullptr,			// Clip4
		args[5].IsClip() ? args[5].AsClip() : nullptr,			// Clip5
		args[6].IsClip() ? args[6].AsClip() : nullptr,			// Clip6
		args[7].IsClip() ? args[7].AsClip() : nullptr,			// Clip7
		args[8].IsClip() ? args[8].AsClip() : nullptr,			// Clip8
		args[9].IsClip() ? args[9].AsClip() : nullptr,			// Clip9
		ParamClipPrecision,			// ClipPrecision, 10-18
		args[19].AsInt(2),			// precision
		args[20].AsInt(2),			// precisionOut
		env);
}

const AVS_Linkage *AVS_linkage = 0;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors) {
	AVS_linkage = vectors;
	env->AddFunction("ConvertToShader", "c[Precision]i[lsb]b[opt]i", Create_ConvertToShader, 0);
	env->AddFunction("ConvertFromShader", "c[Precision]i[Format]s[lsb]b[opt]i", Create_ConvertFromShader, 0);
	env->AddFunction("Shader", "c[Path]s[EntryPoint]s[ShaderModel]s[Param0]s[Param1]s[Param2]s[Param3]s[Param4]s[Param5]s[Param6]s[Param7]s[Param8]s[Clip1]i[Clip2]i[Clip3]i[Clip4]i[Clip5]i[Clip6]i[Clip7]i[Clip8]i[Clip9]i[Output]i[Width]i[Height]i", Create_Shader, 0);
	env->AddFunction("ExecuteShader", "c[Clip1]c[Clip2]c[Clip3]c[Clip4]c[Clip5]c[Clip6]c[Clip7]c[Clip8]c[Clip9]c[Clip1Precision]i[Clip2Precision]i[Clip3Precision]i[Clip4Precision]i[Clip5Precision]i[Clip6Precision]i[Clip7Precision]i[Clip8Precision]i[Clip9Precision]i[Precision]i[OutputPrecision]i", Create_ExecuteShader, 0);

	if (env->FunctionExists("SetFilterMTMode")) {
		auto env2 = static_cast<IScriptEnvironment2*>(env);
		env2->SetFilterMTMode("ConvertToShader", MT_NICE_FILTER, true);
		env2->SetFilterMTMode("ConvertFromShader", MT_NICE_FILTER, true);
		env2->SetFilterMTMode("Shader", MT_NICE_FILTER, true);
		env2->SetFilterMTMode("ExecuteShader", MT_MULTI_INSTANCE, true);
	}

	return "Shader plugin";
}
