P="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin("C:\GitHub\AviSynthShader\Src\Release\Shader.dll")
LoadPlugin(P+"dither.dll")
Import(P+"dither.avsi")
Import("SuperResXBR.avsi")
Import(P+"ResizeX.avsi")

SetMTMode(3,8)
AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")
SetMTMode(2)
SuperResXBR(MatrixIn="601")
#SuperResXBR()
#ResizeX(944, 720, 0, 4, -4, -4)