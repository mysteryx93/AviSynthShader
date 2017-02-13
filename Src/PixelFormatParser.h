#pragma once
#include <map>
#include <string>
#include "avisynth.h"

class PixelFormatParser {
	typedef std::map<std::string, int> my_map;
	my_map enumMap;
public:
	PixelFormatParser();
	int GetPixelFormatAsInt(std::string format);
	std::string GetPixelFormatAsString(int pixel_type);
	VideoInfo GetVideoInfo(int pixel_type);
	VideoInfo GetVideoInfo(std::string format);
};