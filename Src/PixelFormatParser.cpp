#include "PixelFormatParser.h"

PixelFormatParser::PixelFormatParser() {
	enumMap["RGB24"] = VideoInfo::CS_BGR24;
	enumMap["RGB32"] = VideoInfo::CS_BGR32;
	enumMap["YUY2"] = VideoInfo::CS_YUY2;
	enumMap["RAW32"] = VideoInfo::CS_RAW32;
	enumMap["YV24"] = VideoInfo::CS_YV24;
	enumMap["YV16"] = VideoInfo::CS_YV16;
	enumMap["YV12"] = VideoInfo::CS_YV12;
	enumMap["I420"] = VideoInfo::CS_I420;
	enumMap["IYUV"] = VideoInfo::CS_IYUV;
	enumMap["YUV9"] = VideoInfo::CS_YUV9;
	enumMap["YV411"] = VideoInfo::CS_YV411;
	enumMap["Y8"] = VideoInfo::CS_Y8;
	enumMap["YUV444P8"] = VideoInfo::CS_YV24;
	enumMap["YUV422P8"] = VideoInfo::CS_YV16;
	enumMap["YUV420P8"] = VideoInfo::CS_YV12;
	enumMap["YUV444P10"] = VideoInfo::CS_YUV444P10;
	enumMap["YUV422P10"] = VideoInfo::CS_YUV422P10;
	enumMap["YUV420P10"] = VideoInfo::CS_YUV420P10;
	enumMap["Y10"] = VideoInfo::CS_Y10;
	enumMap["YUV444P12"] = VideoInfo::CS_YUV444P12;
	enumMap["YUV422P12"] = VideoInfo::CS_YUV422P12;
	enumMap["YUV420P12"] = VideoInfo::CS_YUV420P12;
	enumMap["Y12"] = VideoInfo::CS_Y12;
	enumMap["YUV444P14"] = VideoInfo::CS_YUV444P14;
	enumMap["YUV422P14"] = VideoInfo::CS_YUV422P14;
	enumMap["YUV420P14"] = VideoInfo::CS_YUV420P14;
	enumMap["Y14"] = VideoInfo::CS_Y14;
	enumMap["YUV444P16"] = VideoInfo::CS_YUV444P16;
	enumMap["YUV422P16"] = VideoInfo::CS_YUV422P16;
	enumMap["YUV420P16"] = VideoInfo::CS_YUV420P16;
	enumMap["Y16"] = VideoInfo::CS_Y16;
	enumMap["YUV444PS"] = VideoInfo::CS_YUV444PS;
	enumMap["YUV422PS"] = VideoInfo::CS_YUV422PS;
	enumMap["YUV420PS"] = VideoInfo::CS_YUV420PS;
	enumMap["Y32"] = VideoInfo::CS_Y32;
	enumMap["RGB48"] = VideoInfo::CS_BGR48;
	enumMap["RGB64"] = VideoInfo::CS_BGR64;
	enumMap["RGBP"] = VideoInfo::CS_RGBP;
	enumMap["RGBP8"] = VideoInfo::CS_RGBP;
	enumMap["RGBP10"] = VideoInfo::CS_RGBP10;
	enumMap["RGBP12"] = VideoInfo::CS_RGBP12;
	enumMap["RGBP14"] = VideoInfo::CS_RGBP14;
	enumMap["RGBP16"] = VideoInfo::CS_RGBP16;
	enumMap["RGBPS"] = VideoInfo::CS_RGBPS;
	enumMap["RGBAP"] = VideoInfo::CS_RGBAP;
	enumMap["RGBAP8"] = VideoInfo::CS_RGBAP;
	enumMap["RGBAP10"] = VideoInfo::CS_RGBAP10;
	enumMap["RGBAP12"] = VideoInfo::CS_RGBAP12;
	enumMap["RGBAP14"] = VideoInfo::CS_RGBAP14;
	enumMap["RGBAP16"] = VideoInfo::CS_RGBAP16;
	enumMap["RGBAPS"] = VideoInfo::CS_RGBAPS;
	enumMap["YUVA444"] = VideoInfo::CS_YUVA444;
	enumMap["YUVA422"] = VideoInfo::CS_YUVA422;
	enumMap["YUVA420"] = VideoInfo::CS_YUVA420;
	enumMap["YUVA444P8"] = VideoInfo::CS_YUVA444;
	enumMap["YUVA422P8"] = VideoInfo::CS_YUVA422;
	enumMap["YUVA420P8"] = VideoInfo::CS_YUVA420;
	enumMap["YUVA444P10"] = VideoInfo::CS_YUVA444P10;
	enumMap["YUVA422P10"] = VideoInfo::CS_YUVA422P10;
	enumMap["YUVA420P10"] = VideoInfo::CS_YUVA420P10;
	enumMap["YUVA444P12"] = VideoInfo::CS_YUVA444P12;
	enumMap["YUVA422P12"] = VideoInfo::CS_YUVA422P12;
	enumMap["YUVA420P12"] = VideoInfo::CS_YUVA420P12;
	enumMap["YUVA444P14"] = VideoInfo::CS_YUVA444P14;
	enumMap["YUVA422P14"] = VideoInfo::CS_YUVA422P14;
	enumMap["YUVA420P14"] = VideoInfo::CS_YUVA420P14;
	enumMap["YUVA444P16"] = VideoInfo::CS_YUVA444P16;
	enumMap["YUVA422P16"] = VideoInfo::CS_YUVA422P16;
	enumMap["YUVA420P16"] = VideoInfo::CS_YUVA420P16;
	enumMap["YUVA444PS"] = VideoInfo::CS_YUVA444PS;
	enumMap["YUVA422PS"] = VideoInfo::CS_YUVA422PS;
	enumMap["YUVA420PS"] = VideoInfo::CS_YUVA420PS;
}

int PixelFormatParser::GetPixelFormatAsInt(std::string format) {
	my_map::const_iterator iValue = enumMap.find(format);
	if (iValue == enumMap.end())
		return 0;
	return iValue->second;
}

std::string PixelFormatParser::GetPixelFormatAsString(int pixel_type) {
	int key = 0;
	std::string Result = std::string("");
	for (auto &i : enumMap) {
		if (i.second == pixel_type) {
			Result = i.first;
			// If format ends with P8, continue searching to return the other name variant.
			if (i.first.length() < 3 || i.first.substr(i.first.length() - 2, 2) != std::string("P8"))
				break;
		}
	}
	return Result;
}

VideoInfo PixelFormatParser::GetVideoInfo(int pixel_type) {
	VideoInfo Result;
	Result.pixel_type = pixel_type;
	return Result;
}

VideoInfo PixelFormatParser::GetVideoInfo(std::string format) {
	VideoInfo Result;
	Result.pixel_type = GetPixelFormatAsInt(format);
	return Result;
}