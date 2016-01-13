"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResDiff.cso" "SuperRes\Diff.hlsl"

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResDiff709.cso" "SuperRes\Diff709.hlsl"

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResDiff601.cso" "SuperRes\Diff709.hlsl" /DKb=0.114 /DKr=0.299

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperRes.cso" "SuperRes\SuperResEx.hlsl"

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResFinal.cso" "SuperRes\SuperResEx.hlsl" /DFinalPass=1

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResFinal709.cso" "SuperRes\SuperRes709.hlsl" /DFinalPass=1

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResFinal601.cso" "SuperRes\SuperRes709.hlsl" /DFinalPass=1 /DKb=0.114 /DKr=0.299

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "YuvToLinear.cso" "Common\YuvToLinear.hlsl"

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "Yuv601ToLinear.cso" "Common\YuvToLinear.hlsl" /DKb=0.114 /DKr=0.299

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "GammaToYuv.cso" "Common\GammaToYuv.hlsl"

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "GammaToLinear.cso" "Common\GammaToLinear.hlsl"

REM "C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "Bicubic.cso" "SuperRes\Bicubic.hlsl"
