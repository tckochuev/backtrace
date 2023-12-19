#include <iostream>
#include <backtrace.h>

void printTrace()
{
	std::vector<void*> buffer(10);
	int captured = tc::backtrace(buffer.data(), static_cast<int>(buffer.size()));
	buffer.erase(std::begin(buffer) + captured, std::end(buffer));
	std::unique_ptr<char*, std::function<void(void*)>> symbs = tc::backtraceSymbols(buffer.data(), std::size(buffer));
	for(int i = 0; i < std::size(buffer); ++i)
	{
		std::cout << symbs.get()[i] << std::endl;
	}
}

int main()
{
	printTrace();
	return 0;
}