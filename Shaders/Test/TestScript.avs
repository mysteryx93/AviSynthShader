PluginPath="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
#SetMTMode(3,8)
LoadPlugin("C:\GitHub\AviSynthShader\Src\Debug\Shader.dll")
#LoadPlugin(PluginPath+"Shader.dll")

AviSource("F:\AVSMeter\Preview.avi").ConvertToYV12()
#ImageSource("Clown.png").ConvertToYV12()
#ConvertToYV24()
#edi_rpow2(2, nns=4, cshift="Spline16Resize", lsb=true, Threads=2)
#ConvertToShader(precision=2, stack16=true)
#DitherPost()

#SetMTMode(2)
#ConvertToYV24()
ConvertToShader(precision=1)
#Shader(path="Shaders\YuvToGamma.cso")
#Shader(path="Shaders\GammaToYuv.cso")
Test()
ConvertFromShader(precision=1)

function Test(clip input) {
cmd = input.Shader(path="Test5.hlsl", shaderModel="ps_5_0")
#Shader(path="Sharpen.hlsl", shaderModel="ps_3_0", param1=".5f", param2=string(width)+","+string(height)+"f")
return cmd.ExecuteShader(input, Clip1Precision=1, Precision=2, OutputPrecision=1)
}

function Test2(clip input) {
input = input.Shader(path="Test.hlsl", shaderModel="ps_3_0").ExecuteShader(input)
input = input.Shader(path="Test.hlsl", shaderModel="ps_3_0").ExecuteShader(input)
input = input.Shader(path="Sharpen.hlsl", shaderModel="ps_3_0", param1="Amount=1f", param2="InputSize=352,288f").ExecuteShader(input)
input = input.Shader(path="Sharpen.hlsl", shaderModel="ps_3_0", param1="Amount=.5f", param2="InputSize=352,288f").ExecuteShader(input)
input = input.Shader(path="Sharpen.hlsl", shaderModel="ps_3_0", param1="Amount=.5f", param2="InputSize=352,288f").ExecuteShader(input)
input = input.Shader(path="Sharpen.hlsl", shaderModel="ps_3_0", param1="Amount=.5f", param2="InputSize=352,288f").ExecuteShader(input)
return input
}