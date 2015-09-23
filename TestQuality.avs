PluginPath="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin(PluginPath+"masktools2.dll")
LoadPlugin("C:\GitHub\AviSynthShader\Debug\Shader.dll")
directshowsource("F:\AVSMeter\Preview.avi")
#DirectShowSource("F:\NaturalGrounding\Morning Musume\GUNDAN of the Love.mp4", audio=false, pixel_type="YV12", fps=23.976)


#ConvertToYV24()
TestQuality()
#ConvertToYV12()

function TestQuality(clip input)
{
original = input
input = input.ConvertToYV24()
input = input.ConvertToShader()
#input = input.Shader(path="Shaders\Test.cso")
input = input.Shader(path="Shaders\YuvToLinear.cso")
input = input.Shader(path="Shaders\LinearToYuv.cso")
input = input.ConvertFromShader()
input = input.ConvertToYV12()
return mt_makediff(input, original, chroma="process")
}

#SuperRes(5, 1, .25, true, """nnedi3_rpow2(2, cshift="spline16resize")""")
