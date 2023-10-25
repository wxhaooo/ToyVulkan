#include <VulkanHelper.h>

namespace vks
{
    namespace Helper
    {
        bool errorModeSilent = false;
        
        bool IsExtensionAvailable(const std::vector<VkExtensionProperties>& properties, const char* extension)
        {
            for (const VkExtensionProperties& p : properties)
                if (strcmp(p.extensionName, extension) == 0)
                    return true;
            return false;
        }

        void exitFatal(const std::string& message, int32_t exitCode)
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

        void exitFatal(const std::string& message, VkResult resultCode)
        {
            exitFatal(message, (int32_t)resultCode);
        }
    }    
}