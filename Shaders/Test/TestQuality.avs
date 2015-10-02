PluginPath="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin(PluginPath+"masktools2.dll")
LoadPlugin("C:\GitHub\AviSynthShader\Shader.dll")
directshowsource("F:\AVSMeter\Preview.avi")


#ConvertToYV24()
TestQuality()
#ConvertToYV12()

function TestQuality(clip input)
{
original = input
input = input.ConvertToFloat(convertYuv=false)
#input = input.Shader(path="Shaders\Test.cso")
input = input.Shader(path="Shaders\YuvToGamma.cso")
input = input.Shader(path="Shaders\GammaToYuv.cso")
input = input.ConvertFromFloat(convertYuv=false)
return mt_makediff(input, original, chroma="process")
}
