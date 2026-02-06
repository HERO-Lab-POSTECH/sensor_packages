/**
 * @file sonar_config.cpp
 * @brief Implementation of sonar configuration registry
 * @date 260206
 */

#include "oculus_sonar/sonar_config.hpp"
#include <opencv2/opencv.hpp>
#include <algorithm>

namespace oculus_sonar {

//==============================================================================
// Sonar Model Configurations
//==============================================================================
// Reference: Blueprint Subsea Oculus M-Series Datasheet
// https://www.blueprintsubsea.com/oculus/oculus-m-series

const std::unordered_map<std::string, SonarModelConfig>
SonarConfigRegistry::models_ = {
    // M750d: 750kHz / 1.2MHz dual frequency
    {"m750d", {
        "m750d",
        // Low freq (750kHz): 130° FOV, 120m range, 4mm resolution
        {130.0f, 20.0f, 0.75f, 120.0f, SonarConstants::M750D_LOW_FREQ_RES_MM},
        // High freq (1.2MHz): 130° FOV, 40m range, 2.5mm resolution
        {130.0f, 20.0f, 1.2f, 40.0f, SonarConstants::M750D_HIGH_FREQ_RES_MM},
        SonarConstants::DEFAULT_MAX_BEAMS
    }},

    // M1200d: 1.2MHz / 2.1MHz dual frequency
    {"m1200d", {
        "m1200d",
        // Low freq (1.2MHz): 130° FOV, 40m range, 2.5mm resolution
        {130.0f, 20.0f, 1.2f, 40.0f, SonarConstants::M1200D_LOW_FREQ_RES_MM},
        // High freq (2.1MHz): 60° FOV, 10m range, 1.5mm resolution
        {60.0f, 12.0f, 2.1f, 10.0f, SonarConstants::M1200D_HIGH_FREQ_RES_MM},
        SonarConstants::DEFAULT_MAX_BEAMS
    }},

    // M3000d: 1.2MHz / 3.0MHz dual frequency (highest resolution)
    {"m3000d", {
        "m3000d",
        // Low freq (1.2MHz): 130° FOV, 30m range, 2.5mm resolution
        {130.0f, 20.0f, 1.2f, 30.0f, SonarConstants::M3000D_LOW_FREQ_RES_MM},
        // High freq (3.0MHz): 40° FOV, 5m range, 2mm resolution
        {40.0f, 12.0f, 3.0f, 5.0f, SonarConstants::M3000D_HIGH_FREQ_RES_MM},
        SonarConstants::DEFAULT_MAX_BEAMS
    }}
};

//==============================================================================
// OpenCV Colormap Configurations
//==============================================================================
// Reference: https://docs.opencv.org/4.x/d3/d50/group__imgproc__colormap.html

const std::unordered_map<std::string, ColormapConfig>
SonarConfigRegistry::colormaps_ = {
    // Classic colormaps
    {"autumn",   {"autumn",   cv::COLORMAP_AUTUMN}},    // 0: Red-yellow
    {"bone",     {"bone",     cv::COLORMAP_BONE}},      // 1: Grayscale with blue tint
    {"jet",      {"jet",      cv::COLORMAP_JET}},       // 2: Blue-cyan-yellow-red (classic)
    {"winter",   {"winter",   cv::COLORMAP_WINTER}},    // 3: Blue-green
    {"rainbow",  {"rainbow",  cv::COLORMAP_RAINBOW}},   // 4: Full spectrum
    {"ocean",    {"ocean",    cv::COLORMAP_OCEAN}},     // 5: Blue shades (good for underwater)
    {"summer",   {"summer",   cv::COLORMAP_SUMMER}},    // 6: Green-yellow
    {"spring",   {"spring",   cv::COLORMAP_SPRING}},    // 7: Magenta-yellow
    {"cool",     {"cool",     cv::COLORMAP_COOL}},      // 8: Cyan-magenta
    {"hsv",      {"hsv",      cv::COLORMAP_HSV}},       // 9: HSV color space
    {"pink",     {"pink",     cv::COLORMAP_PINK}},      // 10: Pink shades
    {"hot",      {"hot",      cv::COLORMAP_HOT}},       // 11: Black-red-yellow-white (thermal)

    // Perceptually uniform colormaps (recommended for scientific visualization)
    {"parula",   {"parula",   cv::COLORMAP_PARULA}},    // 12: Blue-green-yellow
    {"magma",    {"magma",    cv::COLORMAP_MAGMA}},     // 13: Black-purple-orange-yellow
    {"inferno",  {"inferno",  cv::COLORMAP_INFERNO}},   // 14: Black-purple-orange-yellow
    {"plasma",   {"plasma",   cv::COLORMAP_PLASMA}},    // 15: Blue-purple-orange-yellow
    {"viridis",  {"viridis",  cv::COLORMAP_VIRIDIS}},   // 16: Blue-green-yellow (colorblind-safe)
    {"cividis",  {"cividis",  cv::COLORMAP_CIVIDIS}},   // 17: Blue-yellow (colorblind-safe)
    {"twilight", {"twilight", cv::COLORMAP_TWILIGHT}},  // 18: Cyclic purple-orange
    {"turbo",    {"turbo",    cv::COLORMAP_TURBO}},     // 19: Improved rainbow (recommended)

    // Additional colormaps
    {"deepgreen", {"deepgreen", cv::COLORMAP_DEEPGREEN}} // 21: Green shades
};

// Reverse lookup: OpenCV code -> colormap name
const std::unordered_map<int, std::string>
SonarConfigRegistry::colormap_code_to_name_ = {
    {cv::COLORMAP_AUTUMN,    "autumn"},
    {cv::COLORMAP_BONE,      "bone"},
    {cv::COLORMAP_JET,       "jet"},
    {cv::COLORMAP_WINTER,    "winter"},
    {cv::COLORMAP_RAINBOW,   "rainbow"},
    {cv::COLORMAP_OCEAN,     "ocean"},
    {cv::COLORMAP_SUMMER,    "summer"},
    {cv::COLORMAP_SPRING,    "spring"},
    {cv::COLORMAP_COOL,      "cool"},
    {cv::COLORMAP_HSV,       "hsv"},
    {cv::COLORMAP_PINK,      "pink"},
    {cv::COLORMAP_HOT,       "hot"},
    {cv::COLORMAP_PARULA,    "parula"},
    {cv::COLORMAP_MAGMA,     "magma"},
    {cv::COLORMAP_INFERNO,   "inferno"},
    {cv::COLORMAP_PLASMA,    "plasma"},
    {cv::COLORMAP_VIRIDIS,   "viridis"},
    {cv::COLORMAP_CIVIDIS,   "cividis"},
    {cv::COLORMAP_TWILIGHT,  "twilight"},
    {cv::COLORMAP_TURBO,     "turbo"},
    {cv::COLORMAP_DEEPGREEN, "deepgreen"}
};

//==============================================================================
// Registry Implementation
//==============================================================================

const SonarModelConfig& SonarConfigRegistry::getModelConfig(const std::string& model) {
    auto it = models_.find(model);
    if (it == models_.end()) {
        throw std::runtime_error(
            "Unknown sonar model: '" + model + "'. "
            "Supported models: m750d, m1200d, m3000d");
    }
    return it->second;
}

const ColormapConfig& SonarConfigRegistry::getColormapConfig(const std::string& name) {
    auto it = colormaps_.find(name);
    if (it == colormaps_.end()) {
        throw std::runtime_error(
            "Unknown colormap: '" + name + "'. "
            "Use one of: jet, hot, viridis, turbo, plasma, magma, inferno, "
            "ocean, rainbow, bone, autumn, winter, summer, spring, cool, "
            "hsv, pink, parula, cividis, twilight, deepgreen");
    }
    return it->second;
}

const ColormapConfig& SonarConfigRegistry::getColormapByCode(int code) {
    auto it = colormap_code_to_name_.find(code);
    if (it == colormap_code_to_name_.end()) {
        throw std::runtime_error(
            "Unknown colormap code: " + std::to_string(code));
    }
    return getColormapConfig(it->second);
}

std::vector<std::string> SonarConfigRegistry::getSupportedModels() {
    std::vector<std::string> models;
    models.reserve(models_.size());
    for (const auto& pair : models_) {
        models.push_back(pair.first);
    }
    std::sort(models.begin(), models.end());
    return models;
}

std::vector<std::string> SonarConfigRegistry::getSupportedColormaps() {
    std::vector<std::string> colormaps;
    colormaps.reserve(colormaps_.size());
    for (const auto& pair : colormaps_) {
        colormaps.push_back(pair.first);
    }
    std::sort(colormaps.begin(), colormaps.end());
    return colormaps;
}

bool SonarConfigRegistry::isModelSupported(const std::string& model) {
    return models_.find(model) != models_.end();
}

bool SonarConfigRegistry::isColormapSupported(const std::string& name) {
    return colormaps_.find(name) != colormaps_.end();
}

}  // namespace oculus_sonar
