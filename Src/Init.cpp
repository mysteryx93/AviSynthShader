#include <algorithm>
#include "ConvertShader.h"
#include "Shader.h"
#include "ExecuteShader.h"
#include "PixelFormatParser.h"
#include "ConvertStacked.hpp"

const int DefaultConvertYuv = false;
static PixelFormatParser pixelFormatParser;

AVSValue __cdecl Create_ConvertToShader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	PClip input = args[0].AsClip();
	bool planar = args[3].AsBool(false);

	const VideoInfo& vi = input->GetVideoInfo();
	if (!vi.IsY() && !vi.Is420() && !vi.Is422() && !vi.Is444() && !vi.IsRGB())
		env->ThrowError("ConvertToShader: Source format is not supported.");

	bool stack16 = args[2].AsBool(false);
	bool HBD = vi.BitsPerComponent() > 8 || stack16; // High-bit-depth source
	int precision = args[1].AsInt(2);
	if (precision < 0 || precision > 3)
		env->ThrowError("ConvertToShader: Precision must be 0, 1, 2 or 3");
	if (stack16 && vi.BitsPerComponent() > 8)
		env->ThrowError("ConvertToShader: Can't use both lsb and high-bit-depth sources!");
	if (stack16 && precision == 1)
		env->ThrowError("ConvertToShader: When using lsb, don't set precision=1!");
	if (stack16 && vi.IsRGB())
		env->ThrowError("ConvertToShader: Conversion from Stack16 only supports YV12 and YV24");
	if (HBD && precision < 2)
		env->ThrowError("ConvertToShader: Precision must be 2 when using high-bit-depth video");

	if (precision == 0) {
		if (!vi.IsY8())
			input = env->Invoke("ConvertToY8", input).AsClip();
		return input;
	}

	int Opt = args[4].AsInt(-1);
	bool isPlusMt = env->FunctionExists("SetFilterMTMode");
	if (!isPlusMt || Opt > -1) {
		// This code is designed for Avisynth 2.6
		if (precision == 1 && planar && vi.IsYUV()) {
			if (!vi.IsYV24()) {
				AVSValue sargs[2] = { input, "Spline36" };
				const char *nargs[2] = { 0, "chromaresample" };
				input = env->Invoke("ConvertToYV24", AVSValue(sargs, 2), nargs).AsClip();
			}
		}
		else {
			if (vi.IsY8() || vi.IsYV12()) {
				if (stack16) {
					if (!env->FunctionExists("Dither_resize16nr"))
						env->ThrowError("ConvertToShader: Dither_resize16nr is missing.");
					AVSValue sargs[5] = { input, vi.width, vi.height / 2, "Spline36", "YV24" };
					const char *nargs[5] = { 0, 0, 0, "kernel", "csp" };
					input = env->Invoke("Dither_resize16nr", AVSValue(sargs, 5), nargs).AsClip();
				}
				else {
					AVSValue sargs[2] = { input, "Spline36" };
					const char *nargs[2] = { 0, "chromaresample" };
					input = env->Invoke("ConvertToYV24", AVSValue(sargs, 2), nargs).AsClip();
				}
			}

			input = new ConvertShader(
				input,					// source clip
				precision,				// precision, 1 for RGB32, 2 for UINT16 and 3 for half-float data.
				stack16,				// lsb / Stack16
				std::string(""),
				planar,					// Planar
				Opt,					// 0 for C++ only, 1 for use SSE2, 2 for use SSSE3 and others for use F16C. -1 to use Avisynth+ functions.
				env);					// env is the link to essential informations, always provide it
		}
	}
	else {
		if (stack16)
			input = env->Invoke("ConvertFromStacked", input).AsClip();
		else if (precision > 1 && vi.BitsPerComponent() < 16) {
			AVSValue sargs[2] = { input, 16 };
			const char *nargs[2] = { 0, 0 };
			input = env->Invoke("ConvertBits", AVSValue(sargs, 2), nargs).AsClip();
		}

		// With Avisynth+, we'll rely purely on a chain of built-in functions.
		if (vi.IsYUV() || vi.IsYUVA()) {
			if (vi.Is420() || vi.Is422()) {
				AVSValue sargs[2] = { input, "Spline36" };
				const char *nargs[2] = { 0, "chromaresample" };
				input = env->Invoke("ConvertToYUV444", AVSValue(sargs, 2), nargs).AsClip();
			}
			if (!planar) {
				if (vi.NumComponents() == 3)
					input = env->Invoke("AddAlphaPlane", input).AsClip();
				// Cast YUVA as RGBA.
				std::string shuffleFormat = std::string("RGBAP") + (precision == 1 ? "8" : precision == 4 ? "S" : "16");
				AVSValue sargs[4] = { input, precision == 1 ? "RGBA" : "BGRA", "YUVA", shuffleFormat.c_str() };
				const char *nargs[4] = { 0, "planes", "source_planes", "pixel_type" };
				input = env->Invoke("CombinePlanes", AVSValue(sargs, 4), nargs).AsClip();
				// CombinePlanes causes FlipVertical.
				input = env->Invoke(precision > 1 ? "ConvertToRGB64" : "ConvertToRGB32", input).AsClip();
				input = env->Invoke("FlipVertical", input).AsClip();
			}
		}
		else {
			if (planar) {
				if (vi.NumComponents() == 4)
					input = env->Invoke("RemoveAlphaPlane", input).AsClip();
				// Cast YUVA as RGBA.
				if (!vi.IsPlanarRGB())
					input = env->Invoke("ConvertToPlanarRGB", input).AsClip();
				std::string shuffleFormat = std::string("YUV444P") + (precision == 1 ? "8" : precision == 4 ? "S" : "16");
				AVSValue sargs[4] = { input, "YUV", precision == 1 ? "RGB" : "BGR", shuffleFormat.c_str() };
				const char *nargs[4] = { 0, "planes", "source_planes", "pixel_type" };
				input = env->Invoke("CombinePlanes", AVSValue(sargs, 4), nargs).AsClip();
				// CombinePlanes causes FlipVertical.
			} else if (vi.IsPlanarRGB())
				input = env->Invoke(HBD ? "ConvertToRGB64" : "ConvertToRGB32", input).AsClip();
			else if (vi.NumComponents() == 3)
				input = env->Invoke("AddAlphaPlane", input).AsClip();
			if (!planar)
				input = env->Invoke("FlipVertical", input).AsClip();
		}
		if (precision > 1)
			input = env->Invoke("Shader_ConvertToDoubleWidth", input).AsClip();
	}
	return input;
}

