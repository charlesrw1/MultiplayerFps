#include "DrawLocal.h"
#ifdef EDITOR_BUILD
#pragma comment(lib, "ole32.lib")
#undef APIENTRY
#include <DirectXTex.h>
#undef max
#undef min
#endif
#include "Framework/StringUtils.h"

// Save cubemap-array (rgb16f native) to DDS as RGB32F (uncompressed). The
// driver up-converts during readback when the texture is rgb16f.
bool SaveCubeArrayToDDS(IGraphicsTexture* texture, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t cubeCount,
						const char* filename) {
	const uint32_t facesPerCube = 6;
	const uint32_t arraySize = cubeCount * facesPerCube;
	using namespace DirectX;
	// Setup DDS metadata
	TexMetadata metadata = {};
	metadata.width = width;
	metadata.height = height;
	metadata.depth = 1;
	metadata.arraySize = arraySize;
	metadata.mipLevels = mipLevels;
	metadata.format = DXGI_FORMAT_R32G32B32_FLOAT;
	metadata.dimension = TEX_DIMENSION_TEXTURE2D;
	metadata.miscFlags = TEX_MISC_TEXTURECUBE;

	ScratchImage image;
	HRESULT hr = image.Initialize(metadata);
	if (FAILED(hr))
		return false;

	for (uint32_t mip = 0; mip < mipLevels; ++mip) {
		uint32_t mipWidth = std::max(1u, width >> mip);
		uint32_t mipHeight = std::max(1u, height >> mip);

		size_t pixelCount = mipWidth * mipHeight;
		size_t dataSize = pixelCount * 3 * sizeof(float);

		std::vector<float> buffer(pixelCount * 3);

		for (uint32_t layer = 0; layer < arraySize; ++layer) {
			// Native rgb16f → download maps to (RGB, FLOAT); upcasted to float.
			texture->download((int)mip, (int)layer, buffer.data(), (int)dataSize);

			const Image* img = image.GetImage(mip, layer, 0);
			memcpy(img->pixels, buffer.data(), dataSize);
		}
	}
	std::wstring wfilename(filename, filename + strlen(filename));
	hr = SaveToDDSFile(image.GetImages(), image.GetImageCount(), metadata, DDS_FLAGS_FORCE_DX10_EXT, wfilename.c_str());

	std::string parentDir = filename;
	auto findSlash = parentDir.rfind('/');
	if (findSlash != std::string::npos)
		parentDir = parentDir.substr(0, findSlash + 1);

	const std::string pathToNvidiaTextureConvertTool = "./x64/Debug/texconv.exe";

	const std::string format = "BC6H_UF16";
	std::string commandLine = pathToNvidiaTextureConvertTool + " -f ";
	commandLine += format;
	commandLine += " -y -o ";
	commandLine += StringUtils::get_directory(filename);
	commandLine += " " + std::string(filename);

	STARTUPINFOA startup = {};
	PROCESS_INFORMATION out = {};

	if (!CreateProcessA(nullptr, (char*)commandLine.c_str(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup,
						&out)) {
		sys_print(Error, "couldn't create process\n");
		return false;
	}
	WaitForSingleObject(out.hProcess, INFINITE);
	CloseHandle(out.hProcess);
	CloseHandle(out.hThread);

	return SUCCEEDED(hr);
}

bool save_float_texture_as_dds(IGraphicsTexture* texture, uint32_t width, uint32_t height, int mode, const char* filename) {

	using namespace DirectX;
	// Setup DDS metadata
	TexMetadata metadata = {};
	metadata.width = width;
	metadata.height = height;
	metadata.depth = 1;
	metadata.arraySize = 1;
	metadata.mipLevels = 1;
	metadata.format = DXGI_FORMAT_R32G32B32_FLOAT;
	metadata.dimension = TEX_DIMENSION_TEXTURE2D;
	metadata.miscFlags = 0;

	ScratchImage image;
	HRESULT hr = image.Initialize(metadata);
	if (FAILED(hr))
		return false;

	// Download native-format pixels then pack into the DDS 3-channel float
	// scratch image. Native is r11f_g11f_b10f (probe_irradiance) or rg16f
	// (probe_depth). Both round-trip through float in the IGraphicsTexture
	// download path.
	const GraphicsTextureFormat src_fmt = texture->get_texture_format();
	const int src_channels =
		(src_fmt == GraphicsTextureFormat::rg16f || src_fmt == GraphicsTextureFormat::rg32f) ? 2 : 3;

	for (uint32_t mip = 0; mip < 1; ++mip) {
		uint32_t mipWidth = std::max(1u, width >> mip);
		uint32_t mipHeight = std::max(1u, height >> mip);

		size_t pixelCount = mipWidth * mipHeight;
		size_t dstSize = pixelCount * 3 * sizeof(float);

		std::vector<float> src(pixelCount * src_channels);
		texture->download((int)mip, -1, src.data(), (int)(src.size() * sizeof(float)));

		for (uint32_t layer = 0; layer < 1; ++layer) {
			const Image* img = image.GetImage(mip, layer, 0);
			float* dst = reinterpret_cast<float*>(img->pixels);
			for (size_t i = 0; i < pixelCount; ++i) {
				dst[i * 3 + 0] = src[i * src_channels + 0];
				dst[i * 3 + 1] = src_channels >= 2 ? src[i * src_channels + 1] : 0.f;
				dst[i * 3 + 2] = src_channels >= 3 ? src[i * src_channels + 2] : 0.f;
			}
			(void)dstSize;
		}
	}

	ScratchImage dstImage;

	DXGI_FORMAT out_fmt{};
	if (mode == 0)
		out_fmt = DXGI_FORMAT_R16G16_FLOAT;
	else if (mode == 1)
		out_fmt = DXGI_FORMAT_R11G11B10_FLOAT;

	hr = Convert(*image.GetImage(0, 0, 0), // source image
				 out_fmt,				   // target format
				 TEX_FILTER_DEFAULT,	   // optional filter
				 TEX_THRESHOLD_DEFAULT,	   // optional threshold
				 dstImage				   // output
	);

	std::wstring wfilename(filename, filename + strlen(filename));

	hr = SaveToDDSFile(dstImage.GetImages(), dstImage.GetImageCount(), dstImage.GetMetadata(), DDS_FLAGS_FORCE_DX10_EXT,
					   wfilename.c_str());

	std::string parentDir = filename;
	auto findSlash = parentDir.rfind('/');
	if (findSlash != std::string::npos)
		parentDir = parentDir.substr(0, findSlash + 1);

	const std::string pathToNvidiaTextureConvertTool = "./x64/Debug/texconv.exe";

	const std::string format = "BC6H_UF16";
	std::string commandLine = pathToNvidiaTextureConvertTool + " -f ";
	commandLine += format;
	commandLine += " -y -dx10 -o ";
	commandLine += StringUtils::get_directory(filename);
	commandLine += " " + std::string(filename);

	STARTUPINFOA startup = {};
	PROCESS_INFORMATION out = {};

	if (!CreateProcessA(nullptr, (char*)commandLine.c_str(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup,
						&out)) {
		sys_print(Error, "couldn't create process\n");
		return false;
	}
	WaitForSingleObject(out.hProcess, INFINITE);
	CloseHandle(out.hProcess);
	CloseHandle(out.hThread);

	return SUCCEEDED(hr);

	return SUCCEEDED(hr);
}