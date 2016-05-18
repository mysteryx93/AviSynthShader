PluginPath="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
#LoadPlugin("C:\GitHub\AviSynthShader\Src\Debug\Shader.dll")
LoadPlugin("Shader.dll")
Import("Shader.avsi")

#SetMTMode(3,8)
AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")
#SetMTMode(2)
#W=Width
#H=Height
SuperResXBR()
#SuperResXBR()
#BicubicResize(int(W*2.5), int(H*2.5), B=0, C=.75)

SSimDownscaler(int(W*2.5), int(H*2.5))
#SuperResXBR(fWidth=int(W*2.5), fHeight=int(H*2.5))
