set fxc="C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe"
%fxc% /T ps_3_0 /Fo "..\YVToYuv.cso" "YVToYuv.hlsl"
%fxc% /T ps_3_0 /Fo "..\YVToGammaRec709.cso" "YVToGamma.hlsl"
%fxc% /T ps_3_0 /Fo "..\YVToGammaRec601.cso" "YVToGamma.hlsl" /DKb=0.114 /DKr=0.299
%fxc% /T ps_3_0 /Fo "..\YVToGammaPc709.cso" "YVToGamma.hlsl" /DLimitedRange=0
%fxc% /T ps_3_0 /Fo "..\YVToGammaPc601.cso" "YVToGamma.hlsl" /DKb=0.114 /DKr=0.299 /DLimitedRange=0
%fxc% /T ps_3_0 /Fo "..\YVToLinearRec709.cso" "YVToLinear.hlsl"
%fxc% /T ps_3_0 /Fo "..\YVToLinearRec601.cso" "YVToLinear.hlsl" /DKb=0.114 /DKr=0.299
%fxc% /T ps_3_0 /Fo "..\YVToLinearPc709.cso" "YVToLinear.hlsl" /DLimitedRange=0
%fxc% /T ps_3_0 /Fo "..\YVToLinearPc601.cso" "YVToLinear.hlsl" /DKb=0.114 /DKr=0.299 /DLimitedRange=0
%fxc% /T ps_3_0 /Fo "..\YuvToLinearRec709.cso" "YuvToLinear.hlsl"
%fxc% /T ps_3_0 /Fo "..\YuvToLinearRec601.cso" "YuvToLinear.hlsl" /DKb=0.114 /DKr=0.299
%fxc% /T ps_3_0 /Fo "..\YuvToLinearPc709.cso" "YuvToLinear.hlsl" /DLimitedRange=0
%fxc% /T ps_3_0 /Fo "..\YuvToLinearPc601.cso" "YuvToLinear.hlsl" /DKb=0.114 /DKr=0.299 /DLimitedRange=0
%fxc% /T ps_3_0 /Fo "..\YuvToGammaRec709.cso" "YuvToGamma.hlsl"
%fxc% /T ps_3_0 /Fo "..\YuvToGammaRec601.cso" "YuvToGamma.hlsl" /DKb=0.114 /DKr=0.299
%fxc% /T ps_3_0 /Fo "..\YuvToGammaPc709.cso" "YuvToGamma.hlsl" /DLimitedRange=0
%fxc% /T ps_3_0 /Fo "..\YuvToGammaPc601.cso" "YuvToGamma.hlsl" /DKb=0.114 /DKr=0.299 /DLimitedRange=0
%fxc% /T ps_3_0 /Fo "..\GammaToYuvRec709.cso" "GammaToYuv.hlsl"
%fxc% /T ps_3_0 /Fo "..\GammaToYuvRec601.cso" "GammaToYuv.hlsl" /DKb=0.114 /DKr=0.299
%fxc% /T ps_3_0 /Fo "..\GammaToYuvPc709.cso" "GammaToYuv.hlsl" /DLimitedRange=0
%fxc% /T ps_3_0 /Fo "..\GammaToYuvPc601.cso" "GammaToYuv.hlsl" /DKb=0.114 /DKr=0.299 /DLimitedRange=0
%fxc% /T ps_3_0 /Fo "..\LinearToYuvRec709.cso" "LinearToYuv.hlsl"
%fxc% /T ps_3_0 /Fo "..\LinearToYuvRec601.cso" "LinearToYuv.hlsl" /DKb=0.114 /DKr=0.299
%fxc% /T ps_3_0 /Fo "..\LinearToYuvPc709.cso" "LinearToYuv.hlsl" /DLimitedRange=0
%fxc% /T ps_3_0 /Fo "..\LinearToYuvPc601.cso" "LinearToYuv.hlsl" /DKb=0.114 /DKr=0.299 /DLimitedRange=0
%fxc% /T ps_3_0 /Fo "..\GammaToLinear.cso" "GammaToLinear.hlsl"
%fxc% /T ps_3_0 /Fo "..\LinearToGamma.cso" "LinearToGamma.hlsl"

%fxc% /T ps_3_0 /Fo "..\SuperXBR-pass0.cso" "SuperXBR.hlsl" /DPass=0
%fxc% /T ps_3_0 /Fo "..\SuperXBR-pass1.cso" "SuperXBR.hlsl" /DPass=1
%fxc% /T ps_3_0 /Fo "..\SuperXBR-pass2.cso" "SuperXBR.hlsl" /DPass=2

