# README

## 简介

这个项目用来做Vulkan学习的练习。目前分为以下几个子项目：

1. VulkanTutorial
> 这个子项目是[VulkanTutorial](https://vulkan-tutorial.com/)这个网站学习的结果，延伸做了一些IMGUI相关的显示。

2. LoadGLTF
> 这个子项目是[VulkanExample](https://github.com/SaschaWillems/Vulkan/tree/master)中LoadGLTF的学习和延伸：
> 
> 将VulkanExample中的一些能够复用的Helper Function封装成自己的
> 
> 将IMGUI Docking相关功能的代码集成进来
> 
> 将渲染结果渲染成Image然后显示成IMGUI中的图片
> 
> VulkanExample基础相机集成和增强

3. DefferedPBR
> 这个子项目实现了延迟渲染、PBR、SSAO、PCF-Shadow、Skybox、PostProcess以及简单的CPU动画，主要参考[VulkanExample](https://github.com/SaschaWillems/Vulkan/tree/master)和[LearnOpenGL](https://learnopengl.com/Getting-started/OpenGL)的相关章节和代码，其中SSAO和PCF-Shadow使用了Subpass的方式完成，其他都使用不同的Render Pass完成，这不是一种高效的方式，仅作为学习时使用。

## 编译

这个项目依赖cmake以及以下第三方库：

1. glfw3
2. VulkanSDK
3. Eigen3
4. ktx
5. IMGUI （有Docking功能的版本）
6. tinygltf

其中1，2，3需要自行安装，建议使用[vckpg](https://github.com/microsoft/vcpkg)然后设置根目录中`CMakeLists.txt`中`VCPKG_DIR`为vcpkg的所在目录，4，5，6均随项目有可编译的源码。

然后进入项目根目录：

```cmd
cd ToyVulkan
```

创建并进入build文件夹:

```cmd
mkdir build
cd build
```

使用cmake构建：

```cmd
cmake ..
```

创建成功后打开项目编译运行子项目即可。
