#pragma once

#include <iostream>
#include <string>

// Base class for all action nodes
class QAction {
public:
  virtual ~QAction() = default;
  virtual std::string GetName() const = 0;
  virtual void Print(int indent = 0) const = 0;

protected:
  void PrintIndent(int indent) const {
    for (int i = 0; i < indent; i++) {
      std::cout << "  ";
    }
  }
};
