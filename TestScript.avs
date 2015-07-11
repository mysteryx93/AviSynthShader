PluginPath="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin(PluginPath+"masktools2.dll")
LoadPlugin("C:\GitHub\AviSynthShader\Debug\Shader.dll")
AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")

ConvertToShader(1)
# ConvertFromShader()

#TestQuality()

function TestQuality(clip input)
{
original = input
input = ConvertToShader(input)
input = input.ConvertFromShader()
return mt_makediff(input, original, chroma="process")
}

#SuperRes(5, 1, .25, true, """nnedi3_rpow2(2, cshift="spline16resize")""")