%fxc% /T ps_3_0 /Fo "..\SSimDownscalerX.cso" "SSimDownscaler.hlsl" /Daxis=0
%fxc% /T ps_3_0 /Fo "..\SuperResDownscaleAndDiff.cso" "SuperResDownscaleAndDiff.hlsl" /Daxis=1
%fxc% /T ps_3_0 /Fo "..\SuperResDownscaleAndDiffRec709.cso" "SuperResDownscaleAndDiff.hlsl" /Daxis=1 /DConvertGamma=1
%fxc% /T ps_3_0 /Fo "..\SuperResDownscaleAndDiffRec601.cso" "SuperResDownscaleAndDiff.hlsl" /Daxis=1 /DConvertGamma=1 /DKb=0.114 /DKr=0.299
%fxc% /T ps_3_0 /Fo "..\SuperResDownscaleAndDiffPc709.cso" "SuperResDownscaleAndDiff.hlsl" /Daxis=1 /DConvertGamma=1 /DLimitedRange=0
%fxc% /T ps_3_0 /Fo "..\SuperResDownscaleAndDiffPc601.cso" "SuperResDownscaleAndDiff.hlsl" /Daxis=1 /DConvertGamma=1 /DKb=0.114 /DKr=0.299 /DLimitedRange=0
%fxc% /T ps_3_0 /Fo "..\SuperRes.cso" "SuperRes.hlsl" /Daxis=1
%fxc% /T ps_3_0 /Fo "..\SuperResSkipSoft.cso" "SuperRes.hlsl" /Daxis=1 /DSkipSoftening=1
%fxc% /T ps_3_0 /Fo "..\SuperResFinal.cso" "SuperRes.hlsl" /Daxis=1 /DFinalPass=1
%fxc% /T ps_3_0 /Fo "..\SuperResFinalRec709.cso" "SuperRes.hlsl" /Daxis=1 /DFinalPass=1 /DConvertGamma=1
%fxc% /T ps_3_0 /Fo "..\SuperResFinalRec601.cso" "SuperRes.hlsl" /Daxis=1 /DFinalPass=1 /DConvertGamma=1 /DKb=0.114 /DKr=0.299
%fxc% /T ps_3_0 /Fo "..\SuperResFinalPc709.cso" "SuperRes.hlsl" /Daxis=1 /DFinalPass=1 /DConvertGamma=1 /DLimitedRange=0
%fxc% /T ps_3_0 /Fo "..\SuperResFinalPc601.cso" "SuperRes.hlsl" /Daxis=1 /DFinalPass=1 /DConvertGamma=1 /DKb=0.114 /DKr=0.299 /DLimitedRange=0

%fxc% /T ps_3_0 /Fo "..\SSimDownscalerX.cso" "SSimDownscaler.hlsl" /Daxis=0
%fxc% /T ps_3_0 /Fo "..\SSimDownscalerY.cso" "SSimDownscaler.hlsl" /Daxis=1
%fxc% /T ps_3_0 /Fo "..\SSimSoftDownscalerX.cso" "SSimSoftDownscaler.hlsl" /Daxis=0
%fxc% /T ps_3_0 /Fo "..\SSimSoftDownscalerY.cso" "SSimSoftDownscaler.hlsl" /Daxis=1
%fxc% /T ps_3_0 /Fo "..\SSimDownscaledVarI.cso" "SSimDownscaledVarI.hlsl" /Daxis=0
%fxc% /T ps_3_0 /Fo "..\SSimDownscaledVarII.cso" "SSimDownscaledVarII.hlsl" /Daxis=1
%fxc% /T ps_3_0 /Fo "..\SSimSinglePassConvolver.cso" "SSimSinglePassConvolver.hlsl"
%fxc% /T ps_3_0 /Fo "..\SSimCalcR.cso" "SSimCalcR.hlsl"
%fxc% /T ps_3_0 /Fo "..\SSimCalc.cso" "SSimCalc.hlsl"
%fxc% /T ps_3_0 /Fo "..\Dither.cso" "Dither.hlsl"
%fxc% /T ps_3_0 /Fo "..\OutputY.cso" "OutputY.hlsl"
%fxc% /T ps_3_0 /Fo "..\OutputU.cso" "OutputU.hlsl"
%fxc% /T ps_3_0 /Fo "..\OutputV.cso" "OutputV.hlsl"

REM %fxc% /T ps_3_0 /Fo "..\Bicubic.cso" "Bicubic.hlsl"
