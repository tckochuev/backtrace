#include <iostream>
#include <system_error>
#include <array>
#include <vector>
#include <memory>
#include <cstdlib>
#include <cassert>

#ifdef _WIN32
#include <windows.h>
#include <DbgHelp.h>
#endif

int backtrace(void** buffer, int maxDepth)
{
	#ifdef _WIN32
	return CaptureStackBackTrace(0, maxDepth, buffer, nullptr);
	#endif
}

template<typename T, typename Deleter = std::default_delete<T>>
class RecursiveDeleter
{
private:
	template<typename U>
	struct Dimension
	{
		static constexpr size_t value = 0;
	};

	template<typename U>
	struct Dimension<U*>
	{
		static constexpr size_t value = 1 + Dimension<U>::value;
	};

public:
	using Container = std::array<size_t, Dimension<T>::value>;

	RecursiveDeleter(Container dimensions, Deleter deleter = Deleter())
		: dimensions(std::move(dimensions)), deleter(std::move(deleter))
	{}
	void operator()(T* mem) const {
		doDelete(mem);
	}

	Container dimensions;
	Deleter deleter;

private:
	template<typename U>
	void doDelete(U* mem) const
	{
		constexpr size_t curDim = Dimension<U>::value;
		if constexpr(curDim == 0)
		{
			std::invoke(deleter, mem);
		}
		else
		{
			static_assert(std::tuple_size_v<Container> - curDim >= 0);
			static_assert(std::tuple_size_v<Container> - curDim < std::tuple_size_v<Container>);
			for(size_t i = 0; i < dimensions[dimensions.size() - curDim]; ++i)
			{
				doDelete(mem[i]);
			}
			std::invoke(deleter, mem);
		}
	}
};

std::unique_ptr<char*, RecursiveDeleter<char*, decltype(&free)>>
backtraceSymbols(void* const* buffer, int size, unsigned long maxNameLength = 256)
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

	std::unique_ptr<char*, RecursiveDeleter<char*, decltype(&free)>> output(
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
	#endif
}

int main()
{
	std::vector<void*> buffer(10);
	int captured = backtrace(buffer.data(), static_cast<int>(buffer.size()));
	buffer.erase(std::begin(buffer) + captured, std::end(buffer));
	auto symbs = backtraceSymbols(buffer.data(), std::size(buffer));
	for(int i = 0; i < std::size(buffer); ++i)
	{
		std::cout << symbs.get()[i] << std::endl;
	}

	return 0;
}