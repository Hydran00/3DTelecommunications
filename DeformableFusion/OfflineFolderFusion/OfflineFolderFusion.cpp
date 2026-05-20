// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "Peabody.h"

#include "CameraView.h"
#include "CMotionFieldEstimationF2F.h"
#include "Fusion4DGPUKeyFrames.h"
#include "FusionConfig.h"

#include <cuda_runtime.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

int DEPTH_CAMERAS_NUM = 1;
int FrameCountKeyVolume = 1;

namespace fs = std::filesystem;

namespace
{
struct Options
{
	std::string configPath;
	fs::path inputDir;
	fs::path depthDir;
	fs::path rgbDir;
	fs::path outputDir = "offline_fusion_output";
	std::string depthToken;
	std::string rgbToken;
	std::string calibrationPath;
	int cameraCount = 1;
	int gpu = 0;
	double fx = 525.0;
	double fy = 525.0;
	double cx = -1.0;
	double cy = -1.0;
	BoundingBox3D bbox = BoundingBox3D(-100, 100, -200, 0, -100, 100);
};

bool has_image_extension(const fs::path& path)
{
	const auto ext = path.extension().string();
	std::string lower;
	lower.reserve(ext.size());
	for (char c : ext) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
	return lower == ".png" || lower == ".tif" || lower == ".tiff" || lower == ".bmp" ||
		lower == ".jpg" || lower == ".jpeg" || lower == ".exr";
}

std::string lower_string(std::string text)
{
	for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return text;
}

bool natural_less(const fs::path& lhs, const fs::path& rhs)
{
	const auto a = lower_string(lhs.filename().string());
	const auto b = lower_string(rhs.filename().string());
	size_t ia = 0;
	size_t ib = 0;
	while (ia < a.size() && ib < b.size())
	{
		if (std::isdigit(static_cast<unsigned char>(a[ia])) &&
			std::isdigit(static_cast<unsigned char>(b[ib])))
		{
			size_t ja = ia;
			size_t jb = ib;
			while (ja < a.size() && std::isdigit(static_cast<unsigned char>(a[ja]))) ++ja;
			while (jb < b.size() && std::isdigit(static_cast<unsigned char>(b[jb]))) ++jb;
			const auto na = std::stoll(a.substr(ia, ja - ia));
			const auto nb = std::stoll(b.substr(ib, jb - ib));
			if (na != nb) return na < nb;
			ia = ja;
			ib = jb;
			continue;
		}
		if (a[ia] != b[ib]) return a[ia] < b[ib];
		++ia;
		++ib;
	}
	return a.size() < b.size();
}

std::vector<fs::path> list_images(const fs::path& dir, const std::string& token)
{
	if (dir.empty() || !fs::is_directory(dir))
	{
		throw std::runtime_error("Directory not found: " + dir.string());
	}

	const auto tokenLower = lower_string(token);
	std::vector<fs::path> files;
	for (const auto& entry : fs::directory_iterator(dir))
	{
		if (!entry.is_regular_file() || !has_image_extension(entry.path())) continue;
		const auto filename = lower_string(entry.path().filename().string());
		if (!tokenLower.empty() && filename.find(tokenLower) == std::string::npos) continue;
		files.push_back(entry.path());
	}
	std::sort(files.begin(), files.end(), natural_less);
	return files;
}

cv::Mat load_depth(const fs::path& path)
{
	cv::Mat depth = cv::imread(path.string(), cv::IMREAD_UNCHANGED);
	if (depth.empty()) throw std::runtime_error("Cannot read depth image: " + path.string());
	if (depth.channels() != 1) throw std::runtime_error("Depth image must be single-channel: " + path.string());
	if (depth.type() == CV_16UC1) return depth;

	cv::Mat converted;
	if (depth.type() == CV_32FC1)
	{
		depth.convertTo(converted, CV_16UC1);
		return converted;
	}
	if (depth.type() == CV_8UC1)
	{
		depth.convertTo(converted, CV_16UC1, 256.0);
		return converted;
	}

	throw std::runtime_error("Unsupported depth image type in: " + path.string());
}

cv::Mat load_color(const fs::path& path, int width, int height)
{
	cv::Mat color = cv::imread(path.string(), cv::IMREAD_COLOR);
	if (color.empty()) throw std::runtime_error("Cannot read color image: " + path.string());
	if (color.cols != width || color.rows != height)
	{
		cv::resize(color, color, cv::Size(width, height), 0.0, 0.0, cv::INTER_LINEAR);
	}
	return color;
}

std::vector<cv::Mat> load_depth_frame(const std::vector<fs::path>& files, int frame, int cameras)
{
	std::vector<cv::Mat> frameDepths;
	frameDepths.reserve(cameras);
	const int base = frame * cameras;
	for (int camera = 0; camera < cameras; ++camera)
	{
		frameDepths.push_back(load_depth(files[base + camera]));
	}
	return frameDepths;
}

std::vector<cv::Mat> load_color_frame(const std::vector<fs::path>& files, int frame, int cameras, int width, int height)
{
	std::vector<cv::Mat> frameColors;
	frameColors.reserve(cameras);
	const int base = frame * cameras;
	for (int camera = 0; camera < cameras; ++camera)
	{
		frameColors.push_back(load_color(files[base + camera], width, height));
	}
	return frameColors;
}

std::vector<double> parse_double_list(const std::string& text)
{
	std::vector<double> values;
	std::string current;
	for (char c : text)
	{
		if (c == ',' || c == ';')
		{
			if (!current.empty()) values.push_back(std::stod(current));
			current.clear();
		}
		else
		{
			current.push_back(c);
		}
	}
	if (!current.empty()) values.push_back(std::stod(current));
	return values;
}

void print_usage()
{
	std::cout
		<< "Usage:\n"
		<< "  OfflineFolderFusion.exe --depth-dir C:\\data\\depth --output C:\\data\\out [options]\n\n"
		<< "Options:\n"
		<< "  --input DIR              Folder containing depth/rgb files; depth files are filtered by --depth-token.\n"
		<< "  --depth-dir DIR          Folder containing depth images. Overrides --input for depth.\n"
		<< "  --rgb-dir DIR            Optional folder containing RGB images. Overrides --input for RGB.\n"
		<< "  --depth-token TEXT       Filename substring for depth files when using --input.\n"
		<< "  --rgb-token TEXT         Filename substring for RGB files when using --input. Default: rgb.\n"
		<< "  --camera-count N         Number of depth images per frame. Default: 1.\n"
		<< "  --config FILE            Optional DeformableFusion cfg file.\n"
		<< "  --calib FILE             Optional calibCameras3DTM.json file.\n"
		<< "  --gpu N                  CUDA device id. Default: 0.\n"
		<< "  --fx V --fy V --cx V --cy V  Intrinsics fallback when --calib is absent.\n"
		<< "  --bbox xmin,xmax,ymin,ymax,zmin,zmax  Volume bounds in cm.\n";
}

std::string require_value(int argc, char** argv, int& i)
{
	if (i + 1 >= argc) throw std::runtime_error(std::string("Missing value for ") + argv[i]);
	return argv[++i];
}

Options parse_options(int argc, char** argv)
{
	Options opt;
	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		if (arg == "--help" || arg == "-h")
		{
			print_usage();
			std::exit(0);
		}
		else if (arg == "--config") opt.configPath = require_value(argc, argv, i);
		else if (arg == "--input") opt.inputDir = require_value(argc, argv, i);
		else if (arg == "--depth-dir") opt.depthDir = require_value(argc, argv, i);
		else if (arg == "--rgb-dir") opt.rgbDir = require_value(argc, argv, i);
		else if (arg == "--output") opt.outputDir = require_value(argc, argv, i);
		else if (arg == "--depth-token") opt.depthToken = require_value(argc, argv, i);
		else if (arg == "--rgb-token") opt.rgbToken = require_value(argc, argv, i);
		else if (arg == "--calib") opt.calibrationPath = require_value(argc, argv, i);
		else if (arg == "--camera-count") opt.cameraCount = std::stoi(require_value(argc, argv, i));
		else if (arg == "--gpu") opt.gpu = std::stoi(require_value(argc, argv, i));
		else if (arg == "--fx") opt.fx = std::stod(require_value(argc, argv, i));
		else if (arg == "--fy") opt.fy = std::stod(require_value(argc, argv, i));
		else if (arg == "--cx") opt.cx = std::stod(require_value(argc, argv, i));
		else if (arg == "--cy") opt.cy = std::stod(require_value(argc, argv, i));
		else if (arg == "--bbox")
		{
			const auto values = parse_double_list(require_value(argc, argv, i));
			if (values.size() != 6) throw std::runtime_error("--bbox expects 6 comma-separated values");
			opt.bbox = BoundingBox3D(values[0], values[1], values[2], values[3], values[4], values[5]);
		}
		else
		{
			throw std::runtime_error("Unknown option: " + arg);
		}
	}

