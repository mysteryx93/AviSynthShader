LoadPlugin("C:\GitHub\AviSynthShader\Debug\Shader.dll")
DirectShowSource("F:\AVSMeter\Preview.avi")

ConvertToYV24()
ConvertToShader(1)
Shader(path="Shaders\Sharpen.hlsl", shaderModel="ps_2_0", precision=1, param1="Amount=2f", param2="InputSize=352,288f")
ConvertFromShader(1)
ConvertToYV12()
