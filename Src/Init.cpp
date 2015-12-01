#include <windows.h>
#include "avisynth.h"
#include "ConvertToFloat.h"
#include "ConvertFromFloat.h"
#include "Shader.h"
#include "ExecuteShader.h"

const int DefaultPrecision = 2;
const int DefaultConvertYuv = false;

AVSValue __cdecl Create_ConvertToFloat(AVSValue args, void* user_data, IScriptEnvironment* env) {
	PClip input = args[0].AsClip();
	bool ConvertYuv = args[1].AsBool(DefaultConvertYuv);
	if (input->GetVideoInfo().IsYV12())
		input = env->Invoke("ConvertToYV24", input).AsClip();
	// Don't convert YUV to RGB when source format is RGB32.
	if (input->GetVideoInfo().IsRGB32())
		ConvertYuv = false;

	return new ConvertToFloat(
		input,			// source clip
		ConvertYuv,		// whether to convert YUV to RGB on the CPU
		args[2].AsInt(DefaultPrecision), // precision, 1 for RGB32, 2 for UINT16 and 3 for half-float data.
		env);			// env is the link to essential informations, always provide it
}

AVSValue __cdecl Create_ConvertFromFloat(AVSValue args, void* user_data, IScriptEnvironment* env) {
	const char* Format = args[1].AsString("YV12");
	bool ConvertYuv = args[2].AsBool(DefaultConvertYuv);
	// Don't convert RGB to YUV when destination format is RGB32.
	if (strcmp(Format, "RGB32") == 0)
		ConvertYuv = false;

	ConvertFromFloat* Result = new ConvertFromFloat(
		args[0].AsClip(),			// source clip
		Format,						// destination format
		ConvertYuv,					// whether to convert RGB to YUV on the CPU
		args[3].AsInt(DefaultPrecision), // precision, 1 for RGB32, 2 for UINT16 and 3 for half-float data.
		env);						// env is the link to essential informations, always provide it

	if (strcmp(args[1].AsString("YV12"), "YV12") == 0)
		return env->Invoke("ConvertToYV12", Result).AsClip();
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
	int Precision = args[10].AsInt(DefaultPrecision); // precision

	return new ExecuteShader(
		args[0].AsClip(),			// source clip containing commands
		args[1].AsClip(),			// clip 1
		args[2].AsClip(),			// clip 2
		args[3].AsClip(),			// clip 3
		args[4].AsClip(),			// clip 4
		args[5].AsClip(),			// clip 5
		args[6].AsClip(),			// clip 6
		args[7].AsClip(),			// clip 7
		args[8].AsClip(),			// clip 8
		args[9].AsClip(),			// clip 9
		Precision,					// precision
		args[11].AsInt(Precision),	// precisionIn
		args[12].AsInt(Precision),	// precisionOut
		env);
}

const AVS_Linkage *AVS_linkage = 0;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors) {
	AVS_linkage = vectors;
	env->AddFunction("ConvertToFloat", "c[convertYuv]b[precision]i", Create_ConvertToFloat, 0);
	env->AddFunction("ConvertFromFloat", "c[format]s[convertYuv]b[precision]i", Create_ConvertFromFloat, 0);
	env->AddFunction("Shader", "c[path]s[entryPoint]s[shaderModel]s[param0]s[param1]s[param2]s[param3]s[param4]s[param5]s[param6]s[param7]s[param8]s[clip1]i[clip2]i[clip3]i[clip4]i[clip5]i[clip6]i[clip7]i[clip8]i[clip9]i[output]i[width]i[height]i", Create_Shader, 0);
	env->AddFunction("ExecuteShader", "c[clip1]c[clip2]c[clip3]c[clip4]c[clip5]c[clip6]c[clip7]c[clip8]c[clip9]c[precision]i[precisionIn]i[precisionOut]i", Create_ExecuteShader, 0);
	return "Shader plugin";
}
