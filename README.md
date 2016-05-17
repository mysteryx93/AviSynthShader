# AviSynth Shader v1.4

<a href="https://github.com/mysteryx93/AviSynthShader/releases">Download here >></a>

This plugin allows running HLSL pixel shaders within AviSynth. This gives access to various HLSL filters that haven't been programmed in AviSynth.

This implementation allows running several shaders in a row. Shader() returns a clip containing commands, and each subsequent to Shader() adds a line to that command chain clip. You must call ExecuteShader() to execute the chain of commands. You must also call ConvertToShader() before and ConvertFromShader() at the end.

The following example will run Diff1 and Diff2 on the clip before returning a Merge of both results. (these shader names are fictive, you have to use real shaders!)

    ConvertToShader(1)
    Input
    Shader("Diff1.cso", Output=2)
    Shader("Diff2.cso", Output=3)
    Shader("Merge.cso", Clip1=2, Clip2=3, Output=1)
    ShaderExecute(last, Input, Clip1Precision=1, Precision=3, OutputPrecision=1)
    ConvertFromShader(1)

It is recommended to use AviSynth MT so that the CPU can work on other threads while waiting for results from the GPU.
ConvertToShader, ConvertFromShader and Shader support MT=1. ExecuteShader supports MT=2.

## Syntax:

#### ConvertToShader(Input, Precision, lsb)
Converts a YV12, YV24 or RGB32 clip into a wider frame containing UINT16 or half-float data. Clips must be converted in such a way before running any shader.

16-bit-per-channel half-float data isn't natively supported by AviSynth. It is stored in a RGB32 container with a Width that is twice larger. When using Clip.Width, you must divine by 2 to get the accurate width.

Arguments:  
Precision: 1 to convert into BYTE, 2 to convert into UINT16, 3 to convert into half-float. Default=2  
lsb: Whether to convert from DitherTools' Stack16 format. Only YV12 and YV24 are supported. Default=false

#### ConvertFromShader(Input, Precision, Format, lsb)
Convert a half-float clip into a standard YV12, YV24 or RGB32 clip.

Arguments:  
Precision: 1 to convert into BYTE, 2 to convert into UINT16, 3 to convert into half-float. Default=2  
Format: The video format to convert to. Valid formats are YV12, YV24 and RGB32. Default=YV12.  
lsb: Whether to convert to DitherTools' Stack16 format. Only YV12 and YV24 are supported. Default=false

#### Shader(Input, Path, EntryPoint, ShaderModel, Param1-Param9, Clip1-Clip9, Output, Width, Height)
Runs a HLSL pixel shader on specified clip. You can either run a compiled .cso file or compile a .hlsl file.

Arguments:  
Input: The first input clip.  
Path: The path to the HLSL pixel shader file to run. If not specified, Clip1 will be copied to Output.  
EntryPoint: If compiling HLSL source code, specified the code entry point.  
ShaderModel: If compiling HLSL source code, specified the shader model. Usually PS_2_0 or PS_3_0  
Param0-Param8: Sets each of the shader parameters.  
Ex: "float4 p4 : register(c4);" will be set with Param4="1,1,1,1f"  
End each value with 'f'(float), 'i'(int) or 'b'(bool) to specify its type.  
Param0 corresponds to c0, Param1 corresponds to c1, etc.  
If setting float or int, you can set a vector or 2, 3 or 4 elements by separating the values with ','.  
Clip1-Clip9: The index of the clips to set into this shader. Input clips are defined when calling ExecuteShader. Clip1 sets 's0' within the shader, while clip2-clip9 set 's1' to 's8'. The order is important.  
Default for clip1 is 1, for clip2-clip9 is 0 which means no source clip.  
Output: The clip index where to write the output of this shader, between 1 and 9. Default is 1 which means it will be the output of ExecuteShader. If set to another value, you can use it as the input of another shader. The last shader in the chain must have output=1.  
Width, Height: The size of the output texture. Default = same as input texture.  

#### ExecuteShader(cmd, Clip1-Clip9, Clip1Precision-Clip9Precision, Precision, OutputPrecision)
Executes the chain of commands on specified input clips.

