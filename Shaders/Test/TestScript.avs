#SetMTMode(3,8)
LoadPlugin("C:\GitHub\AviSynthShader\Src\Debug\Shader.dll")
#LoadPlugin("C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\Shader.dll")
AviSource("F:\AVSMeter\Preview.avi")
#SetMTMode(2)
#ConvertToYV24()
ConvertToFloat()
#Shader(path="Shaders\YuvToGamma.cso")
#cmd = Shader(path="Sharpen.hlsl", shaderModel="ps_3_0", param1="Amount=1f", param2="InputSize=352,288f")
#Shader(path="Shaders\GammaToYuv.cso")
Test()
ConvertFromFloat()
#Distributor()

function Test(clip input) {
cmd = input.Shader(path="Test.hlsl", shaderModel="ps_3_0")
cmd = cmd.Shader(path="Sharpen.hlsl", shaderModel="ps_3_0", param1=".5f", param2="352,288f")
cmd = cmd.Shader(path="Sharpen.hlsl", shaderModel="ps_3_0", param1="2f", param2="352,288f")
#cmd = cmd.Shader(path="Sharpen.hlsl", shaderModel="ps_3_0", param1="1f", param2="352,288f")
#cmd = cmd.Shader(path="Sharpen.hlsl", shaderModel="ps_3_0", param1="1f", param2="352,288f")
#cmd = cmd.Shader(path="Sharpen.hlsl", shaderModel="ps_3_0", param1="1f", param2="352,288f")
return cmd.ExecuteShader(input)
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