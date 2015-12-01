function SuperXBR(clip input, float "edgeStrength", float "sharpness", bool "thirdPass", bool "convert")
{
	edgeStrength = default(edgeStrength, 1)
	sharpness = default(sharpness, 1)
	thirdPass = default(thirdPass, true)
	convert = default(convert, true)
	
	Assert(edgeStrength >= 0 && edgeStrength <= 5, "EdgeStrength must be between 0 and 5")
	Assert(sharpness >= 0 && sharpness <= 1.5, "Sharpness must be between 0 and 1.5")
	Assert(convert || thirdPass, "ThirdPass is required when Convert=false")

	input
    
	convertYuv = convert && !IsRGB()
	sourceFormat = IsYV12() ? "YV12" : IsYV24() ? "YV24" : IsRGB24() ? "RGB24" : "RGB32"

	input = convert ? ConvertToFloat(precision=1) : last
	
	input
	inputWidth = convert ? width : width / 2
	inputHeight = height
	args_string = string(edgeStrength,"%.32f") + "," + string(sharpness,"%.32f") + "0,0f"
	size0_string = string(inputWidth) + "," + string(inputHeight)+"," + string(1./InputWidth,"%.32f") + "," + string(1./height,"%.32f")+"f"
	size1_string = string(2*inputWidth) + "," + string(2*inputHeight) + "," + string(1./(2*inputWidth),"%.32f") + "," + string(1./(2*inputHeight),"%.32f")+"f"
    
	# It works just as well in YUV colorspace
	#convertYuv ? Shader("YuvToGamma.cso") : nop

	Shader("super-xbr-pass0.cso",\
		param2=args_string,\
		param3=size0_string,\
		width=2*inputWidth,height=2*inputHeight)

	Shader("super-xbr-pass1.cso",\
		param2=args_string,\
		param3=size1_string)
        
	thirdPass ? Shader("super-xbr-pass2.cso",\
		param2=args_string,\
		param3=size1_string) : nop

	#convertYuv ? Shader("GammaToYuv.cso") : nop
	last.ExecuteShader(input, precision=2, precisionIn=convert?1:2, precisionOut=convert?1:2)

	convert ? ConvertFromFloat(format=sourceFormat, precision=1) : last

	!thirdPass ? spline36resize(width, height, -.5, -.5, width, height) : last
}