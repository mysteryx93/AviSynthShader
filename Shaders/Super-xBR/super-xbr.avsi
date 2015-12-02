function SuperXBR(clip Input, float "EdgeStrength", float "Sharpness", bool "ThirdPass", int "PrecisionIn")
{
	EdgeStrength = default(EdgeStrength, 1)
	Sharpness = default(Sharpness, 1)
	ThirdPass = default(ThirdPass, true)
	PrecisionIn = default(PrecisionIn, 0)	
	convert = PrecisionIn==0

	Assert(EdgeStrength >= 0 && EdgeStrength <= 5, "EdgeStrength must be between 0 and 5")
	Assert(Sharpness >= 0 && Sharpness <= 1.5, "Sharpness must be between 0 and 1.5")
	Assert(convert || ThirdPass, "ThirdPass is required when PrecisionIn > 0")
	Assert(PrecisionIn >= 0 && PrecisionIn <= 3, "PrecisionIn must be between 0 and 3")

	Input
    
	#convertYuv = convert && !IsRGB()
	sourceFormat = IsYV12() ? "YV12" : IsYV24() ? "YV24" : IsRGB24() ? "RGB24" : "RGB32"

	Input = convert ? ConvertToShader(Precision=2) : last
	
	Input
	InputWidth = Width / (PrecisionIn==0 || PrecisionIn==1 ? 2 : 2)
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
	last.ExecuteShader(Input, Precision=2, PrecisionIn=convert?1:PrecisionIn, PrecisionOut=convert?1:PrecisionIn)

	convert ? ConvertFromShader(Format=sourceFormat, Precision=1) : last

	!ThirdPass ? spline36resize(2*InputWidth, 2*InputHeight, -.5, -.5, 2*InputWidth, 2*InputHeight) : last
}