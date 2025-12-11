#include "Quantum3D.h"
#include "stdafx.h"

#include <QLoggingCategory>

int main(int argc, char *argv[]) {
  // Enable Vulkan backend for Qt's RHI (Rendering Hardware Interface)
  qputenv("QSG_RHI_BACKEND", "vulkan");
  qputenv("QT_QUICK_BACKEND", "rhi");

  // Optional: Enable Vulkan validation layers for debugging
  // qputenv("QT_VULKAN_INSTANCE_EXTENSIONS", "VK_LAYER_KHRONOS_validation");

  // Ensure High DPI scaling is enabled (standard for Qt 6, but explicit doesn't
  // hurt)
  QApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

  QApplication app(argc, argv);

  Quantum3D window;
  window.showMaximized();
  return app.exec();
}