AVSValue __cdecl Create_ConvertFromShader(AVSValue args, void* user_data, IScriptEnvironment* env) {
	PClip input = args[0].AsClip();
	const VideoInfo& viSrc = input->GetVideoInfo();

	if (!viSrc.IsRGB32() && !viSrc.IsYV24() && !viSrc.IsY8())
		env->ThrowError("ConvertFromShader: Source must be RGB32, Planar YV24 or Y8");

	int precision = args[1].AsInt(2);
	if (precision < 0 || precision > 3)
		env->ThrowError("ConvertFromShader: Precision must be 0, 1, 2 or 3");

	auto format = std::string(args[2].AsString("YV24"));
	std::transform(format.begin(), format.end(), format.begin(), toupper); // convert lower to UPPER
	VideoInfo viDst = pixelFormatParser.GetVideoInfo(std::string(format));
	bool HBD = viDst.BitsPerComponent() > 8;
	//if (viDst.pixel_type == 0 || (precision > 0 && format != "YV12" && format != "YV24" && format != "RGB24" && format != "RGB32" && !HBD))
	//	env->ThrowError("ConvertFromShader: Destination format is not supported");

	bool stack16 = args[3].AsBool(false);
	if (stack16 && precision == 1)
		env->ThrowError("ConvertFromShader: When using lsb, don't set precision=1!");
	if (stack16 && HBD)
		env->ThrowError("ConvertFromShader: Can't use both lsb and high-bit-depth sources!");
	if (stack16 && (viDst.IsYV12() || viDst.IsY8()) && !env->FunctionExists("Dither_resize16nr")) 
		env->ThrowError("ConvertFromShader: Dither_resize16nr is missing.");
	if (stack16 && !viDst.IsYV12() && !viDst.IsYV24())
		env->ThrowError("ConvertFromShader: Conversion to Stack16 only supports YV12 and YV24");
	if (HBD && precision < 2)
		env->ThrowError("ConvertFromShader: Precision must be 2 when using high-bit-depth");

	int Opt = args[4].AsInt(-1);
	bool isPlusMt = env->FunctionExists("SetFilterMTMode");
	if (!isPlusMt || Opt > -1) {
		// This code is designed for Avisynth 2.6
		if ((precision == 0 || precision == 1) && (viSrc.IsY() || viSrc.IsYV24()) && (viDst.IsY8() || viDst.IsYV12() || viDst.IsYV16() || viDst.IsYV24())) {
			if (viDst.IsY8() && !viSrc.IsY())
				input = env->Invoke("ConvertToY8", input).AsClip();
			if (viDst.IsYV12())
				input = env->Invoke("ConvertToYV12", input).AsClip();
			if (viDst.IsYV16())
				input = env->Invoke("ConvertToYV24", input).AsClip();
			if (viDst.IsYV24() && !viSrc.IsYV24())
				input = env->Invoke("ConvertToYV24", input).AsClip();
		}
		else {
			input = new ConvertShader(
				input,				// source clip
				precision,			// precision, 1 for RGB32, 2 for UINT16 and 3 for half-float data.
				stack16,			// lsb / Stack16
				format,				// destination format
				false,
				Opt,				// 0 for C++ only, 1 for use SSE2, 2 for use SSSE3 and others for use F16C. -1 to use Avisynth+ functions.
				env);				// env is the link to essential informations, always provide it

			if (viDst.IsY8() || viDst.IsYV12() || viDst.IsYV16()) {
				if (stack16) {
					AVSValue sargs[6] = { input, input->GetVideoInfo().width, input->GetVideoInfo().height / 2, "Spline36", format.c_str(), true };
					const char *nargs[6] = { 0, 0, 0, "kernel", "csp", "invks" };
					input = env->Invoke("Dither_resize16nr", AVSValue(sargs, 6), nargs).AsClip();
				}
				else if (viDst.IsYV12())
					input = env->Invoke("ConvertToYV12", input).AsClip();
				else if (viDst.IsY8())
					input = env->Invoke("ConvertToY8", input).AsClip();
			}
		}
	}
	else {
		// With Avisynth+, we'll rely purely on a chain of built-in functions.
		if (precision > 1)
			input = env->Invoke("Shader_ConvertFromDoubleWidth", input).AsClip();

		if (viSrc.IsYUV()) {
			// Planar output.
			if (viDst.IsRGB()) {
				// Cast YUV as RGB.
				std::string shuffleFormat = std::string("RGBP") + (precision == 1 ? "8" : precision == 4 ? "S" : "16");
				AVSValue sargs[4] = { input, precision == 1 ? "RGB" : "BGR", "YUV", shuffleFormat.c_str() };
				const char *nargs[4] = { 0, "planes", "source_planes", "pixel_type" };
				input = env->Invoke("CombinePlanes", AVSValue(sargs, 4), nargs).AsClip();
				// CombinePlanes causes FlipVertical.
				if (!viDst.IsPlanarRGB() && !viDst.IsPlanarRGBA())
					input = env->Invoke(viDst.IsRGB24() ? "ConvertToRGB24" : viDst.IsRGB32() ? "ConvertToRGB32" : viDst.IsRGB48() ? "ConvertToRGB48" : viDst.IsRGB64() ? "ConvertToRGB64" : "", input).AsClip();
			}
		}
		else {
			// Stacked output
			if (!viDst.IsRGB()) {
				// Cast RGB as YUV.
				if (!viSrc.IsPlanarRGB())
					input = env->Invoke("ConvertToPlanarRGBA", input).AsClip();
				std::string shuffleFormat = std::string("YUVA444P") + (precision == 1 ? "8" : precision == 4 ? "S" : "16");
				AVSValue sargs[4] = { input, "YUVA", precision == 1 ? "RGBA" : "BGRA", shuffleFormat.c_str() };
				const char *nargs[4] = { 0, "planes", "source_planes", "pixel_type" };
				input = env->Invoke("CombinePlanes", AVSValue(sargs, 4), nargs).AsClip();
				// CombinePlanes causes FlipVertical.
			}
			input = env->Invoke("FlipVertical", input).AsClip();
		}

		if (viDst.NumComponents() == 3)
			input = env->Invoke("RemoveAlphaPlane", input).AsClip();
		if (viDst.Is420() || viDst.Is422()) {
			AVSValue sargs[2] = { input, "Spline36" };
			const char *nargs[2] = { 0, "chromaresample" };
			input = env->Invoke(viDst.Is422() ? "ConvertToYUV422" : "ConvertToYUV420", AVSValue(sargs, 2), nargs).AsClip();
		}

		if (precision > 1 && viDst.BitsPerComponent() < 16 && !stack16) {
			if (viDst.BitsPerComponent() == 8) {
				// Dither
				AVSValue sargs[3] = { input, viDst.BitsPerComponent(), -1 };
				const char *nargs[3] = { 0, 0, "dither" };
				input = env->Invoke("ConvertBits", AVSValue(sargs, 3), nargs).AsClip();
			}
			else {
				// 10-14bit: don't dither. "dither=-1" can't be used due to a bug.
				AVSValue sargs[2] = { input, viDst.BitsPerComponent() };
				const char *nargs[2] = { 0, 0 };
				input = env->Invoke("ConvertBits", AVSValue(sargs, 2), nargs).AsClip();
			}
		}

		if (stack16)
			input = env->Invoke("ConvertToStacked", input).AsClip();
	}
	return input;
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
		args[22].AsInt(1),			// Engines count
		args[23].AsBool(false),		// Resource (don't search for file)
		env);
}

