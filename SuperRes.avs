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

original = input.ConvertToYV24().ConvertToShader().Shader(path="Shaders\YuvToGamma.cso")
Eval("input = input." + upscalecommand)
input = input.ConvertToYV24().ConvertToShader()
input = input.Shader("Shaders\GammaToLinear.cso")
input = SuperResPass(input, original, strength, softness, hqdownscaling)
input = passes > 1 ? input.SuperResPass(original, strength, softness, hqdownscaling) : input
input = passes > 2 ? input.SuperResPass(original, strength, softness, hqdownscaling) : input
input = passes > 3 ? input.SuperResPass(original, strength, softness, hqdownscaling) : input
input = passes > 4 ? input.SuperResPass(original, strength, softness, hqdownscaling) : input

input.Shader(path="Shaders\LinearToGamma.cso")
input = input.Shader(path="Shaders\GammaToYuv.cso").ConvertFromShader().ConvertToYV12()
return input
}

function SuperResPass(clip input, clip original, float strength, float softness, bool hqdownscaling)
{
downsample = hqdownscaling==true ? input.BicubicResize(original.Width, original.Height) : input.BilinearResize(original.Width, original.Height)
diff = downsample.Shader("Shaders\SuperResDiff.cso", clip1=original)
#return diff
return input.Shader("F:\AVSMeter\MPDN_Extensions-master\Extensions\RenderScripts\SuperRes\SuperResEx.hlsl", shaderModel="ps_3_0", clip1=diff, param1="p1=352,288f")
#return input.Shader("Shaders\SuperRes.cso", clip1=diff, param1="height=352f")
#param1="p1=" + string(input.Width/2) + "," + string(input.Height) + ",0,0f", Param2="args0=" + string(float(1)/(input.Width/2),"%1.8f") + "," + string(float(1)/input.Height,"%1.8f") + "f"
}