Arguments:  
cmd: A clip containing the commands returned by calling Shader.  
Clip1-Clip9: The clips on which to run the shaders.  
Clip1Precision-Clip9Precision: 1 if input clips is BYTE, 2 if UINT16, 3 if half-float. Default=2 or the value of the previous clip  
Precision: 1 to execute with 8-bit precision, 2 to execute with 16-bit precision, 3 to execute with half-float precision. Default=2  
OutputPrecision: 1 to get an output clip with BYTE, 2 for UINT16, 3 for half-float. Default=2  

#### SuperResXBR(Input, Passes, Str, Soft, XbrStr, XbrSharp, MatrixIn, MatrixOut, FormatOut, Convert, ConvertYuv, lsb_in, lsb_out, fWidth, fHeight, fStr, fSoft)
Enhances upscaling quality, combining Super-xBR and SuperRes to run in the same command chain, reducing memory transfers and increasing performance.

Arguments Passes, Str, Soft are the same as SuperRes.
Arguments XbrStr, XbrSharp are the same as SuperXBR.
Arguments fWidth, fHeight, fStr, fSoft are the same as SSimDownscaler and allows downscaling the output before reading back from GPU


#### SuperRes(Input, Passes, Str, Soft, Upscale, MatrixIn, MatrixOut, FormatOut, Convert, ConvertYuv, lsb_in, lsb_upscale, lsb_out, fWidth, fHeight, fStr, fSoft)
Enhances upscaling quality.

Arguments:  
Passes: How many SuperRes passes to run. Default=1.  
Str: How agressively we want to run SuperRes, between 0 and 1. Default=1.  
Soft: How much smoothness we want to add, between 0 and 1. Default=0.  
Upscale: An upscaling command that must contain offset-correction. Ex: """nnedi3_rpow2(2, cshift="Spline16Resize")"""  
MatrixIn/MatrixOut: The input and output color matrix (601 or 709). This can be used for color matrix conversion. Default="709" for both  
FormatOut: The output format. Default = same as input.  
Convert: Whether to call ConvertToShader and ConvertFromShader within the shader. Default=true  
ConvertYuv: Whether do YUV-RGB color conversion. Default=true unless Convert=true and source is RGB  
lsb_in, lsb_upscale, lsb_out: Whether the input, result of Upscale and output are to be converted to/from DitherTools' Stack16 format. Default=false
fWidth, fHeight, fStr, fSort: Allows downscaling the output before reading back from GPU. See SSimDownscaler.


#### Super-xBR(Input, Str, Sharp, FormatOut, Convert, lsb_in, lsb_out, fWidth, fHeight, fStr, fSoft)
Doubles the size of the image. Produces a sharp result, but with severe ringing.

Arguments:  
Str: Value between 0 and 5 specifying the strength. Default=1.  
Sharp: Value between 0 and 1.5 specifying the weight. Default=1.  
FormatOut: The output format. Default = same as input.  
Convert: Whether to call ConvertToShader and ConvertFromShader within the shader. Default=true  
lsb_in, lsb_out: Whether the input and output are to be converted to/from DitherTools' Stack16 format. Default=false
fWidth, fHeight, fStr, fSort: Allows downscaling the output before reading back from GPU. See SSimDownscaler.


#### SSimDownscaler(Input, W, H, Str, Soft, MatrixIn, MatrixOut, FormatOut, Convert, lsb_in, lsb_out)
Downscales the image in high quality.

Arguments:
W: The width to resize to.
H: The height to resize to.
Str: The algorithm strength to apply.
Soft: If true, the result will be softer.
Other arguments are the same as SuperRes.


#### ColorMatrixShader(input, MatrixIn, MatrixOut, FormatOut)
Converts the color matrix with 16 bit depth to avoid banding. Source can be YV12, YV24, RGB24 or RGB32.

Arguments:  
MatrixIn/MatrixOut: The input and output color matrix (601 or 709). Default="709" for both  
FormatOut: The output format. Default = same as input.


Shiandow provides many other HLSL shaders available here that can be integrated into AviSynth.  
https://github.com/zachsaw/MPDN_Extensions/tree/master/Extensions/RenderScripts
