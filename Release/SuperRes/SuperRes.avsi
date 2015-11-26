function SuperRes(clip input, int "passes", float "strength", float "softness", string "upscalecommand", bool "srcMatrix601", bool "convert")
{
	passes = default(passes, 1)
	strength = default(strength, 1)
	softness = default(softness, 0)
	convert = default(convert, true)
	srcMatrix601 = default(srcMatrix601, false)

	Assert((passes > 0 && passes <= 3) ? true : false, chr(10) + "Passes must be between 1 and 3" + chr(10))
	Assert((strength >= 0 && strength <= 1) ? true : false, chr(10) + "Strength must be between 0 and 1" + chr(10))
	Assert((softness >= 0 && softness <= 1) ? true : false, chr(10) + "Softness must be between 0 and 1" + chr(10))
	Assert(Defined(upscalecommand), chr(10) + "You must specify upscalecommand" + chr(10))

	input

	convertYuv = convert && !IsRGB()
	sourceFormat = IsYV12 ? "YV12" : IsYV24 ? "YV24" : IsRGB24 ? "RGB24" : IsRGB32 ? "RGB32" : ""
	Assert(sourceFormat != "", chr(10) + "Source must be YV12, YV24, RGB24 or RGB32" + chr(10))

	original = convert ? ConvertToFloat(precision=1) : last
	Eval(upscalecommand)
	input = convert ? ConvertToFloat(precision=1) : last
	input.Shader(convertYuv && srcMatrix601 ? "Yuv601ToLinear.cso" : convertYuv ? "YuvToLinear.cso" : "GammaToLinear.cso")
	SuperResPass(last, input, original, strength, softness, 1, passes, convertYuv, srcMatrix601)
	passes > 1 ? SuperResPass(input, original, strength, softness, 2, passes, convertYuv, srcMatrix601) : nop
	passes > 2 ? SuperResPass(input, original, strength, softness, 3, passes, convertYuv, srcMatrix601) : nop

	ExecuteShader(last, input, original, precision=2, precisionIn=convert?1:2, precisionOut=convert?1:2)
	convert ? ConvertFromFloat(format=sourceFormat, precision=1) : last
}

function SuperResPass(clip cmd, clip input, clip original, float strength, float softness, int pass, int passes, bool convertYuv, bool srcMatrix601)
{
	cmd.Shader("Bicubic.cso", output=3, \
		param0=string(original.Width / 2) + "," + string(original.Height) + "," + string(1./(original.Width / 2),"%.32f") + "," + string(1./original.Height,"%.32f") + "f",\
		param1=string(input.Width / 2) + "," + string(input.Height) + "," + string(1./(input.Width / 2),"%.32f") + "," + string(1./input.Height,"%.32f") + "f",\
		param2=string(1/3.,"%.32f") + "," + string(1/3.,"%.32f") + "f",\
		width=original.Width / 2, height=original.Height)

	Shader(convertYuv && srcMatrix601 ? "SuperResDiff601.cso" : convertYuv ? "SuperResDiff709.cso" : "SuperResDiff.cso", clip1=3, clip2=2, output=4)

	Shader(pass==passes ? (convertYuv ? "SuperResFinal709.cso" : "SuperResFinal.cso") : "SuperRes.cso", clip1=1, clip2=4, output=1, \
		param2=string(original.Width/2) + "," + string(original.Height) + "," + string(1./(original.Width/2),"%.32f") + "," + string(1./original.Height,"%.32f") + "f", \
		Param3=string(strength,"%.32f") + "," + string(softness,"%.32f") + "," + string(pass) + "," + string(passes) + "f")
}