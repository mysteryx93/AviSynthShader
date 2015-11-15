function SuperRes(clip input, int "passes", float "strength", float "softness", string "upscalecommand", string "folder", bool "convert")
{
	passes = default(passes, 1)
	strength = default(strength, 1)
	softness = default(softness, 0)
	folder = default(folder, "")
	convert = default(convert, true)
	precision = 2

	Assert((passes > 0 && passes <= 3) ? true : false, chr(10) + "Passes must be between 1 and 3" + chr(10))
	Assert((strength >= 0 && strength <= 1) ? true : false, chr(10) + "Strength must be between 0 and 1" + chr(10))
	Assert((softness >= 0 && softness <= 1) ? true : false, chr(10) + "Softness must be between 0 and 1" + chr(10))
	Assert(Defined(upscalecommand), chr(10) + "You must specify upscalecommand" + chr(10))

	input

	convertYuv = convert && !IsRGB()
	sourceFormat = IsYV12() ? "YV12" : IsYV24() ? "YV24" : IsRGB24() ? "RGB24" : "RGB32"

	original = convert ? ConvertToFloat(precision=precision) : last
	Eval(upscalecommand)
	input = convert ? ConvertToFloat(precision=precision) : last
	input.Shader(folder + (convertYuv ? "YuvToLinear.cso" : "GammaToLinear.cso"))
	SuperResPass(last, input, original, strength, softness, 1, passes, convertYuv, folder)
	passes > 1 ? SuperResPass(input, original, strength, softness, 2, passes, convertYuv, folder) : nop
	passes > 2 ? SuperResPass(input, original, strength, softness, 3, passes, convertYuv, folder) : nop

	ExecuteShader(last, input, original, precision=precision)
	convert ? ConvertFromFloat(format=sourceFormat,precision=precision) : last
}

function SuperResPass(clip cmd, clip input, clip original, float strength, float softness, int pass, int passes, bool convertYuv, string folder)
{
	cmd.Shader(folder+"Bicubic.cso", output=3, \
		param0=string(original.Width / 2) + "," + string(original.Height) + "," + string(1./(original.Width / 2),"%f") + "," + string(1./original.Height,"%f") + "f",\
		param1=string(input.Width / 2) + "," + string(input.Height) + "," + string(1./(input.Width / 2),"%.32f") + "," + string(1./input.Height,"%.32f") + "f",\
		param2=string(1/3.,"%.32f") + "," + string(1/3.,"%.32f") + "f",\
		width=original.Width / 2, height=original.Height)

	Shader(folder + (convertYuv ? "SuperResDiff.cso" : "SuperResDiffRgb.cso"), clip1=3, clip2=2, output=4)

	Shader(folder + (pass==passes ? (convertYuv ? "SuperResFinal.cso" : "SuperResFinalRgb.cso") : "SuperRes.cso"), clip1=1, clip2=4, output=1, \
		param2=string(original.Width/2) + "," + string(original.Height) + "," + string(float(1)/(original.Width/2),"%f") + "," + string(float(1)/original.Height,"%f") + "f", \
		Param3=string(strength,"%f") + "," + string(softness,"%f") + "," + string(pass) + "," + string(passes) + "f")
}