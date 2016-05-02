P="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin("C:\GitHub\AviSynthShader\Src\Debug\Shader.dll")
Import("Shader.avsi")
Import(P+"ResizeX.avsi")

#SetMTMode(3,8)
AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")
#SetMTMode(2)
SuperResXBR(MatrixIn="601")
#SuperResXBR()
#ResizeX(944, 720, 0, 4, -4, -4)