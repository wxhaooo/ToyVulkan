#include<DeferredPBR.h>
#include <Singleton.hpp>
#include <GraphicSettings.hpp>

#if _DEBUG
bool enableValidation = true;
#else
bool enableValidation = false;

#endif

int main()
{
	Singleton<GraphicSettings>::Instance()->validation = enableValidation;
	std::unique_ptr<DeferredPBR> deferredPBRApp = std::make_unique<DeferredPBR>();
	deferredPBRApp->InitFondation();
	deferredPBRApp->InitVulkan();
	deferredPBRApp->Prepare();
	deferredPBRApp->RenderLoop();
	return 0;
}