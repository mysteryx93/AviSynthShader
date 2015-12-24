function SuperXBR(clip Input, float "EdgeStrength", float "Sharpness", bool "ThirdPass", bool "Convert", bool "lsb_in", bool "lsb_out")
{
	EdgeStrength = default(EdgeStrength, 1)
	Sharpness = default(Sharpness, 1)
	ThirdPass = default(ThirdPass, true)
	Convert = default(Convert, true)
	lsb_in = default(lsb_in, false)
	lsb_out = default(lsb_out, false)

	Assert(EdgeStrength >= 0 && EdgeStrength <= 5, "EdgeStrength must be between 0 and 5")
	Assert(Sharpness >= 0 && Sharpness <= 1.5, "Sharpness must be between 0 and 1.5")
	Assert(Convert || ThirdPass, "ThirdPass is required when PrecisionIn > 0")
	Assert((!lsb_in && !lsb_out) || Convert, "Convert must be True to use lsb_in, lsb_upscale or lsb_out")

    	Input
	#convertYuv = convert && !IsRGB()
	PrecisionIn = Convert && !lsb_in ? 1 : 2
	PrecisionOut = Convert && !lsb_out ? 1 : 2
	sourceFormat = IsYV12 ? "YV12" : IsYV24 ? "YV24" : IsRGB24 ? "RGB24" : "RGB32"

	Input = convert ? ConvertToShader(PrecisionIn, lsb=lsb_in) : last
	
	Input
	InputWidth = Width / PrecisionIn
	InputHeight = Height
	args_string = string(EdgeStrength,"%.32f") + "," + string(Sharpness,"%.32f") + ",0,0f"
	size0_string = string(InputWidth) + "," + string(InputHeight) + "," + string(1./InputWidth,"%.32f") + "," + string(1./InputHeight,"%.32f") + "f"
	size1_string = string(2*InputWidth) + "," + string(2*InputHeight) + "," + string(1./(2*InputWidth),"%.32f") + "," + string(1./(2*InputHeight),"%.32f") + "f"
    
	# It works just as well in YUV colorspace
	#convertYuv ? Shader("YuvToGamma.cso") : nop

	Shader("super-xbr-pass0.cso",\
		Param2 = args_string,\
		Param3 = size0_string,\
		Width = 2*InputWidth, Height = 2*InputHeight)

	Shader("super-xbr-pass1.cso",\
		Param2=args_string,\
		Param3=size1_string)
        
	ThirdPass ? Shader("super-xbr-pass2.cso",\
		Param2=args_string,\
		Param3=size1_string) : nop

	#convertYuv ? Shader("GammaToYuv.cso") : nop
	last.ExecuteShader(Input, Precision=2, Clip1Precision=PrecisionIn, OutputPrecision=PrecisionOut)

	convert ? ConvertFromShader(PrecisionOut, Format=sourceFormat, lsb=lsb_out) : last
	info

	!ThirdPass && convert ? spline36resize(2*InputWidth, 2*InputHeight, -.5, -.5, 2*InputWidth, 2*InputHeight) : last
}