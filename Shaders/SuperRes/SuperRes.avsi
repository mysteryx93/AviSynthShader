function SuperRes(clip input, int "passes", float "strength", float "softness", string "upscalecommand", string "folder")
{
	passes = default(passes, 1)
	strength = default(strength, 1)
	softness = default(softness, 0)
	folder = default(folder, ScriptDir())

	Assert((passes > 0 && passes <= 3) ? true : false, chr(10) + "Passes must be between 1 and 3" + chr(10))
	Assert((strength >= 0 && strength <= 1) ? true : false, chr(10) + "Strength must be between 0 and 1" + chr(10))
	Assert((softness >= 0 && softness <= 1) ? true : false, chr(10) + "Softness must be between 0 and 1" + chr(10))
	Assert(Defined(upscalecommand), chr(10) + "You must specify upscalecommand" + chr(10))

	convertYuv = !input.IsRGB()
	sourceFormat = input.IsYV12() ? "YV12" : input.IsYV24() ? "YV24" : input.IsRGB24() ? "RGB24" : "RGB32"

	original = input.ConvertToFloat()
	Eval("input = input." + upscalecommand)
	input = input.ConvertToFloat()
	cmd = input.Shader(folder + (convertYuv ? "YuvToLinear.cso" : "GammaToLinear.cso"))
	cmd = SuperResPass(cmd, input, original, strength, softness, 1, passes, convertYuv, folder)
	cmd = passes > 1 ? cmd.SuperResPass(input, original, strength, softness, 2, passes, convertYuv, folder) : cmd
	cmd = passes > 2 ? cmd.SuperResPass(input, original, strength, softness, 3, passes, convertYuv, folder) : cmd

	input = cmd.ExecuteShader(input, original).ConvertFromFloat(sourceFormat)
	return input
}

function SuperResPass(clip cmd, clip input, clip original, float strength, float softness, int pass, int passes, bool convertYuv, string folder)
{
	cmd = cmd.Shader(folder+"Bicubic.cso", output=3, \
		param1=string(original.Width / 2) + "," + string(original.Height) + "," + string(1./(original.Width / 2),"%f") + "," + string(1./original.Height,"%f") + "f",\
		param2=string(input.Width / 2) + "," + string(input.Height) + "," + string(1./(input.Width / 2),"%.32f") + "," + string(1./input.Height,"%.32f") + "f",\
		param3=string(1/3.,"%.32f") + "," + string(1/3.,"%.32f") + "f",\
		width=original.Width / 2, height=original.Height)

	cmd = cmd.Shader(folder + (convertYuv ? "SuperResDiff.cso" : "SuperResDiffCpu.cso"), clip1=3, clip2=2, output=4)

	cmd = cmd.Shader(folder + (pass==passes ? (convertYuv ? "SuperResFinal.cso" : "SuperResFinalCpu.cso") : "SuperRes.cso"), clip1=1, clip2=4, output=1, \
		param1=string(original.Width/2) + "," + string(original.Height) + "," + string(float(1)/(original.Width/2),"%f") + "," + string(float(1)/original.Height,"%f") + "f", \
		Param2=string(strength,"%f") + "," + string(softness,"%f") + "," + string(pass) + "," + string(passes) + "f")
	return cmd
}