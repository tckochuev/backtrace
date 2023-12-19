#include <backtrace.h>

#include <system_error>
#include <cstdlib>
#include <cassert>

#ifdef _WIN32
#include <windows.h>
#include <DbgHelp.h>
#else
#include <execinfo.h>
#endif

namespace  tc
{

int backtrace(void** buffer, int maxDepth)
{
	#ifdef _WIN32
	return CaptureStackBackTrace(0, maxDepth, buffer, nullptr);
	#else
	return ::backtrace(buffer, maxDepth);
	#endif
}

std::unique_ptr<
	char*, 
	#ifdef _WIN32
	MultiDimArrayDeleter<char*, decltype(&free)>
	#else
	decltype(&free)
	#endif
>
backtraceSymbols(
	void* const* buffer, 
	int size, 
	#ifdef __unix__
	[[maybe_unused]]
	#endif
	unsigned long maxNameLength
)
{
	#ifdef _WIN32
	HANDLE process = GetCurrentProcess();
	if(!SymInitialize(process, nullptr, TRUE))
	{
		throw std::system_error(GetLastError(), std::system_category());
	}

	std::unique_ptr<SYMBOL_INFO> symbInfo(
		reinterpret_cast<SYMBOL_INFO*>(new std::byte[sizeof(SYMBOL_INFO) + (maxNameLength - 1) * sizeof(TCHAR)])
	);
	symbInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
	symbInfo->MaxNameLen = maxNameLength;

	std::unique_ptr<char*, MultiDimArrayDeleter<char*, decltype(&free)>> output(
		static_cast<char**>(calloc(size, sizeof(char*))),
		{{static_cast<size_t>(size)}, &free}
	);
	if(!output)
	{
		throw std::bad_alloc();
	}

	char** rawOutput = output.get();
	for(int i = 0; i < size; ++i)
	{
		if(SymFromAddr(process, reinterpret_cast<DWORD64>(buffer[i]), nullptr, symbInfo.get()))
		{
			assert(symbInfo->Name);
			assert(symbInfo->NameLen <= maxNameLength);
			rawOutput[i] = static_cast<char*>(calloc(symbInfo->NameLen + 1, sizeof(char)));
			if(!rawOutput[i])
			{
				throw std::bad_alloc();
			}
			std::copy(symbInfo->Name, symbInfo->Name + symbInfo->NameLen, rawOutput[i]);
			rawOutput[i][symbInfo->NameLen] = 0;
		}
		else
		{
			throw std::system_error(GetLastError(), std::system_category());
		}
	}
	return output;
	#else
	std::unique_ptr<char*, decltype(&free)> output(
		backtrace_symbols(buffer, size),
		&free
	);
	if(!output)
	{
		throw std::bad_alloc();
	}
	else
	{
		return output;
	}
	#endif
}

}//namespace tc