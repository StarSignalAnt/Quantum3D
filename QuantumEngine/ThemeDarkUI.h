#pragma once

#include "UITheme.h"

namespace Vivid {

class ThemeDarkUI : public UITheme {
public:
  ThemeDarkUI();
  ~ThemeDarkUI() override = default;

  void Init(VividDevice *device) override;
  const char *GetName() const override { return "DarkUI"; }
};

} // namespace Vivid
