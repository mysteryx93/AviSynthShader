PluginPath="C:\GitHub\AviSynthShader\Shaders\Super-xBR\"
#PluginPath="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin("C:\GitHub\AviSynthShader\Src\Release\Shader.dll")
Import("super-xbr.avsi")

#SetMTMode(3,8)
AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")
#SetMTMode(2)
SuperXBR(edgeStrength=.6, weight=.6, folder=PluginPath)