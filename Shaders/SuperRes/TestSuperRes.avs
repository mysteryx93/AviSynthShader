PluginPath="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin(PluginPath+"nnedi3.dll")
LoadPlugin("C:\GitHub\AviSynthShader\Shader.dll")
Import("SuperRes.avsi")

AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")

SuperRes(2, .5, .1, true, """nnedi3_rpow2(2, cshift="Spline16Resize")""")