#if defined(_WIN32) || defined(__unix__)
#include <array>
#include <memory>
#include <functional>

namespace tc 
{

template<typename T, typename Deleter = std::default_delete<T>>
class MultiDimArrayDeleter
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

	MultiDimArrayDeleter(Container dimensions, Deleter deleter = Deleter())
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

int backtrace(void** buffer, int maxDepth);

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
	unsigned long maxNameLength = 256
);

}
#else
#error "Supported only on windows and unix"
#endif