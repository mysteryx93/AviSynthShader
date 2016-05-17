P="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin("Shader.dll")
Import("Shader.avsi")
#LoadPlugin(P+"Shaders\Shader.dll")
#Import(P+"Shaders\Shader.avsi")
#Import(P+"ResizeX.avsi")

SetFilterMTMode("DEFAULT_MT_MODE",2)
SetFilterMTMode("AviSource",3)

#SetMTMode(3,8)
AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")
#SetMTMode(2)
#SuperResXBR(2, 0.6, 0, XbrStr=2, XbrSharp=1.2, MatrixIn="601")
#SuperResXBR()
#ResizeX(944, 720, 0, 4, -4, -4)

#ConvertToYV24()
SuperResXBR(2, 0.6, 0, XbrStr=2, XbrSharp=1.2, MatrixIn="601")
#ConvertToYV12()
SuperResXBR(2, 0.6, 0, XbrStr=2, XbrSharp=1.2, fWidth=600, fHeight=400)
Prefetch(8)