#pragma once

#include <iostream>

// Global debug flag - set to false to disable debug output
// To enable debug: set g_DebugEnabled = true; at start of main()
namespace QDebug {
static bool Enabled = false;
}

// Debug print macro - only outputs when debug is enabled
#define QDEBUG(msg)                                                            \
  if (QDebug::Enabled) {                                                       \
    std::cout << msg;                                                          \
  }
#define QDEBUG_LINE(msg)                                                       \
  if (QDebug::Enabled) {                                                       \
    std::cout << msg << std::endl;                                             \
  }
