PluginPath="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
LoadPlugin("C:\GitHub\AviSynthShader\Src\Release\Shader.dll")
#LoadPlugin(PluginPath+"Shader.dll")
#LoadPlugin(PluginPath+"ColorMatrix.dll")
Import("Shader.avsi")

AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")
#ColorMatrix(mode="Rec.601->Rec.709")
ColorMatrixShader(MatrixOut="601")
#ColorMatrix601to709()
#ColorMatrix709to601()