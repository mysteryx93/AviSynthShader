LoadPlugin("C:\GitHub\AviSynthShader\Debug\Shader.dll")
AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")

ConvertToShader()
ConvertFromShader()
#ConvertToRGB()

#SimpleSample(80, false)
#SuperRes(5, 1, .25, true, """nnedi3_rpow2(2, cshift="spline16resize")""")
