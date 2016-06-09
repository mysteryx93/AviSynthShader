"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResDownscaler.cso" "SuperRes\SSimDownscaler.hlsl" /Daxis=0

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResDownscaleAndDiff.cso" "SuperRes\DownscaleAndDiff.hlsl" /Daxis=1

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResDownscaleAndDiff709.cso" "SuperRes\DownscaleAndDiff709.hlsl" /Daxis=1

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResDownscaleAndDiff601.cso" "SuperRes\DownscaleAndDiff709.hlsl" /Daxis=1 /DKb=0.114 /DKr=0.299

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResDownscaleAndDiffYV709.cso" "SuperRes\DownscaleAndDiffYV.hlsl" /Daxis=1

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResDownscaleAndDiffYV601.cso" "SuperRes\DownscaleAndDiffYV.hlsl" /Daxis=1 /DKb=0.114 /DKr=0.299

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperRes.cso" "SuperRes\SuperRes.hlsl"

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResSkipSoftening.cso" "SuperRes\SuperRes.hlsl" /DSkipSoftening=1

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResFinal.cso" "SuperRes\SuperRes.hlsl" /DFinalPass=1

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResFinal709.cso" "SuperRes\SuperRes709.hlsl" /DFinalPass=1

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResFinal601.cso" "SuperRes\SuperRes709.hlsl" /DFinalPass=1 /DKb=0.114 /DKr=0.299

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "YuvToLinear.cso" "Common\YuvToLinear.hlsl"

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "Yuv601ToLinear.cso" "Common\YuvToLinear.hlsl" /DKb=0.114 /DKr=0.299

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "GammaToYuv.cso" "Common\GammaToYuv.hlsl"

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "GammaToLinear.cso" "Common\GammaToLinear.hlsl"
