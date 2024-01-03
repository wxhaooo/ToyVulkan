#include <filesystem>
#include <fstream>
#include <VulkanHelper.h>

namespace vks
{
    namespace helper
    {
        bool errorModeSilent = false;

        bool FileExists(const std::string &filename)
        {
#if _HAS_CXX17
            return std::filesystem::exists(filename);
#else
            std::ifstream f(filename.c_str());
            return !f.fail();
#endif
        }

        void ExitFatal(const std::string& message, int32_t exitCode)
        {
#if defined(_WIN32)
            if (!errorModeSilent) {
                MessageBox(NULL, message.c_str(), NULL, MB_OK | MB_ICONERROR);
            }
#elif defined(__ANDROID__)
            LOGE("Fatal error: %s", message.c_str());
        vks::android::showAlert(message.c_str());
#endif
        std::cerr << message << "\n";
#if !defined(__ANDROID__)
        exit(exitCode);
#endif
        }

        void ExitFatal(const std::string& message, VkResult resultCode)
        {
            ExitFatal(message, (int32_t)resultCode);
        }

    	// iOS & macOS: VulkanExampleBase::getAssetPath() implemented externally to allow access to Objective-C components
    	const std::string GetAssetPath()
        {
            std::string workingPath = std::filesystem::current_path().string();
        	return workingPath + "/assets/";
        }
		
    	const std::string GetShaderBasePath()
        {
            std::string workingPath = std::filesystem::current_path().string();
        	return workingPath + "/shaders/";
        }

        const std::string GetFileExtension(std::string filename)
        {
            return 	std::filesystem::path(filename).extension().string();
        }

        // template<typename ... Args>
        // std::string StringFormat( const std::string& format, Args ... args )
        // {
        //     int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
        //     if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
        //     auto size = static_cast<size_t>( size_s );
        //     std::unique_ptr<char[]> buf( new char[ size ] );
        //     std::snprintf( buf.get(), size, format.c_str(), args ... );
        //     return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
        // }
    }    
}
