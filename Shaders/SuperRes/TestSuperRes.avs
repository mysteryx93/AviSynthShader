PluginPath="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin(PluginPath+"nnedi3.dll")
LoadPlugin("C:\GitHub\AviSynthShader\Src\Release\Shader.dll")
Import("SuperRes.avsi")

#SetMTMode(3,8)
AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")

#SetMTMode(2)
#nnedi3_rpow2(2, cshift="Spline16Resize", Threads=1)
SuperRes(2, 1, 0, true, """nnedi3_rpow2(2, cshift="Spline16Resize", Threads=1)""")