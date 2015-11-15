function SuperXBR(clip input, float "edgeStrength", float "weight", bool "thirdPass", string "folder", bool "convert")
{
	edgeStrength = default(edgeStrength, 1)
	weight = default(weight, 1)
	thirdPass = default(thirdPass, true)
	convert = default(convert, true)
	folder = default(folder, "")
	precision = 2
	
	Assert(edgeStrength >= 0 && edgeStrength <= 5, "Edge strength must be between 0 and 5")
	Assert(weight >= 0 && weight <= 1.5, "Weight must be between 0 and 1.5")

	input
    
	convertYuv = convert && !IsRGB()
	sourceFormat = IsYV12() ? "YV12" : IsYV24() ? "YV24" : IsRGB24() ? "RGB24" : "RGB32"

	input = convert ? ConvertToFloat(precision=precision) : last
	#convertYuv ? Shader(folder+"YuvToGamma.cso") : nop
	
	input
	args_string = string(edgeStrength,"%.32f") + "," + string(weight,"%.32f") + "0,0f"
	size0_string = string(width/2) + "," + string(height)+"," + string(1./(width/2),"%.32f") + "," + string(1./height,"%.32f")+"f"
	size1_string = string(width) + "," + string(2*height) + "," + string(1./width,"%.32f") + "," + string(1./(2*height),"%.32f")+"f"
    
	Shader(folder+"super-xbr-pass0.cso",\
		param2=args_string,\
		param3=size0_string,\
		width=width,height=2*height)

	Shader(folder+"super-xbr-pass1.cso",\
		param2=args_string,\
		param3=size1_string)
        
	thirdPass ? Shader(folder+"super-xbr-pass2.cso",\
		param2=args_string,\
		param3=size1_string) : nop

	#convertYuv ? Shader(folder+"GammaToYuv.cso") : nop
	last.ExecuteShader(input,precision=precision)

	convert ? ConvertFromFloat(format=sourceFormat,precision=precision) : last
}