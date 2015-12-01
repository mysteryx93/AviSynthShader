#SetMTMode(3,8)
LoadPlugin("C:\GitHub\AviSynthShader\Src\Debug\Shader.dll")
#LoadPlugin("C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\Shader.dll")
#AviSource("F:\AVSMeter\Preview.avi")
ImageSource("Clown.png").ConvertToYV12()
#SetMTMode(2)
#ConvertToYV24()
ConvertToFloat(precision=1)
#Shader(path="Shaders\YuvToGamma.cso")
#Shader(path="Shaders\GammaToYuv.cso")
Test()
ConvertFromFloat(precision=3)

function Test(clip input) {
input
#cmd = input.Shader(path="Test.hlsl", shaderModel="ps_3_0")
Shader(path="Sharpen.hlsl", shaderModel="ps_3_0", param1=".5f", param2=string(width)+","+string(height)+"f")
return ExecuteShader(last, input, precisionIn=1, precision=3, precisionOut=3)
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