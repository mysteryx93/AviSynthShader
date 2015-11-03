#pragma once
#include "d3d9.h"
#include "atlbase.h"
#include "D3D9Macros.h"
#include "avisynth.h"
#include <windows.h>
#include <mutex>

/// Calling AviSynth functions from other threads causes conflicts, so we must query the info before sending to the execution thread.
struct FrameRef {
	FrameRef::FrameRef() {
		Width = 0, Height = 0, Pitch = 0, Buffer = NULL;
	}

	FrameRef::FrameRef(PVideoFrame clip, byte* buffer, int precision) {
		Width = clip->GetRowSize() / precision / 4;
		Height = clip->GetHeight();
		Pitch = clip->GetPitch();
		Buffer = buffer;
	}

	int Width, Height, Pitch;
	byte* Buffer;
};

/// Contains the pixel shader command to execute.
struct CommandStruct {
	CommandStruct() : Path(NULL), Lock(new std::mutex()) {}
	~CommandStruct() { /*delete Lock;*/ }

	const char* Path;
	const char* EntryPoint;
	const char* ShaderModel;
	const char* Param[9];
	FrameRef Input[9];
	FrameRef Output;
	HANDLE Event;
	std::mutex* Lock;
	CommandStruct* OriginalCopy; 

	bool IsNull() {
		return (!Path);
	}

	/// Performs a deep copy of strings and video buffers for multithreading.
	CommandStruct DeepCopy() {
		// Although there should be only one thread using this data at once, we allocate the memory space within a lock 
		// so that the memory gets allocated with the right flags for multithreading.
		std::lock_guard<std::mutex> lock(*Lock);
		CommandStruct Result;
		Result.Path = CopyString(Path);
		Result.EntryPoint = CopyString(EntryPoint);
		Result.ShaderModel = CopyString(ShaderModel);
		for (int i = 0; i < 9; i++) {
			Result.Param[i] = CopyString(Param[i]);
			Result.Input[i] = Input[i];
			Result.Input[i].Buffer = CopyBuffer(&Input[i]);
		}
		Result.Output = Output;
		Result.Output.Buffer = CopyBuffer(&Output);
		Result.Event = Event;
		Result.OriginalCopy = this;
		return Result;
	}

	/// Releases the memory allocated by deep copy and copies the output back into the original copy.
	CommandStruct Release() {
		std::lock_guard<std::mutex> lock(*Lock);

		if (Output.Buffer && OriginalCopy && OriginalCopy->Output.Buffer)
			memcpy(OriginalCopy->Output.Buffer, Output.Buffer, Output.Pitch * Output.Height);

		SafeDelete<const char*>(Path);
		SafeDelete<const char*>(EntryPoint);
		SafeDelete<const char*>(ShaderModel);
		for (int i = 0; i < 9; i++) {
			SafeDelete<const char*>(Param[i]);
			if (Input[i].Buffer)
				SafeDelete<byte*>(Input[i].Buffer);
		}
		if (Output.Buffer)
			SafeDelete<byte*>(Output.Buffer);

		return *OriginalCopy;
	}

private:
	char* CopyString(const char* copyFrom) {
		char* Result = NULL;
		if (copyFrom) {
			int Length = strlen(copyFrom);
			Result = (char*)malloc(Length + 1);
			strcpy(Result, copyFrom);
		}
		return Result;
	}

	byte* CopyBuffer(const FrameRef* copyFrom) {
		byte* Result = NULL;
		if (copyFrom && copyFrom->Buffer) {
			int Length = copyFrom->Pitch * copyFrom->Height;
			Result = (byte*)malloc(Length);
			memcpy(Result, copyFrom->Buffer, Length);
		}
		return Result;
	}

	template <typename T>
	inline void SafeDelete(T& p)
	{
		if (p) {
			free((void*)p);
			p = NULL;
		}
	}
};