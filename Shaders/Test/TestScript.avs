PluginPath="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
#SetMTMode(3,8)
LoadPlugin("C:\GitHub\AviSynthShader\Src\Debug\Shader.dll")
#LoadPlugin(PluginPath+"Shader.dll")
AviSource("F:\AVSMeter\Preview.avi").ConvertToYV12()
#ImageSource("Clown.png").ConvertToYV12()

#SetMTMode(2)
ConvertToShader(precision=1)
Test()
ConvertFromShader(precision=1)

function Test(clip input) {
input
#cmd = input.Shader(path="Test.hlsl", shaderModel="ps_3_0")
Shader(path="Sharpen.hlsl", shaderModel="ps_3_0", Param0=".5f", Param1=string(width)+","+string(height)+"f")
return ExecuteShader(last, input, Clip1Precision=1, Precision=3, OutputPrecision=1)
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