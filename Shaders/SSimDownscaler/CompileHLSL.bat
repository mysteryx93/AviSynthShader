"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SSimSoftDownscalerX.cso" "SoftDownscaler.hlsl" /Daxis=0
"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SSimSoftDownscalerY.cso" "SoftDownscaler.hlsl" /Daxis=1
"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SSimDownscalerX.cso" "Scalers/Downscaler.hlsl" /Daxis=0
"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SSimDownscalerY.cso" "Scalers/Downscaler.hlsl" /Daxis=1

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SSimDownscaledVarI.cso" "DownscaledVarI.hlsl" /Daxis=0
"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SSimDownscaledVarII.cso" "DownscaledVarII.hlsl" /Daxis=1

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SSimSinglePassConvolver.cso" "SinglePassConvolver.hlsl"
"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SSimCalcR.cso" "CalcR.hlsl"
"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SSimCalc.cso" "Calc.hlsl"