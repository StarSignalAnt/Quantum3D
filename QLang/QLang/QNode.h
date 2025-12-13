#pragma once

#include "QAction.h"

// QNode - base class for all code block elements (statements, control flow,
// etc.)
class QNode : public QAction {
public:
  virtual ~QNode() = default;
};
