#pragma once
#include <memory>

template<typename T>
class Singleton
{
private:
    inline static std::unique_ptr<T> instance = nullptr;

public:
    Singleton() = delete;
    Singleton(Singleton const&) = delete;
    Singleton& operator=(Singleton const&) = delete;
    ~Singleton(){instance.release();}

    template<typename... Ts>
    static T* Instance(Ts... args)
    {
        if(instance == nullptr)
            CreateInstance(args...);

        return instance.get();
    }

private:
    template<typename... Ts>
    static void CreateInstance(Ts... args)
    {
        instance = std::make_unique<T>(args...);
    }
};
