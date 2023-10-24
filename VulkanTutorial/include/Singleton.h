#pragma once
#include <utility>

template<typename T>
class Singleton
{
private:
	inline static std::unique_ptr<T> instance = nullptr;

public:
	static T* Instance()
	{
		if(instance == nullptr)
			CreateInstance();

		return instance.get();
	}

private:
	static void CreateInstance()
	{
		instance = std::make_unique<T>();
	}
};
