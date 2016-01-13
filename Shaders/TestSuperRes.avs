PluginPath="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin(PluginPath+"nnedi3.dll")
LoadPlugin("C:\GitHub\AviSynthShader\Src\Debug\Shader.dll")
#LoadPlugin(PluginPath+"Shader.dll")
Import("Shader.avsi")

#SetMTMode(3,8)
AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")
#SetMTMode(2)
SuperRes(1, 1, 0, """nnedi3_rpow2(2, cshift="Spline16Resize", Threads=2)""", MatrixIn="601")
