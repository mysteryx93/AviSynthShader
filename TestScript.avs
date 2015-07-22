PluginPath="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin(PluginPath+"masktools2.dll")
LoadPlugin("C:\GitHub\AviSynthShader\Debug\Shader.dll")
AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")

ConvertToYV24()
#TestQuality(4)
ConvertToShader(1)
Shader(path="Shaders\Sharpen.hlsl", shaderModel="ps_2_0", precision=1, param1="Amount=2f", param2="InputSize=352,288f")
ConvertFromShader(1)
ConvertToYV12()

function TestQuality(clip input, int precision)
{
original = input
input = input.ConvertToYV24()
input = input.ConvertToShader(precision)
input = input.ConvertFromShader(precision)
input = input.ConvertToYV12()
return mt_makediff(input, original, chroma="process")
}

#SuperRes(5, 1, .25, true, """nnedi3_rpow2(2, cshift="spline16resize")""")
