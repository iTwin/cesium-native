// This file was generated by generate-gltf-classes.
// DO NOT EDIT THIS FILE!
#pragma once

#include "ArrayJsonHandler.h"
#include "IntegerJsonHandler.h"
#include "NamedObjectJsonHandler.h"

namespace CesiumGltf {
  struct Skin;

  class SkinJsonHandler : public NamedObjectJsonHandler {
  public:
    void reset(IJsonHandler* pHandler, Skin* pObject);
    Skin* getObject();
    virtual void reportWarning(const std::string& warning, std::vector<std::string>&& context = std::vector<std::string>()) override;

    virtual IJsonHandler* Key(const char* str, size_t length, bool copy) override;

  protected:
    IJsonHandler* SkinKey(const char* str, Skin& o);

  private:

    Skin* _pObject;
    IntegerJsonHandler<int32_t> _inverseBindMatrices;
    IntegerJsonHandler<int32_t> _skeleton;
    ArrayJsonHandler<int32_t, IntegerJsonHandler<int32_t>> _joints;
  };
}