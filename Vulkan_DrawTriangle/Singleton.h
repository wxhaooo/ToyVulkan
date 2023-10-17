#pragma once
#include <utility>

template<typename T>
class Singleton
{
private:
	T instance = nullptr;
public:
	template<typename ...Args>
	static T Instance()
	{
		if(instance == nullptr)
			CreateInstance<Args>();

		return instance;
	}

private:
	template<typename ...Args>
	static void CreateInstance()
	{
		instance = new Singleton();;
	}
};
