// This file was generated by generate-classes.
// DO NOT EDIT THIS FILE!
#pragma once

#include "CesiumGltf/Library.h"
#include "CesiumGltf/TextureInfo.h"

#include <CesiumUtility/ExtensibleObject.h>

#include <optional>
#include <vector>

namespace CesiumGltf {
/**
 * @brief glTF extension that defines the specular material model.
 */
struct CESIUMGLTF_API ExtensionKhrMaterialsSpecular final
    : public CesiumUtility::ExtensibleObject {
  static inline constexpr const char* TypeName = "ExtensionKhrMaterialsSpecular";
  static inline constexpr const char* ExtensionName = "KHR_materials_specular";

  /**
   * @brief The strength of the specular reflection.
   *
   * This parameter scales the amount of specular reflection on non-metallic surfaces.
   * It has no effect on metals.
   */
  double specularFactor = 1;

  /**
   * @brief A texture that defines the specular factor in the alpha channel.
   *
   * A texture that defines the specular factor in the alpha channel. This will be
   * multiplied by specularFactor.
   */
  std::optional<CesiumGltf::TextureInfo> specularTexture;

  /**
   * @brief The F0 RGB color of the specular reflection.
   *
   * This is an additional RGB color parameter that tints the specular reflection of
   * non-metallic surfaces. At grazing angles, the reflection still blends to white,
   * and the parameter has not effect on metals. The value is linear.
   */
  std::vector<double> specularColorFactor = {1, 1, 1};

  /**
   * @brief A texture that defines the F0 color of the specular reflection.
   *
   * A texture that defines the specular color in the RGB channels (encoded in sRGB).
   * This will be multiplied by specularColorFactor.
   */
  std::optional<CesiumGltf::TextureInfo> specularColorTexture;
};
} // namespace CesiumGltf