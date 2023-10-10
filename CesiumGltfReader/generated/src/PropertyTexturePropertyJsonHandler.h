// This file was generated by generate-classes.
// DO NOT EDIT THIS FILE!
#pragma once

#include "TextureInfoJsonHandler.h"

#include <CesiumGltf/PropertyTextureProperty.h>
#include <CesiumJsonReader/ArrayJsonHandler.h>
#include <CesiumJsonReader/IntegerJsonHandler.h>
#include <CesiumJsonReader/JsonObjectJsonHandler.h>

namespace CesiumJsonReader {
class JsonReaderOptions;
}

namespace CesiumGltfReader {
class PropertyTexturePropertyJsonHandler : public TextureInfoJsonHandler {
public:
  using ValueType = CesiumGltf::PropertyTextureProperty;

  PropertyTexturePropertyJsonHandler(
      const CesiumJsonReader::JsonReaderOptions& options) noexcept;
  void reset(
      IJsonHandler* pParentHandler,
      CesiumGltf::PropertyTextureProperty* pObject);

  virtual IJsonHandler* readObjectKey(const std::string_view& str) override;

protected:
  IJsonHandler* readObjectKeyPropertyTextureProperty(
      const std::string& objectType,
      const std::string_view& str,
      CesiumGltf::PropertyTextureProperty& o);

private:
  CesiumGltf::PropertyTextureProperty* _pObject = nullptr;
  CesiumJsonReader::
      ArrayJsonHandler<int64_t, CesiumJsonReader::IntegerJsonHandler<int64_t>>
          _channels;
  CesiumJsonReader::JsonObjectJsonHandler _offset;
  CesiumJsonReader::JsonObjectJsonHandler _scale;
  CesiumJsonReader::JsonObjectJsonHandler _max;
  CesiumJsonReader::JsonObjectJsonHandler _min;
};
} // namespace CesiumGltfReader