	if (opt.depthDir.empty()) opt.depthDir = opt.inputDir;
	if (opt.rgbDir.empty()) opt.rgbDir = opt.inputDir;
	if (opt.depthDir.empty()) throw std::runtime_error("Pass --depth-dir or --input");
	if (opt.cameraCount < 1) throw std::runtime_error("--camera-count must be >= 1");
	if (opt.depthToken.empty() && !opt.inputDir.empty() && opt.depthDir == opt.inputDir) opt.depthToken = "depth";
	if (opt.rgbToken.empty()) opt.rgbToken = "rgb";
	return opt;
}

std::vector<std::unique_ptr<GCameraView>> create_cameras(const Options& opt, int width, int height)
{
	std::vector<std::unique_ptr<GCameraView>> cameras;
	cameras.reserve(opt.cameraCount);
	for (int camera = 0; camera < opt.cameraCount; ++camera)
	{
		auto view = std::make_unique<GCameraView>(height, width);
		if (!opt.calibrationPath.empty())
		{
			if (!view->LoadCalibrationFrom3DTMFormat(opt.calibrationPath.c_str(), camera, "depth", -1.0))
			{
				throw std::runtime_error("Cannot load depth calibration camera " + std::to_string(camera));
			}
		}
		else
		{
			const double cx = opt.cx >= 0.0 ? opt.cx : width * 0.5;
			const double cy = opt.cy >= 0.0 ? opt.cy : height * 0.5;
			const double k[9] = { opt.fx, 0.0, cx, 0.0, opt.fy, cy, 0.0, 0.0, 1.0 };
			const double r[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
			const double t[3] = { 0.0, 0.0, 0.0 };
			view->SetCalibrationMatrix(k, r, t, nullptr);
		}
		cameras.push_back(std::move(view));
	}
	return cameras;
}

std::vector<GCameraView*> raw_camera_ptrs(std::vector<std::unique_ptr<GCameraView>>& cameras)
{
	std::vector<GCameraView*> raw;
	raw.reserve(cameras.size());
	for (auto& camera : cameras) raw.push_back(camera.get());
	return raw;
}

template <typename FusionT>
void apply_common_config(FusionT& fusion, CConfig* conf, const char* prefix)
{
	if (!conf) return;
	conf->SetErrorIfNameNotFound(false);
	fusion.align_mu = conf->GetValueWithDefault<float>("Fusion", (std::string(prefix) + "_align_mu").c_str(), fusion.align_mu);
	fusion.ed_nodes_res = conf->GetValueWithDefault<float>("Fusion", (std::string(prefix) + "_ed_nodes_res").c_str(), fusion.ed_nodes_res);
	fusion.vxl_res = conf->GetValueWithDefault<float>("Fusion", (std::string(prefix) + "_vxl_res").c_str(), fusion.vxl_res);
	fusion.ed_nodes_res_low = conf->GetValueWithDefault<float>("Fusion", (std::string(prefix) + "_ed_nodes_res_low").c_str(), fusion.ed_nodes_res_low);
	fusion.vxl_res_low = conf->GetValueWithDefault<float>("Fusion", (std::string(prefix) + "_vxl_res_low").c_str(), fusion.vxl_res_low);
	fusion.fusion_mu = conf->GetValueWithDefault<float>("Fusion", (std::string(prefix) + "_fusion_mu").c_str(), fusion.fusion_mu);
	fusion.iso_surface_level = conf->GetValueWithDefault<float>("Fusion", (std::string(prefix) + "_iso_surface_level").c_str(), fusion.iso_surface_level);
	fusion.bBilateralFilter = conf->GetValueWithDefault<bool>("Fusion", (std::string(prefix) + "_bBilateralFilter").c_str(), fusion.bBilateralFilter);
	fusion.bUseDepthTopBitAsSeg = conf->GetValueWithDefault<bool>("Fusion", (std::string(prefix) + "_bUseDepthTopBitAsSeg").c_str(), fusion.bUseDepthTopBitAsSeg);
}

void feed_optional_color(
	Fusion4DKeyFrames& fusion4d,
	CMotionFieldEstimationF2F& f2f,
	const std::vector<fs::path>& colorFiles,
	int frame,
	int cameras,
	int width,
	int height)
{
	if (colorFiles.empty()) return;
	auto colorImages = load_color_frame(colorFiles, frame, cameras, width, height);
	fusion4d.vol_fusion.feed_color_textures(colorImages);
	f2f.vol_fusion.feed_color_textures(colorImages);
}
}

