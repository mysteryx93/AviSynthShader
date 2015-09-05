LoadPlugin("C:\GitHub\AviSynthShader - DX9\Debug\Shader.dll")
DirectShowSource("F:\AVSMeter\Preview.avi")

ConvertToYV24()
ConvertToShader(2)
Shader(path="Shaders\Sharpen.hlsl", shaderModel="ps_2_0", precision=2, param1="Amount=1f", param2="InputSize=352,288f")
ConvertFromShader(2)
ConvertToYV12()
