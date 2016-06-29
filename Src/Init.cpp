#include <algorithm>
#include "ConvertShader.h"
#include "Shader.h"
#include "ExecuteShader.h"

const int DefaultConvertYuv = false;

AVSValue __cdecl Create_ConvertToShader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	PClip input = args[0].AsClip();
	const VideoInfo& vi = input->GetVideoInfo();
	if (!vi.IsY8() && !vi.IsYV12() && !vi.IsYV24() && !vi.IsRGB24() && !vi.IsRGB32())
		env->ThrowError("ConvertToShader: Source must be Y8, YV12, YV24, RGB24 or RGB32");

	int precision = args[1].AsInt(2);
	if (precision < 0 || precision > 3)
		env->ThrowError("ConvertToShader: Precision must be 0, 1, 2 or 3");

	bool stack16 = args[2].AsBool(false);
	if (stack16 && precision == 1)
		env->ThrowError("ConvertToShader: When using lsb, don't set precision=1!");
	if (stack16 && vi.IsRGB())
		env->ThrowError("ConvertToShader: Conversion from Stack16 only supports YV12 and YV24");

	if (precision == 0) {
		if (!vi.IsY8())
			input = env->Invoke("ConvertToY8", input).AsClip();
		return input;
	}

	if (vi.IsY8() || vi.IsYV12()) {
		if (stack16) {
			if (!env->FunctionExists("Dither_resize16nr"))
				env->ThrowError("ConvertToShader: Dither_resize16nr is missing.");
			AVSValue sargs[5] = { input, vi.width, vi.height / 2, "Spline36", "YV24" };
			const char *nargs[5] = { 0, 0, 0, "kernel", "csp" };
			input = env->Invoke("Dither_resize16nr", AVSValue(sargs, 5), nargs).AsClip();
		} else
			input = env->Invoke("ConvertToYV24", input).AsClip();
	}

	bool planar = args[3].AsBool(false);
	if (precision == 1 && planar && vi.IsYUV()) {
		return input;
	}

	return new ConvertShader(
		input,					// source clip
		precision,				// precision, 1 for RGB32, 2 for UINT16 and 3 for half-float data.
		stack16,				// lsb / Stack16
		std::string(""),
		planar,					// Planar
		args[4].AsInt(-1),		// 0 for C++ only, 1 for use SSE2, 2 for use SSSE3 and others for use F16C.
		env);					// env is the link to essential informations, always provide it

}

AVSValue __cdecl Create_ConvertFromShader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	PClip input = args[0].AsClip();
	const VideoInfo& vi = input->GetVideoInfo();

	if (!vi.IsRGB32() && !vi.IsYV24() && !vi.IsY8())
		env->ThrowError("ConvertFromShader: Source must be RGB32, Planar YV24 or Y8");

	int precision = args[1].AsInt(2);
	if (precision < 0 || precision > 3)
		env->ThrowError("ConvertFromShader: Precision must be 0, 1, 2 or 3");

	auto format = std::string(args[2].AsString("YV24"));
	std::transform(format.begin(), format.end(), format.begin(), toupper); // convert lower to UPPER
	if (precision > 0 && format != "YV12" && format != "YV24" && format != "RGB24" && format != "RGB32")
		env->ThrowError("ConvertFromShader: Destination format must be YV12, YV24, RGB24 or RGB32");

	bool stack16 = args[3].AsBool(false);
	if (stack16 && precision == 1)
		env->ThrowError("ConvertFromShader: When using lsb, don't set precision=1!");
	if (stack16 && (format == "YV12" || format == "Y8") && !env->FunctionExists("Dither_resize16nr")) {
		env->ThrowError("ConvertFromShader: Dither_resize16nr is missing.");
	}

	if ((precision == 0 || precision == 1) && (vi.IsY8() || vi.IsYV24()) && (format == "Y8" || format == "YV12" || format == "YV24")) {
		if (format == "YV12")
			input = env->Invoke("ConvertToYV12", input).AsClip();
		if (format == "Y8" && !vi.IsY8())
			input = env->Invoke("ConvertToY8", input).AsClip();
		if (format == "YV24" && !vi.IsYV24())
			input = env->Invoke("ConvertToYV24", input).AsClip();
		return input;
	}

	bool rgb_dst = (format == "RGB24" || format == "RGB32");
	if (stack16 && rgb_dst)
		env->ThrowError("ConvertFromShader: Conversion to Stack16 only supports YV12 and YV24");

	ConvertShader* Result = new ConvertShader(
		input,				// source clip
		precision,			// precision, 1 for RGB32, 2 for UINT16 and 3 for half-float data.
		stack16,			// lsb / Stack16
		format,				// destination format
		false,
		args[4].AsInt(-1),	// 0 for C++ only, 1 for use SSE2, 2 for use SSSE3 and others for use F16C.
		env);				// env is the link to essential informations, always provide it

	if (format == "YV12" || format == "Y8") {
		if (stack16) {
			AVSValue sargs[6] = { Result, Result->GetVideoInfo().width, Result->GetVideoInfo().height / 2, "Spline36", format.c_str(), true };
			const char *nargs[6] = { 0, 0, 0, "kernel", "csp", "invks" };
			return env->Invoke("Dither_resize16nr", AVSValue(sargs, 6), nargs);
		}
		else if (format == "YV12")
			return env->Invoke("ConvertToYV12", Result);
		else if (format == "Y8")
			return env->Invoke("ConvertToYV8", Result);
	}

	return Result;
}