int main(int argc, char** argv)
{
	try
	{
		const Options opt = parse_options(argc, argv);
		DEPTH_CAMERAS_NUM = opt.cameraCount;

		if (!opt.configPath.empty())
		{
			if (!FusionConfig::initialize_config(opt.configPath))
			{
				return 2;
			}
			auto conf = FusionConfig::current();
			if (conf)
			{
				DEPTH_CAMERAS_NUM = conf->GetValueWithDefault<int>("DepthGeneration", "DepthCameraCount", DEPTH_CAMERAS_NUM);
				FrameCountKeyVolume = conf->GetValueWithDefault<int>("Fusion", "FrameCountKeyVolume", FrameCountKeyVolume);
			}
		}

		if (DEPTH_CAMERAS_NUM != opt.cameraCount)
		{
			throw std::runtime_error("Config DepthCameraCount differs from --camera-count");
		}

		cudaError_t cudaStatus = cudaSetDevice(opt.gpu);
		if (cudaStatus != cudaSuccess)
		{
			throw std::runtime_error(std::string("cudaSetDevice failed: ") + cudaGetErrorString(cudaStatus));
		}

		fs::create_directories(opt.outputDir);
		const auto depthFiles = list_images(opt.depthDir, opt.depthToken);
		const auto colorFiles = opt.rgbDir.empty() ? std::vector<fs::path>() : list_images(opt.rgbDir, opt.rgbToken);
		if (depthFiles.empty()) throw std::runtime_error("No depth images found");
		if (depthFiles.size() % opt.cameraCount != 0)
		{
			throw std::runtime_error("Depth image count is not divisible by --camera-count");
		}

		const int frameCount = static_cast<int>(depthFiles.size() / opt.cameraCount);
		const bool hasColor = colorFiles.size() >= depthFiles.size();
		if (!colorFiles.empty() && !hasColor)
		{
			std::cerr << "RGB files ignored: expected at least " << depthFiles.size() << ", found " << colorFiles.size() << "\n";
		}

		auto firstDepth = load_depth_frame(depthFiles, 0, opt.cameraCount);
		const int depthWidth = firstDepth.front().cols;
		const int depthHeight = firstDepth.front().rows;
		for (const auto& depth : firstDepth)
		{
			if (depth.cols != depthWidth || depth.rows != depthHeight)
			{
				throw std::runtime_error("All depth images in a frame must have the same size");
			}
		}

		auto cameras = create_cameras(opt, depthWidth, depthHeight);
		auto cameraPtrs = raw_camera_ptrs(cameras);

		Fusion4DKeyFrames fusion4d(nullptr, opt.cameraCount, depthWidth, depthHeight, FrameCountKeyVolume);
		CMotionFieldEstimationF2F f2f(nullptr, opt.cameraCount, depthWidth, depthHeight, FrameCountKeyVolume);
		fusion4d.setup_cameras(cameraPtrs);
		f2f.setup_cameras(cameraPtrs);

		auto conf = FusionConfig::current();
		apply_common_config(fusion4d, conf, "fusion4d");
		apply_common_config(f2f, conf, "f2f_motion");

		const std::string out = opt.outputDir.string();
		const char* outDir = out.empty() ? nullptr : out.c_str();

		std::cout << "Processing " << frameCount << " frame(s), " << opt.cameraCount
			<< " camera(s), " << depthWidth << "x" << depthHeight << ", GPU " << opt.gpu << "\n";

		if (hasColor) feed_optional_color(fusion4d, f2f, colorFiles, 0, opt.cameraCount, depthWidth, depthHeight);
		fusion4d.set_up_1st_frame(firstDepth, opt.bbox, outDir, 0);
		f2f.set_up_1st_frame(firstDepth, opt.bbox, outDir, 0);

		for (int frame = 1; frame < frameCount; ++frame)
		{
			auto depths = load_depth_frame(depthFiles, frame, opt.cameraCount);
			if (hasColor) feed_optional_color(fusion4d, f2f, colorFiles, frame, opt.cameraCount, depthWidth, depthHeight);

			std::cout << "Frame " << frame << "/" << (frameCount - 1) << "\n";
			f2f.add_a_frame(depths, outDir, frame, false);
			EDNodesParasGPU edInit = f2f.ed_nodes_for_init();
			fusion4d.add_a_frame(depths, &edInit, outDir, frame, false);
			cudaDeviceSynchronize();
		}

		std::cout << "Done. Debug/surface outputs written to " << opt.outputDir << "\n";
		return 0;
	}
	catch (const std::exception& ex)
	{
		std::cerr << "OfflineFolderFusion error: " << ex.what() << "\n\n";
		print_usage();
		return 1;
	}
}
