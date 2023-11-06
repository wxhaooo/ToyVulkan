#include <VulkanHelper.h>
#include <fstream>

namespace vks
{
    namespace helper
    {
        bool errorModeSilent = false;

        bool FileExists(const std::string &filename)
        {
            std::ifstream f(filename.c_str());
            return !f.fail();
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
    }    
}