AVSValue __cdecl Create_GetBitDepth(AVSValue args, void* user_data, IScriptEnvironment* env) {
	VideoInfo vi;
	AVSValue Format = args[1].AsString("");
	if (Format.AsString() == "")
		vi = args[0].AsClip()->GetVideoInfo();
	else {
		int pixelFormat = pixelFormatParser.GetPixelFormatAsInt(std::string(Format.AsString()));
		if (pixelFormat != 0)
			vi = pixelFormatParser.GetVideoInfo(pixelFormat);
		else
			env->ThrowError("Shader_GetBitDepth: Specified format is invalid");
	}
	return AVSValue(vi.BitsPerComponent());
}

const AVS_Linkage *AVS_linkage = 0;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors) {
	AVS_linkage = vectors;
	env->AddFunction("ConvertToShader", "c[Precision]i[lsb]b[planar]b[opt]i", Create_ConvertToShader, 0);
	env->AddFunction("ConvertFromShader", "c[Precision]i[Format]s[lsb]b[opt]i", Create_ConvertFromShader, 0);
	env->AddFunction("Shader", "c[Path]s[EntryPoint]s[ShaderModel]s[Param0]s[Param1]s[Param2]s[Param3]s[Param4]s[Param5]s[Param6]s[Param7]s[Param8]s[Clip1]i[Clip2]i[Clip3]i[Clip4]i[Clip5]i[Clip6]i[Clip7]i[Clip8]i[Clip9]i[Output]i[Width]i[Height]i[Precision]i[Defines]s", Create_Shader, 0);
	env->AddFunction("ExecuteShader", "c[Clip1]c[Clip2]c[Clip3]c[Clip4]c[Clip5]c[Clip6]c[Clip7]c[Clip8]c[Clip9]c[Clip1Precision]i[Clip2Precision]i[Clip3Precision]i[Clip4Precision]i[Clip5Precision]i[Clip6Precision]i[Clip7Precision]i[Clip8Precision]i[Clip9Precision]i[Precision]i[OutputPrecision]i[PlanarOut]b[Engines]i[Resource]b", Create_ExecuteShader, 0);
	env->AddFunction("Shader_GetBitDepth", "c[format]s", Create_GetBitDepth, 0);

	env->AddFunction("Shader_ConvertFromStacked", "c[bits]i", ConvertFromStacked::Create, 0);
	env->AddFunction("Shader_ConvertToStacked", "c", ConvertToStacked::Create, 0);
	env->AddFunction("Shader_ConvertFromDoubleWidth", "c[bits]i", ConvertFromDoubleWidth::Create, 0);
	env->AddFunction("Shader_ConvertToDoubleWidth", "c", ConvertToDoubleWidth::Create, 0);
	return "Shader plugin";
}
