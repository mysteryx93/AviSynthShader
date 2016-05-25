P="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin("C:\GitHub\AviSynthShader\Src\Debug\Shader.dll")
Import("C:\GitHub\AviSynthShader\Shaders\Shader.avsi")
#Import(P+"ResizeX.avsi")

AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")
#SuperResXBR()
#ConvertToRGB24()
SuperResXBR(5, MatrixIn="601", fWidth=int(Width*1.5)/2*2, fHeight=int(Height*1.5)/2*2)
