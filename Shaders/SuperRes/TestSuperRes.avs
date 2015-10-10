PluginPath="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin(PluginPath+"nnedi3.dll")
LoadPlugin("C:\GitHub\AviSynthShader\Src\Debug\Shader.dll")
Import("SuperRes.avsi")
#ColorBarsHD(300, 200).ConvertToYV12()

#SetMTMode(3,4)
AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")
#SetMTMode(2)
#nnedi3_rpow2(2, cshift="Spline16Resize", Threads=1)
SuperRes(2, .42, 0, true, """nnedi3_rpow2(2, cshift="Spline16Resize", Threads=2)""")
#ConvertToFloat(convertYuv=false)
#ConvertFromFloat(convertYuv=false)
#Distributor()