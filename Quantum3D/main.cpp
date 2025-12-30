#include "Quantum3D.h"
#include "stdafx.h"

#include <QApplication>
#include <QIcon>
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
  app.setWindowIcon(QIcon(":/Quantum3D/icons/Q3Icon.png"));

  QApplication::setStyle(QStyleFactory::create("Fusion"));

  // Optional: improve contrast and modern appearance

  QPalette darkPalette;
  darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
  darkPalette.setColor(QPalette::WindowText, Qt::white);
  darkPalette.setColor(QPalette::Base, QColor(42, 42, 42));
  darkPalette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
  darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
  darkPalette.setColor(QPalette::ToolTipText, Qt::white);
  darkPalette.setColor(QPalette::Text, Qt::white);
  darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
  darkPalette.setColor(QPalette::ButtonText, Qt::white);
  darkPalette.setColor(QPalette::BrightText, Qt::red);
  darkPalette.setColor(QPalette::Highlight, QColor(90, 90, 90));
  darkPalette.setColor(QPalette::HighlightedText, Qt::white);

  app.setPalette(darkPalette);
  Quantum3D window;
  window.showMaximized();
  return app.exec();
}
