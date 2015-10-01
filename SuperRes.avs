PluginPath="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin(PluginPath+"nnedi3.dll")
LoadPlugin(PluginPath+"masktools2.dll")
LoadPlugin("C:\GitHub\AviSynthShader\Debug\Shader.dll")
AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")

SuperRes(1, 1, 0, true, """nnedi3_rpow2(2, cshift="Spline16Resize")""")

function SuperRes(clip input, int "passes", float "strength", float "softness", bool "hqdownscaling", string "upscalecommand")
{
passes = default(passes, 3)
strength = default(strength, 1)
softness = default(softness, 0)
hqdownscaling = default(hqdownscaling, true)

Assert((passes > 0 && passes <= 5) ? true : false, chr(10) + "Passes must be between 1 and 5" + chr(10))
Assert((strength >= 0 && strength <= 1) ? true : false, chr(10) + "Strength must be between 0 and 1" + chr(10))
Assert((softness >= 0 && softness <= 1) ? true : false, chr(10) + "Softness must be between 0 and 1" + chr(10))
Assert(Defined(upscalecommand), chr(10) + "You must specify upscalecommand" + chr(10))

original = input.ConvertToYV24().ConvertToShader() #.Shader(path="Shaders\YuvToGamma.cso")
Eval("input = input." + upscalecommand)
input = input.ConvertToYV24().ConvertToShader()
input = input.Shader("Shaders\GammaToLinear.cso")
input = SuperResPass(input, original, strength, softness, hqdownscaling, 1, passes)
input = passes > 1 ? input.SuperResPass(original, strength, softness, hqdownscaling, 2, passes) : input
input = passes > 2 ? input.SuperResPass(original, strength, softness, hqdownscaling, 3, passes) : input
input = passes > 3 ? input.SuperResPass(original, strength, softness, hqdownscaling, 4, passes) : input
input = passes > 4 ? input.SuperResPass(original, strength, softness, hqdownscaling, 5, passes) : input

input.Shader(path="Shaders\LinearToGamma.cso")
input = input.ConvertFromShader().ConvertToYV12() #.Shader(path="Shaders\GammaToYuv.cso")
return input
}

function SuperResPass(clip input, clip original, float strength, float softness, bool hqdownscaling, int pass, int passes)
{
downsample = hqdownscaling==true ? input.BicubicResize(original.Width, original.Height) : input.BilinearResize(original.Width, original.Height)
diff = downsample.Shader("Shaders\SuperResDiff.cso", clip1=original)

return input.Shader(pass==passes ? "Shaders\SuperResFinal.cso" : "Shaders\SuperRes.cso", clip1=diff, \
	param1="size1=" + string(original.Width/2) + "," + string(original.Height) + "," + string(float(1)/(original.Width/2),"%f") + "," + string(float(1)/original.Height,"%f") + "f", \
	Param2="args0=" + string(strength,"%f") + "," + string(softness,"%f") + "f")
}