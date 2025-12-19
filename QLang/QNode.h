#pragma once

#include "QActionNode.h"

// QNode - base class for all code block elements (statements, control flow,
// etc.)
class QNode : public QActionNode {
public:
  virtual ~QNode() = default;
};
