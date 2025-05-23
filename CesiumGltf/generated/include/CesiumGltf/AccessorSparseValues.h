// This file was generated by generate-classes.
// DO NOT EDIT THIS FILE!
#pragma once

#include <CesiumGltf/Library.h>
#include <CesiumUtility/ExtensibleObject.h>

#include <cstdint>

namespace CesiumGltf {
/**
 * @brief An object pointing to a buffer view containing the deviating accessor
 * values. The number of elements is equal to `accessor.sparse.count` times
 * number of components. The elements have the same component type as the base
 * accessor. The elements are tightly packed. Data **MUST** be aligned following
 * the same rules as the base accessor.
 */
struct CESIUMGLTF_API AccessorSparseValues final
    : public CesiumUtility::ExtensibleObject {
  /**
   * @brief The original name of this type.
   */
  static constexpr const char* TypeName = "AccessorSparseValues";

  /**
   * @brief The index of the bufferView with sparse values. The referenced
   * buffer view **MUST NOT** have its `target` or `byteStride` properties
   * defined.
   */
  int32_t bufferView = -1;

  /**
   * @brief The offset relative to the start of the bufferView in bytes.
   */
  int64_t byteOffset = 0;

  /**
   * @brief Calculates the size in bytes of this object, including the contents
   * of all collections, pointers, and strings. This will NOT include the size
   * of any extensions attached to the object. Calling this method may be slow
   * as it requires traversing the object's entire structure.
   */
  int64_t getSizeBytes() const {
    int64_t accum = 0;
    accum += int64_t(sizeof(AccessorSparseValues));
    accum += CesiumUtility::ExtensibleObject::getSizeBytes() -
             int64_t(sizeof(CesiumUtility::ExtensibleObject));

    return accum;
  }
};
} // namespace CesiumGltf
