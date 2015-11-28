function SuperXBR(clip input, float "edgeStrength", float "weight", bool "thirdPass", bool "convert")
{
	edgeStrength = default(edgeStrength, 1)
	weight = default(weight, 1)
	thirdPass = default(thirdPass, true)
	convert = default(convert, true)
	
	Assert(edgeStrength >= 0 && edgeStrength <= 5, "Edge strength must be between 0 and 5")
	Assert(weight >= 0 && weight <= 1.5, "Weight must be between 0 and 1.5")

	input
    
	convertYuv = convert && !IsRGB()
	sourceFormat = IsYV12() ? "YV12" : IsYV24() ? "YV24" : IsRGB24() ? "RGB24" : "RGB32"

	input = convert ? ConvertToFloat(precision=2) : last
	#convertYuv ? Shader("YuvToGamma.cso") : nop
	
	input
	inputWidth = convert ? width : width / 2
	args_string = string(edgeStrength,"%.32f") + "," + string(weight,"%.32f") + "0,0f"
	size0_string = string(inputWidth) + "," + string(height)+"," + string(1./InputWidth,"%.32f") + "," + string(1./height,"%.32f")+"f"
	size1_string = string(2*inputWidth) + "," + string(2*height) + "," + string(1./(2*inputWidth),"%.32f") + "," + string(1./(2*height),"%.32f")+"f"
    
	Shader("super-xbr-pass0.cso",\
		param2=args_string,\
		param3=size0_string,\
		width=2*inputWidth,height=2*height)

	Shader("super-xbr-pass1.cso",\
		param2=args_string,\
		param3=size1_string)
        
	thirdPass ? Shader("super-xbr-pass2.cso",\
		param2=args_string,\
		param3=size1_string) : nop

	#convertYuv ? Shader("GammaToYuv.cso") : nop
	last.ExecuteShader(input, precision=2, precisionIn=convert?2:2, precisionOut=convert?1:2)

	convert ? ConvertFromFloat(format=sourceFormat,precision=1) : last
}