function SuperRes(clip input, int "passes", float "strength", float "softness", bool "hqdownscaling", string "upscalecommand")
{
passes = default(passes, 3)
strength = default(strength, 1)
softness = default(softness, 0)
hqdownscaling = default(hqdownscaling, true)

Assert((passes > 0 && passes <= 5) ? true : false, chr(10) + "Passes must be between 1 and 5" + chr(10))
Assert((strength >= 0 && strength <= 1) ? true : false, chr(10) + "Strength must be between 0 and 1" + chr(10))
Assert((softness >= 0 && softness <= 1) ? true : false, chr(10) + "Softness must be between 0 and 1" + chr(10))
Assert((softness >= 0 && softness <= 1) ? true : false, chr(10) + "Softness must be between 0 and 1" + chr(10))
Assert(Defined(upscalecommand), chr(10) + "You must specify upscalecommand" + chr(10))

original = input
Eval("input = input." + upscalecommand)
input = input.SuperResPass(original, strength, softness, hqdownscaling)
input = passes > 1 ? input.SuperResPass(original, strength, softness, hqdownscaling) : input
input = passes > 2 ? input.SuperResPass(original, strength, softness, hqdownscaling) : input
input = passes > 3 ? input.SuperResPass(original, strength, softness, hqdownscaling) : input
input = passes > 4 ? input.SuperResPass(original, strength, softness, hqdownscaling) : input

return input
}

function SuperResPass(clip c, clip original, float strength, float softness, bool hqdownscaling)
{
downsample = hqdownscaling==true ? c.BicubicResize(original.Width, original.Height) : c.BilinearResize(original.Width, original.Height)
diff = mt_makediff(downsample, original, chroma="process")
# return SuperResCore(c, diff, strength, softness)
diff = diff.BicubicResize(c.width, c.height)
return mt_makediff(c, diff, chroma="process")
}