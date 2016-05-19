P="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin(P+"nnedi3.dll")
#LoadPlugin("C:\GitHub\AviSynthShader\Src\Release\Shader.dll")
LoadPlugin("Shader.dll")
Import("Shader.avsi")
#LoadPlugin(P+"FTurn.dll")
Import(P+"AviSynthMT.avsi")
#SetFilterMTMode("DEFAULT_MT_MODE", 2)
Import(P+"edi_rpow2.avsi")
Import(P+"ResizeX.avsi")

AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")

#nnedi3(dh = true, Threads=1)
#turnleft()
#nnedi3(dh = true, Threads=1)
#turnright()

edi_rpow2(2, Threads=2)
#SuperRes(2, .43, 0, """nnedi3_rpow2(2, cshift="Spline16Resize", Threads=2)""", MatrixIn="601")
Prefetch(8)