AVSValue __cdecl Create_Shader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	return new Shader(
		args[0].AsClip(),			// source clip
		args[1].AsString(""),		// shader path
		args[2].AsString("main"),	// entry point
		args[3].AsString("ps_3_0"),	// shader model
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
		args[25].AsInt(-1),			// precision
		args[26].AsString(""),		// defines
		env);						// env is the link to essential informations, always provide it
}

AVSValue __cdecl Create_ExecuteShader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	int ParamClipPrecision[9];
	int CurrentPrecision = 1;
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
		ParamClipPrecision,			// ClipPrecision, args[10-18]
		args[19].AsInt(3),			// Precision
		args[20].AsInt(1),			// PrecisionOut
		args[21].AsBool(false),		// PlanarOut
		env);
}

const AVS_Linkage *AVS_linkage = 0;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors) {
	AVS_linkage = vectors;
	env->AddFunction("ConvertToShader", "c[Precision]i[lsb]b[planar]b[opt]i", Create_ConvertToShader, 0);
	env->AddFunction("ConvertFromShader", "c[Precision]i[Format]s[lsb]b[opt]i", Create_ConvertFromShader, 0);
	env->AddFunction("Shader", "c[Path]s[EntryPoint]s[ShaderModel]s[Param0]s[Param1]s[Param2]s[Param3]s[Param4]s[Param5]s[Param6]s[Param7]s[Param8]s[Clip1]i[Clip2]i[Clip3]i[Clip4]i[Clip5]i[Clip6]i[Clip7]i[Clip8]i[Clip9]i[Output]i[Width]i[Height]i[Precision]i[Defines]s", Create_Shader, 0);
	env->AddFunction("ExecuteShader", "c[Clip1]c[Clip2]c[Clip3]c[Clip4]c[Clip5]c[Clip6]c[Clip7]c[Clip8]c[Clip9]c[Clip1Precision]i[Clip2Precision]i[Clip3Precision]i[Clip4Precision]i[Clip5Precision]i[Clip6Precision]i[Clip7Precision]i[Clip8Precision]i[Clip9Precision]i[Precision]i[OutputPrecision]i[PlanarOut]b", Create_ExecuteShader, 0);

	if (env->FunctionExists("SetFilterMTMode")) {
		auto env2 = static_cast<IScriptEnvironment2*>(env);
		env2->SetFilterMTMode("ConvertToShader", MT_NICE_FILTER, true);
		env2->SetFilterMTMode("ConvertFromShader", MT_NICE_FILTER, true);
		env2->SetFilterMTMode("Shader", MT_NICE_FILTER, true);
		env2->SetFilterMTMode("ExecuteShader", SUPPORT_MT_NICE_FILTER ? MT_NICE_FILTER : MT_MULTI_INSTANCE, true);
	}

	return "Shader plugin";
}
