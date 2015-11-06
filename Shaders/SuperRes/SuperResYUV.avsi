function SuperRes(clip input, int "passes", float "strength", float "softness", string "upscalecommand", string "folder")
{
	passes = default(passes, 1)
	strength = default(strength, 1)
	softness = default(softness, 0)
	folder = default(folder, "./")

	Assert((passes > 0 && passes <= 3) ? true : false, chr(10) + "Passes must be between 1 and 3" + chr(10))
	Assert((strength >= 0 && strength <= 1) ? true : false, chr(10) + "Strength must be between 0 and 1" + chr(10))
	Assert((softness >= 0 && softness <= 1) ? true : false, chr(10) + "Softness must be between 0 and 1" + chr(10))
	Assert(Defined(upscalecommand), chr(10) + "You must specify upscalecommand" + chr(10))

	original = input.ConvertToFloat(convertYuv=false)
	Eval("input = input." + upscalecommand)
	input = input.ConvertToFloat(convertYuv=false)
	input = input.Shader(folder+"YuvToGamma.cso").Shader(folder+"GammaToLinear.cso").ExecuteShader(input)
	input = SuperResPass(input, original, strength, softness, 1, passes, folder)
	input = passes > 1 ? input.SuperResPass(original, strength, softness, 2, passes, folder) : input
	input = passes > 2 ? input.SuperResPass(original, strength, softness, 3, passes, folder) : input

	input = input.ConvertFromFloat(convertYuv=false)
	return input
}

function SuperResPass(clip input, clip original, float strength, float softness, int pass, int passes, string folder)
{
	downsample = input.ConvertFromFloat(convertYuv=false,format="RGB32")
	downsample = downsample.BicubicResize(original.Width / 2, original.Height)
	downsample = downsample.ConvertToFloat(convertYuv=false)

	cmd = input.Shader(folder+"YuvToGamma.cso", clip1=3, output=3)
	cmd = cmd.Shader(folder+"SuperResDiff.cso", clip1=2, clip2=3, output=4)

	cmd = cmd.Shader(folder + (pass==passes ? "SuperResFinal.cso" : "SuperRes.cso"), clip1=1, clip2=4, output=1, \
		param1="size1=" + string(original.Width/2) + "," + string(original.Height) + "," + string(float(1)/(original.Width/2),"%f") + "," + string(float(1)/original.Height,"%f") + "f", \
		Param2="args0=" + string(strength,"%f") + "," + string(softness,"%f") + "," + string(pass) + "," + string(passes) + "f")
	cmd = pass == passes ? cmd.Shader(folder+"GammaToYuv.cso", clip1=1, output=1) : cmd
	return cmd.ExecuteShader(input, downsample, original)
}