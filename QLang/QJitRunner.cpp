#include "QJitRunner.h"
#include "QAssign.h"
#include "QClass.h"
#include "QCode.h"
#include "QError.h"
#include "QExpression.h"
#include "QFor.h"
#include "QIf.h"
#include "QInstanceDecl.h"
#include "QJitProgram.h"
#include "QMemberAssign.h"
#include "QMethod.h"
#include "QModuleFile.h"

#include "QMethodCall.h"
#include "QReturn.h"

#include "Parser.h"
#include "QLVM.h"
#include "QLVMContext.h"
#include "QProgram.h"
#include "QStatement.h"
#include "QStaticRegistry.h"
#include "QVariableDecl.h"
#include "Tokenizer.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>

QJitRunner::QJitRunner(std::shared_ptr<QLVMContext> lvmContext,
                       std::shared_ptr<QErrorCollector> errorCollector)
    : m_LVMContext(lvmContext), m_ErrorCollector(errorCollector) {
#if QLANG_DEBUG
  std::cout << "[DEBUG] QJitRunner created" << std::endl;
#endif
}

QJitRunner::~QJitRunner() {}

// ============================================================================
// Type Mapping
// ============================================================================

llvm::Type *QJitRunner::GetLLVMType(int tokenType,
                                    const std::string &typeName) {
  auto &context = QLVM::GetContext();
  TokenType tt = static_cast<TokenType>(tokenType);

  switch (tt) {
  case TokenType::T_INT32:
    return llvm::Type::getInt32Ty(context);
  case TokenType::T_INT64:
    return llvm::Type::getInt64Ty(context);
  case TokenType::T_FLOAT32:
    return llvm::Type::getFloatTy(context);
  case TokenType::T_FLOAT64:
    return llvm::Type::getDoubleTy(context);
  case TokenType::T_BOOL:
    return llvm::Type::getInt1Ty(context);
  case TokenType::T_SHORT:
    return llvm::Type::getInt16Ty(context);
  case TokenType::T_CPTR:
    return llvm::PointerType::getUnqual(context); // void* pointer
  case TokenType::T_STRING_TYPE:
    return llvm::PointerType::getUnqual(context);
  case TokenType::T_IDENTIFIER:
    // This could be a class type - look it up
    if (!typeName.empty()) {
      auto classIt = m_CompiledClasses.find(typeName);
      if (classIt != m_CompiledClasses.end()) {
        // Return pointer to the struct type for class return values
        return llvm::PointerType::getUnqual(context);
      }
    }
    std::cerr << "[ERROR] QJitRunner: Unknown class type: " << typeName
              << std::endl;
    return nullptr;
  default:
    // Also try typename lookup for any other token type
    if (!typeName.empty()) {
      auto classIt = m_CompiledClasses.find(typeName);
      if (classIt != m_CompiledClasses.end()) {
        return llvm::PointerType::getUnqual(context);
      }
    }
    std::cerr << "[ERROR] QJitRunner: Unknown type token: " << tokenType
              << " (typeName: " << typeName << ")" << std::endl;
    return nullptr;
  }
}

// ============================================================================
// Expression Compilation - Recursive Parser
// ============================================================================

llvm::Value *QJitRunner::CompilePrimaryExpr(const std::vector<Token> &tokens,
                                            size_t &pos,
                                            llvm::Type *expectedType,
                                            std::string *outClassName) {
  auto &builder = QLVM::GetBuilder();

  if (pos >= tokens.size()) {
    std::cerr << "[ERROR] QJitRunner: Unexpected end of expression"
              << std::endl;
    return nullptr;
  }

  const Token &token = tokens[pos];

  // Handle parenthesized sub-expressions
  if (token.type == TokenType::T_LPAREN) {
    pos++; // consume '('
    llvm::Value *result = CompileExprTokens(tokens, pos, expectedType);
    if (pos < tokens.size() && tokens[pos].type == TokenType::T_RPAREN) {
      pos++; // consume ')'
    } else {
      std::cerr << "[ERROR] QJitRunner: Missing closing parenthesis"
                << std::endl;
    }
    return result;
  }

  // Handle literals and identifiers
  pos++; // consume the token

  switch (token.type) {
  case TokenType::T_INTEGER: {
    int64_t value = std::stoll(token.value);
    if (expectedType && expectedType->isFloatingPointTy()) {
      return llvm::ConstantFP::get(expectedType, static_cast<double>(value));
    }
    llvm::Type *intType = expectedType ? expectedType : builder.getInt32Ty();
    return llvm::ConstantInt::get(intType, value);
  }

  case TokenType::T_FLOAT: {
    double value = std::stod(token.value);
    if (expectedType && expectedType->isIntegerTy()) {
      return llvm::ConstantInt::get(expectedType, static_cast<int64_t>(value));
    }
    llvm::Type *floatType = expectedType ? expectedType : builder.getDoubleTy();
    return llvm::ConstantFP::get(floatType, value);
  }

  case TokenType::T_STRING:
    return builder.CreateGlobalStringPtr(token.value);

  case TokenType::T_TRUE:
    return llvm::ConstantInt::getTrue(QLVM::GetContext());

  case TokenType::T_FALSE:
    return llvm::ConstantInt::getFalse(QLVM::GetContext());

  case TokenType::T_NEW: {
    if (pos >= tokens.size() || tokens[pos].type != TokenType::T_IDENTIFIER) {
      std::cerr << "[ERROR] QJitRunner: Expected class name after 'new'"
                << std::endl;
      return nullptr;
    }
    std::string className = tokens[pos].value;
    pos++; // consume class name

    // Consume parentheses if any
    if (pos < tokens.size() && tokens[pos].type == TokenType::T_LPAREN) {
      pos++; // (
      int depth = 1;
      while (pos < tokens.size() && depth > 0) {
        if (tokens[pos].type == TokenType::T_LPAREN)
          depth++;
        else if (tokens[pos].type == TokenType::T_RPAREN)
          depth--;
        pos++;
      }
    }

    auto classIt = m_CompiledClasses.find(className);
    if (classIt == m_CompiledClasses.end()) {
      std::cerr << "[ERROR] QJitRunner: Unknown class: " << className
                << std::endl;
      return nullptr;
    }

    // Allocate memory on heap using malloc and call constructor
    llvm::Function *mallocFunc = m_LVMContext->GetLLVMFunc("malloc");
    if (!mallocFunc) {
      // Fallback: try to find or declare in current module
      mallocFunc = QLVM::GetModule()->getFunction("malloc");
      if (!mallocFunc) {
        std::vector<llvm::Type *> args = {
            llvm::Type::getInt64Ty(QLVM::GetContext())};
        llvm::FunctionType *mallocType = llvm::FunctionType::get(
            llvm::PointerType::getUnqual(QLVM::GetContext()), args, false);
        mallocFunc =
            llvm::Function::Create(mallocType, llvm::Function::ExternalLinkage,
                                   "malloc", QLVM::GetModule());
      }
    }

    if (!mallocFunc) {
      std::cerr << "[ERROR] QJitRunner: malloc not found" << std::endl;
      return nullptr;
    }

    uint64_t size = QLVM::GetModule()->getDataLayout().getTypeAllocSize(
        classIt->second.structType);
    llvm::Value *sizeVal = llvm::ConstantInt::get(
        llvm::Type::getInt64Ty(QLVM::GetContext()), size);

    llvm::Value *mallocPtr =
        builder.CreateCall(mallocFunc, {sizeVal}, "new." + className + ".heap");

    // Call constructor
    auto methIt = classIt->second.methods.find(className);
    if (methIt != classIt->second.methods.end()) {
      builder.CreateCall(methIt->second, {mallocPtr});
    }

    if (outClassName)
      *outClassName = className;

    return mallocPtr;
  }

  case TokenType::T_IDENTIFIER: {
    std::string varName = token.value;

    // Check for method call on 'this' (e.g., GetNumber())
    if (pos < tokens.size() && tokens[pos].type == TokenType::T_LPAREN) {
      auto methodCall = std::make_shared<QMethodCall>("this", varName);
      pos++; // consume '('

      auto args = std::make_shared<QParameters>();
      if (pos < tokens.size() && tokens[pos].type != TokenType::T_RPAREN) {
        while (pos < tokens.size()) {
          auto expr = std::make_shared<QExpression>();
          int depth = 0;
          while (pos < tokens.size()) {
            if (tokens[pos].type == TokenType::T_LPAREN)
              depth++;
            else if (tokens[pos].type == TokenType::T_RPAREN) {
              if (depth == 0)
                break;
              depth--;
            } else if (tokens[pos].type == TokenType::T_COMMA && depth == 0) {
              break;
            }
            expr->AddElement(tokens[pos]);
            pos++;
          }
          args->AddParameter(expr);
          if (pos < tokens.size() && tokens[pos].type == TokenType::T_COMMA) {
            pos++; // consume ','
          } else {
            break;
          }
        }
      }

      if (pos < tokens.size() && tokens[pos].type == TokenType::T_RPAREN) {
        pos++; // consume ')'
      }

      methodCall->SetArguments(args);
      if (outClassName && !m_CurrentClassName.empty()) {
        auto classIt = m_CompiledClasses.find(m_CurrentClassName);
        if (classIt != m_CompiledClasses.end()) {
          auto retIt = classIt->second.methodReturnTypes.find(varName);
          if (retIt != classIt->second.methodReturnTypes.end()) {
            *outClassName = retIt->second;
          }
        }
      }

      return CompileMethodCall(methodCall);
    }

    // Check for member access or method call (instance.member or
    // instance.method())
    if (pos < tokens.size() && tokens[pos].type == TokenType::T_DOT) {
      pos++; // consume '.'

      if (pos >= tokens.size() || tokens[pos].type != TokenType::T_IDENTIFIER) {
        std::cerr << "[ERROR] QJitRunner: Expected member name after '.'"
                  << std::endl;
        return nullptr;
      }

      std::string memberName = tokens[pos].value;
      pos++; // consume member name

      // Check if this is a method call (instance.method())
      if (pos < tokens.size() && tokens[pos].type == TokenType::T_LPAREN) {
        auto methodCall = std::make_shared<QMethodCall>(varName, memberName);
        pos++; // consume '('

        auto args = std::make_shared<QParameters>();
        if (pos < tokens.size() && tokens[pos].type != TokenType::T_RPAREN) {
          while (pos < tokens.size()) {
            auto expr = std::make_shared<QExpression>();
            int depth = 0;
            while (pos < tokens.size()) {
              if (tokens[pos].type == TokenType::T_LPAREN)
                depth++;
              else if (tokens[pos].type == TokenType::T_RPAREN) {
                if (depth == 0)
                  break;
                depth--;
              } else if (tokens[pos].type == TokenType::T_COMMA && depth == 0) {
                break;
              }
              expr->AddElement(tokens[pos]);
              pos++;
            }
            args->AddParameter(expr);
            if (pos < tokens.size() && tokens[pos].type == TokenType::T_COMMA) {
              pos++; // consume ','
            } else {
              break;
            }
          }
        }

        if (pos < tokens.size() && tokens[pos].type == TokenType::T_RPAREN) {
          pos++; // consume ')'
        }

        methodCall->SetArguments(args);
        if (outClassName) {
          auto typeIt = m_VariableTypes.find(varName);
          if (typeIt != m_VariableTypes.end()) {
            auto classIt = m_CompiledClasses.find(typeIt->second);
            if (classIt != m_CompiledClasses.end()) {
              auto retIt = classIt->second.methodReturnTypes.find(memberName);
              if (retIt != classIt->second.methodReturnTypes.end()) {
                *outClassName = retIt->second;
              }
            }
          }
        }
        return CompileMethodCall(methodCall);
      }

      // Check if varName is a STATIC class (not an instance variable)
      auto staticClassIt = m_CompiledClasses.find(varName);
      if (staticClassIt != m_CompiledClasses.end() &&
          staticClassIt->second.isStatic) {
        // This is a static class - get global instance from registry
        CompiledClass &classInfo = staticClassIt->second;
        int memberIdx = FindMemberIndex(classInfo, memberName);
        if (memberIdx < 0) {
          std::cerr << "[ERROR] QJitRunner: Member '" << memberName
                    << "' not found in static class '" << varName << "'"
                    << std::endl;
          return nullptr;
        }

        // Get static instance pointer from registry
        void *staticPtr = QStaticRegistry::Instance().GetInstance(varName);
        if (!staticPtr) {
          std::cerr << "[ERROR] QJitRunner: Static instance for '" << varName
                    << "' not found in registry" << std::endl;
          return nullptr;
        }

        // Create constant pointer to static instance
        llvm::Value *instancePtr = llvm::ConstantInt::get(
            builder.getInt64Ty(), reinterpret_cast<uint64_t>(staticPtr));
        instancePtr = builder.CreateIntToPtr(instancePtr, builder.getPtrTy(),
                                             varName + ".static.ptr");

        llvm::Value *memberPtr = builder.CreateStructGEP(
            classInfo.structType, instancePtr, static_cast<unsigned>(memberIdx),
            varName + "." + memberName + ".ptr");

        return builder.CreateLoad(classInfo.memberTypes[memberIdx], memberPtr,
                                  varName + "." + memberName);
      }

      // Look up instance (regular variable)
      auto varIt = m_LocalVariables.find(varName);
      if (varIt == m_LocalVariables.end()) {
        std::cerr << "[ERROR] QJitRunner: Undefined variable: " << varName
                  << std::endl;
        return nullptr;
      }

      // Look up class type for this instance
      auto typeIt = m_VariableTypes.find(varName);
      if (typeIt == m_VariableTypes.end()) {
        std::cerr << "[ERROR] QJitRunner: Variable '" << varName
                  << "' is not a class instance" << std::endl;
        return nullptr;
      }

      std::string className = typeIt->second;
      auto classIt = m_CompiledClasses.find(className);
      if (classIt == m_CompiledClasses.end()) {
        std::cerr << "[ERROR] QJitRunner: Class '" << className << "' not found"
                  << std::endl;
        return nullptr;
      }

      CompiledClass &classInfo = classIt->second;
      int memberIdx = FindMemberIndex(classInfo, memberName);
      if (memberIdx < 0) {
        std::cerr << "[ERROR] QJitRunner: Member '" << memberName
                  << "' not found in class '" << className << "'" << std::endl;
        return nullptr;
      }

      // GEP to member and load
      llvm::AllocaInst *alloca = varIt->second;
      llvm::Value *instancePtr = alloca;

      // Class instances are always stored as pointers on the stack, so we must
      // load the pointer We know it's a class instance because we checked
      // m_VariableTypes above
      instancePtr =
          builder.CreateLoad(builder.getPtrTy(), alloca, varName + ".ptr");

      llvm::Value *memberPtr = builder.CreateStructGEP(
          classInfo.structType, instancePtr, static_cast<unsigned>(memberIdx),
          varName + "." + memberName + ".ptr");

      return builder.CreateLoad(classInfo.memberTypes[memberIdx], memberPtr,
                                varName + "." + memberName);
    }

    // Regular variable access
    auto it = m_LocalVariables.find(varName);
    if (it != m_LocalVariables.end()) {
      // Check if this is a class instance - if so, return the pointer directly
      // (don't load the struct value)
      auto typeIt = m_VariableTypes.find(varName);
      if (typeIt != m_VariableTypes.end()) {
        // It's a class instance - return the pointer
        if (outClassName)
          *outClassName = typeIt->second;
        return it->second;
      }
      // Primitive type - load the value
      return builder.CreateLoad(it->second->getAllocatedType(), it->second,
                                varName);
    } else {
      // Try implicit member access (this.varName)
      if (m_CurrentInstance && !m_CurrentClassName.empty()) {
        auto classIt = m_CompiledClasses.find(m_CurrentClassName);
        if (classIt != m_CompiledClasses.end()) {
          CompiledClass &classInfo = classIt->second;
          int memberIdx = FindMemberIndex(classInfo, varName);

          if (memberIdx >= 0) {
            llvm::Value *memberPtr = builder.CreateStructGEP(
                classInfo.structType, m_CurrentInstance,
                static_cast<unsigned>(memberIdx), "this." + varName + ".ptr");

            return builder.CreateLoad(classInfo.memberTypes[memberIdx],
                                      memberPtr, "this." + varName);
          }
        }
      }

      std::cerr << "[ERROR] QJitRunner: Undefined variable: " << varName
                << std::endl;
      return nullptr;
    }
  }

  default:
    std::cerr << "[ERROR] QJitRunner: Unexpected token in expression: "
              << token.value << " (type " << static_cast<int>(token.type) << ")"
              << std::endl;
    return nullptr;
  }
}

llvm::Value *QJitRunner::ApplyBinaryOp(const std::string &op, llvm::Value *left,
                                       llvm::Value *right) {
  auto &builder = QLVM::GetBuilder();

  if (!left || !right)
    return nullptr;

  // Promote if one is float and other is int
  if (left->getType()->isFloatingPointTy() && right->getType()->isIntegerTy()) {
    right = builder.CreateSIToFP(right, left->getType(), "promotetmp");
  } else if (right->getType()->isFloatingPointTy() &&
             left->getType()->isIntegerTy()) {
    left = builder.CreateSIToFP(left, right->getType(), "promotetmp");
  }

  bool isFloat = left->getType()->isFloatingPointTy() ||
                 right->getType()->isFloatingPointTy();

  // Arithmetic operators
  if (op == "+") {
    // Check for string concatenation
    if (left->getType()->isPointerTy() && right->getType()->isPointerTy()) {
      // In opaque pointers, we can't check element type.
      // Assuming all pointer additions are string concatenations for now.
      llvm::Function *concatFunc = m_LVMContext->GetLLVMFunc("string_concat");
      if (concatFunc) {
        return builder.CreateCall(concatFunc, {left, right}, "str_add_tmp");
      } else {
        std::cerr
            << "[ERROR] QJitRunner: string_concat runtime function not found"
            << std::endl;
      }
    }

    return isFloat ? builder.CreateFAdd(left, right, "addtmp")
                   : builder.CreateAdd(left, right, "addtmp");
  } else if (op == "-") {
    return isFloat ? builder.CreateFSub(left, right, "subtmp")
                   : builder.CreateSub(left, right, "subtmp");
  } else if (op == "*") {
    return isFloat ? builder.CreateFMul(left, right, "multmp")
                   : builder.CreateMul(left, right, "multmp");
  } else if (op == "/") {
    return isFloat ? builder.CreateFDiv(left, right, "divtmp")
                   : builder.CreateSDiv(left, right, "divtmp");
  } else if (op == "%") {
    if (isFloat) {
      std::cerr << "[ERROR] QJitRunner: Modulo not supported for floats"
                << std::endl;
      return nullptr;
    }
    return builder.CreateSRem(left, right, "modtmp");
  }

  // Comparison operators
  else if (op == ">") {
    return isFloat ? builder.CreateFCmpOGT(left, right, "gttmp")
                   : builder.CreateICmpSGT(left, right, "gttmp");
  } else if (op == "<") {
    return isFloat ? builder.CreateFCmpOLT(left, right, "lttmp")
                   : builder.CreateICmpSLT(left, right, "lttmp");
  } else if (op == ">=") {
    return isFloat ? builder.CreateFCmpOGE(left, right, "getmp")
                   : builder.CreateICmpSGE(left, right, "getmp");
  } else if (op == "<=") {
    return isFloat ? builder.CreateFCmpOLE(left, right, "letmp")
                   : builder.CreateICmpSLE(left, right, "letmp");
  } else if (op == "==" || op == "=") {
    return isFloat ? builder.CreateFCmpOEQ(left, right, "eqtmp")
                   : builder.CreateICmpEQ(left, right, "eqtmp");
  } else if (op == "!=" || op == "<>") {
    return isFloat ? builder.CreateFCmpONE(left, right, "netmp")
                   : builder.CreateICmpNE(left, right, "netmp");
  }
  std::cerr << "[ERROR] QJitRunner: Unknown operator: " << op << std::endl;
  return nullptr;
}

int QJitRunner::GetOperatorPrecedence(const std::string &op) {
  if (op == "*" || op == "/" || op == "%")
    return 20;
  if (op == "+" || op == "-")
    return 10;
  if (op == ">" || op == "<" || op == ">=" || op == "<=" || op == "==" ||
      op == "!=" || op == "=" || op == "<>")
    return 5;
  return 0;
}

llvm::Value *QJitRunner::CompileExprTokens(const std::vector<Token> &tokens,
                                           size_t &pos,
                                           llvm::Type *expectedType,
                                           std::string *outClassName) {
  // Start with precedence 0
  return CompileExprTokensRecursive(tokens, pos, expectedType, 0, outClassName);
}

// Internal version that supports precedence climbing
llvm::Value *QJitRunner::CompileExprTokensRecursive(
    const std::vector<Token> &tokens, size_t &pos, llvm::Type *expectedType,
    int minPrecedence, std::string *outClassName) {

  std::string leftClassName;
  llvm::Value *result =
      CompilePrimaryExpr(tokens, pos, expectedType, &leftClassName);
  if (!result)
    return nullptr;

  if (outClassName)
    *outClassName = leftClassName;

  while (pos < tokens.size()) {
    const Token &opToken = tokens[pos];

    if (opToken.type == TokenType::T_RPAREN ||
        opToken.type == TokenType::T_COMMA) {
      break;
    }

    // Handle operators - T_OPERATOR, T_GREATER (>), T_LESS (<)
    std::string op;
    if (opToken.type == TokenType::T_OPERATOR) {
      op = opToken.value;
    } else if (opToken.type == TokenType::T_GREATER) {
      op = ">";
    } else if (opToken.type == TokenType::T_LESS) {
      op = "<";
    } else {
      break; // Not an operator, stop
    }

    int precedence = GetOperatorPrecedence(op);
    if (precedence < minPrecedence) {
      break;
    }

    pos++; // consume operator

    std::string rightClassName;
    // Climb! rhs should have higher precedence than current op
    llvm::Value *right = CompileExprTokensRecursive(
        tokens, pos, expectedType, precedence + 1, &rightClassName);
    if (!right)
      return nullptr;

    // Apply operator (either overload or primitive)
    bool usedOverload = false;
    if (!leftClassName.empty()) {
      auto classIt = m_CompiledClasses.find(leftClassName);
      if (classIt != m_CompiledClasses.end()) {
        // Determine method name from operator
        std::string methodName;
        if (op == "+")
          methodName = "Plus";
        else if (op == "-")
          methodName = "Minus";
        else if (op == "*")
          methodName = "Multiply";
        else if (op == "/")
          methodName = "Divide";

        if (!methodName.empty()) {
          auto &classInfo = classIt->second;
          auto methodIt = classInfo.methods.find(methodName);
          if (methodIt != classInfo.methods.end() && methodIt->second) {
            auto &builder = QLVM::GetBuilder();
            // Call the method: result = left.MethodName(right)

            // Fix arguments: If they are pointers to pointers (stack allocas),
            // we load the object pointer
            llvm::Value *leftArg = result;
            if (leftArg->getType()->isPointerTy()) {
              // Heuristic: If it's a pointer to pointer, load it.
              // However, with opaque pointers this is hard.
              // We rely on knowing that class instances are handled as
              // pointers. But 'result' from CompilePrimaryExpr (variable) is
              // AllocaInst (stack address). 'result' from CompileMethodCall is
              // the object pointer. We need to differentiate. If it's an
              // instruction, we can check.
              if (llvm::isa<llvm::AllocaInst>(leftArg)) {
                leftArg = builder.CreateLoad(builder.getPtrTy(), leftArg,
                                             "left.load");
              }
            }

            llvm::Value *rightArg = right;
            if (rightArg->getType()->isPointerTy()) {
              if (llvm::isa<llvm::AllocaInst>(rightArg)) {
                rightArg = builder.CreateLoad(builder.getPtrTy(), rightArg,
                                              "right.load");
              }
            }

            std::vector<llvm::Value *> args = {leftArg, rightArg};
            result = builder.CreateCall(methodIt->second, args,
                                        "op_" + methodName + "_tmp");
            usedOverload = true;

            // Update class type for result
            auto retIt = classInfo.methodReturnTypes.find(methodName);
            leftClassName = (retIt != classInfo.methodReturnTypes.end())
                                ? retIt->second
                                : "";
            if (outClassName)
              *outClassName = leftClassName;

            std::cout << "[DEBUG] QJitRunner: Used operator overload "
                      << classIt->first << "." << methodName << " for '" << op
                      << "'" << std::endl;
          }
        }
      }
    }

    if (!usedOverload) {
      // Don't try to do arithmetic on pointer types (class instances)
      if (result->getType()->isPointerTy() && right->getType()->isPointerTy()) {
        // Check if this is string concatenation (allowed)
        if (op != "+") {
          std::cerr << "[ERROR] QJitRunner: Cannot apply operator '" << op
                    << "' to class instances without overload" << std::endl;
          return nullptr;
        }
      }
      result = ApplyBinaryOp(op, result, right);
      if (!result)
        return nullptr;

      // Result of primitive binary op is not a class instance
      leftClassName = "";
      if (outClassName)
        *outClassName = "";
    }
  }

  return result;
}

llvm::Value *QJitRunner::CompileExpression(std::shared_ptr<QExpression> expr,
                                           llvm::Type *expectedType,
                                           std::string *outClassName) {
  if (!expr)
    return nullptr;

  const auto &elements = expr->GetElements();

  if (elements.empty()) {
    std::cerr << "[ERROR] QJitRunner: Empty expression" << std::endl;
    return nullptr;
  }

  size_t pos = 0;
  llvm::Value *val = CompileExprTokens(elements, pos, expectedType);
  if (!val)
    return nullptr;

  // Final cast if needed
  if (expectedType && val->getType() != expectedType) {
    auto &builder = QLVM::GetBuilder();
    if (expectedType->isFloatingPointTy() && val->getType()->isIntegerTy()) {
      return builder.CreateSIToFP(val, expectedType, "cast_tmp");
    } else if (expectedType->isIntegerTy() &&
               val->getType()->isFloatingPointTy()) {
      return builder.CreateFPToSI(val, expectedType, "cast_tmp");
    }
  }

  return val;
}

// ============================================================================
// Variable Declaration
// ============================================================================

void QJitRunner::CompileVariableDecl(std::shared_ptr<QVariableDecl> varDecl) {
  if (!varDecl)
    return;

  auto &builder = QLVM::GetBuilder();

  std::string varName = varDecl->GetName();
  llvm::Type *varType = GetLLVMType(static_cast<int>(varDecl->GetVarType()),
                                    varDecl->GetTypeName());

  if (!varType) {
    std::cerr << "[ERROR] QJitRunner: Cannot determine type for variable: "
              << varName << std::endl;
    return;
  }

  std::cout << "[DEBUG] QJitRunner: Compiling variable declaration: " << varName
            << std::endl;

  llvm::AllocaInst *alloca = builder.CreateAlloca(varType, nullptr, varName);
  m_LocalVariables[varName] = alloca;

  // If this is a class-type variable, register it in m_VariableTypes
  if (varDecl->GetVarType() == TokenType::T_IDENTIFIER &&
      !varDecl->GetTypeName().empty()) {
    if (m_CompiledClasses.find(varDecl->GetTypeName()) !=
        m_CompiledClasses.end()) {
      m_VariableTypes[varName] = varDecl->GetTypeName();
      std::cout << "[DEBUG] QJitRunner: Registered variable '" << varName
                << "' as class type '" << varDecl->GetTypeName() << "'"
                << std::endl;
    }
  }

  if (varDecl->HasInitializer()) {
    llvm::Value *initValue =
        CompileExpression(varDecl->GetInitializer(), varType);
    if (initValue) {
      builder.CreateStore(initValue, alloca);

      // Deduce class name if unknown
      if (varDecl->GetVarType() == TokenType::T_UNKNOWN ||
          varDecl->GetVarType() == TokenType::T_IDENTIFIER) {
        auto expr = varDecl->GetInitializer();
        const auto &elements = expr->GetElements();
        if (!elements.empty() && elements[0].type == TokenType::T_NEW) {
          if (elements.size() > 1 &&
              elements[1].type == TokenType::T_IDENTIFIER) {
            m_VariableTypes[varName] = elements[1].value;
            std::cout << "[DEBUG] QJitRunner: Deduced type for '" << varName
                      << "' as '" << elements[1].value << "'" << std::endl;
          }
        }
      }
    } else {
      std::cerr << "[ERROR] QJitRunner: Failed to compile initializer for: "
                << varName << std::endl;
    }
  }
}

// ============================================================================
// For Loop Compilation
// ============================================================================

void QJitRunner::CompileForLoop(std::shared_ptr<QFor> forNode) {
  if (!forNode)
    return;

  std::cout << "[DEBUG] QJitRunner: Compiling for loop, var: "
            << forNode->GetVarName() << std::endl;

  auto &context = QLVM::GetContext();
  auto &builder = QLVM::GetBuilder();

  llvm::Function *currentFunc = builder.GetInsertBlock()->getParent();

  std::string varName = forNode->GetVarName();
  llvm::AllocaInst *loopVar = nullptr;

  auto it = m_LocalVariables.find(varName);
  if (it != m_LocalVariables.end()) {
    loopVar = it->second;
  } else {
    llvm::Type *varType = builder.getInt32Ty();
    if (forNode->HasDeclaredType()) {
      varType = GetLLVMType(static_cast<int>(forNode->GetVarType()));
    }
    loopVar = builder.CreateAlloca(varType, nullptr, varName);
    m_LocalVariables[varName] = loopVar;
  }

  llvm::Value *startVal =
      CompileExpression(forNode->GetStart(), loopVar->getAllocatedType());
  if (!startVal) {
    std::cerr << "[ERROR] QJitRunner: Failed to compile for loop start value"
              << std::endl;
    return;
  }
  builder.CreateStore(startVal, loopVar);

  llvm::Value *endVal =
      CompileExpression(forNode->GetEnd(), loopVar->getAllocatedType());
  if (!endVal) {
    std::cerr << "[ERROR] QJitRunner: Failed to compile for loop end value"
              << std::endl;
    return;
  }

  llvm::Value *stepVal = nullptr;
  if (forNode->GetStep()) {
    stepVal =
        CompileExpression(forNode->GetStep(), loopVar->getAllocatedType());
  } else {
    stepVal = llvm::ConstantInt::get(loopVar->getAllocatedType(), 1);
  }

  llvm::BasicBlock *condBB =
      llvm::BasicBlock::Create(context, "loop.cond", currentFunc);
  llvm::BasicBlock *bodyBB =
      llvm::BasicBlock::Create(context, "loop.body", currentFunc);
  llvm::BasicBlock *incrBB =
      llvm::BasicBlock::Create(context, "loop.incr", currentFunc);
  llvm::BasicBlock *exitBB =
      llvm::BasicBlock::Create(context, "loop.exit", currentFunc);

  builder.CreateBr(condBB);

  builder.SetInsertPoint(condBB);
  llvm::Value *currentVal = builder.CreateLoad(loopVar->getAllocatedType(),
                                               loopVar, varName + ".val");
  llvm::Value *cmp = builder.CreateICmpSLE(currentVal, endVal, "loopcond");
  builder.CreateCondBr(cmp, bodyBB, exitBB);

  builder.SetInsertPoint(bodyBB);
  if (forNode->GetBody()) {
    CompileCodeBlock(forNode->GetBody());
  }
  builder.CreateBr(incrBB);

  builder.SetInsertPoint(incrBB);
  llvm::Value *nextVal =
      builder.CreateAdd(builder.CreateLoad(loopVar->getAllocatedType(), loopVar,
                                           varName + ".cur"),
                        stepVal, varName + ".next");
  builder.CreateStore(nextVal, loopVar);
  builder.CreateBr(condBB);

  builder.SetInsertPoint(exitBB);

  std::cout << "[DEBUG] QJitRunner: For loop compiled successfully"
            << std::endl;
}

// ============================================================================
// If Statement Compilation
// ============================================================================

void QJitRunner::CompileIf(std::shared_ptr<QIf> ifNode) {
  if (!ifNode)
    return;

  auto &builder = QLVM::GetBuilder();
  auto &context = QLVM::GetContext();
  llvm::Function *currentFunc = builder.GetInsertBlock()->getParent();

  std::cout << "[DEBUG] QJitRunner: Compiling if statement" << std::endl;

  // Compile the main if condition
  llvm::Value *condVal = CompileExpression(ifNode->GetCondition());
  if (!condVal) {
    std::cerr << "[ERROR] QJitRunner: Failed to compile if condition"
              << std::endl;
    return;
  }

  // If the condition is an i1 (bool), use it directly. Otherwise compare != 0
  if (!condVal->getType()->isIntegerTy(1)) {
    condVal = builder.CreateICmpNE(
        condVal, llvm::ConstantInt::get(condVal->getType(), 0), "ifcond");
  }

  // Create basic blocks for then, else-if chain, else, and merge
  llvm::BasicBlock *thenBB =
      llvm::BasicBlock::Create(context, "if.then", currentFunc);
  llvm::BasicBlock *mergeBB =
      llvm::BasicBlock::Create(context, "if.merge", currentFunc);

  // Determine if we have else-if or else blocks
  const auto &elseIfBlocks = ifNode->GetElseIfBlocks();
  bool hasElseIfs = !elseIfBlocks.empty();
  bool hasElse = ifNode->HasElse();

  // The "else" BB - either first elseif, else block, or merge
  llvm::BasicBlock *elseBB = nullptr;
  if (hasElseIfs) {
    elseBB = llvm::BasicBlock::Create(context, "if.elseif", currentFunc);
  } else if (hasElse) {
    elseBB = llvm::BasicBlock::Create(context, "if.else", currentFunc);
  } else {
    elseBB = mergeBB;
  }

  // Create conditional branch
  builder.CreateCondBr(condVal, thenBB, elseBB);

  // Compile then block
  builder.SetInsertPoint(thenBB);
  CompileCodeBlock(ifNode->GetThenBlock());

  // Branch to merge (if not already terminated)
  if (!builder.GetInsertBlock()->getTerminator()) {
    builder.CreateBr(mergeBB);
  }

  // Handle else-if blocks
  llvm::BasicBlock *currentElseIfBB = elseBB;
  for (size_t i = 0; i < elseIfBlocks.size(); ++i) {
    const auto &elseIfPair = elseIfBlocks[i];

    builder.SetInsertPoint(currentElseIfBB);

    // Compile else-if condition
    llvm::Value *elseIfCond = CompileExpression(elseIfPair.first);
    if (!elseIfCond) {
      std::cerr << "[ERROR] QJitRunner: Failed to compile else-if condition"
                << std::endl;
      continue;
    }

    // Ensure condition is bool
    if (!elseIfCond->getType()->isIntegerTy(1)) {
      elseIfCond = builder.CreateICmpNE(
          elseIfCond, llvm::ConstantInt::get(elseIfCond->getType(), 0),
          "elseifcond");
    }

    // Create then block for this else-if
    llvm::BasicBlock *elseIfThenBB =
        llvm::BasicBlock::Create(context, "elseif.then", currentFunc);

    // Determine next block
    llvm::BasicBlock *nextBB = nullptr;
    if (i + 1 < elseIfBlocks.size()) {
      nextBB = llvm::BasicBlock::Create(context, "if.elseif", currentFunc);
    } else if (hasElse) {
      nextBB = llvm::BasicBlock::Create(context, "if.else", currentFunc);
    } else {
      nextBB = mergeBB;
    }

    builder.CreateCondBr(elseIfCond, elseIfThenBB, nextBB);

    // Compile else-if body
    builder.SetInsertPoint(elseIfThenBB);
    CompileCodeBlock(elseIfPair.second);

    if (!builder.GetInsertBlock()->getTerminator()) {
      builder.CreateBr(mergeBB);
    }

    currentElseIfBB = nextBB;
  }

  // Handle else block
  if (hasElse) {
    builder.SetInsertPoint(currentElseIfBB);
    CompileCodeBlock(ifNode->GetElseBlock());

    if (!builder.GetInsertBlock()->getTerminator()) {
      builder.CreateBr(mergeBB);
    }
  }

  // Continue at merge point
  builder.SetInsertPoint(mergeBB);

  std::cout << "[DEBUG] QJitRunner: If statement compiled successfully"
            << std::endl;
}

// ============================================================================
// Statement Compilation
// ============================================================================

void QJitRunner::CompileStatement(std::shared_ptr<QStatement> stmt) {
  if (!stmt)
    return;

  auto &builder = QLVM::GetBuilder();
  std::string funcName = stmt->GetName();

  std::cout << "[DEBUG] QJitRunner: Compiling statement call: " << funcName
            << std::endl;

  llvm::Function *targetFunc = m_LVMContext->GetLLVMFunc(funcName);
  if (!targetFunc) {
    // Check if it's an implicit method call (this.method())
    if (m_CurrentInstance && !m_CurrentClassName.empty()) {
      auto classIt = m_CompiledClasses.find(m_CurrentClassName);
      if (classIt != m_CompiledClasses.end()) {
        if (classIt->second.methods.count(funcName)) {
          std::cout << "[DEBUG] QJitRunner: Found implicit method call for "
                    << funcName << std::endl;
          // Convert QStatement to QMethodCall-like behavior

          // Need to manually create arguments since we don't have a QMethodCall
          // node But we can just call CompileMethodCall logic inline or
          // CreateCall directly

          llvm::Function *methodFunc = classIt->second.methods[funcName];
          std::vector<llvm::Value *> callArgs;
          callArgs.push_back(m_CurrentInstance); // this

          if (auto params = stmt->GetParameters()) {
            llvm::FunctionType *ft = methodFunc->getFunctionType();
            // We need to match arguments to parameters, skipping 'this' (param
            // 0) Method parameters start at index 1

            const auto &exprs = params->GetParameters();
            for (size_t i = 0; i < exprs.size(); ++i) {
              // Check if we have enough parameters in function definition
              if (i + 1 < ft->getNumParams()) {
                llvm::Type *paramType =
                    ft->getParamType(static_cast<unsigned>(i + 1));
                llvm::Value *argValue = CompileExpression(exprs[i], paramType);
                if (argValue) {
                  callArgs.push_back(argValue);
                } else {
                  std::cerr << "[ERROR] QJitRunner: Failed to compile argument "
                            << i << " for implicit method call " << funcName
                            << std::endl;
                }
              } else {
                // Varargs or error?
                llvm::Value *argValue = CompileExpression(exprs[i]);
                if (argValue) {
                  // Vararg promotion: float -> double
                  if (argValue->getType()->isFloatTy()) {
                    argValue =
                        builder.CreateFPExt(argValue, builder.getDoubleTy());
                  }
                  callArgs.push_back(argValue);
                }
              }
            }
          }

          builder.CreateCall(methodFunc, callArgs);
          return;
        }
      }
    }

    std::cerr << "[ERROR] QJitRunner: Function not registered in QLVMContext: "
              << funcName << std::endl;
    return;
  }

  std::vector<llvm::Value *> llvmArgs;
  if (auto params = stmt->GetParameters()) {
    const auto &exprs = params->GetParameters();
    for (size_t i = 0; i < exprs.size(); ++i) {
      llvm::Type *paramType = nullptr;
      if (i < targetFunc->getFunctionType()->getNumParams()) {
        paramType = targetFunc->getFunctionType()->getParamType(
            static_cast<unsigned>(i));
      }

      llvm::Value *argValue = CompileExpression(exprs[i], paramType);
      if (argValue) {
        // Vararg promotion: float -> double
        if (!paramType && argValue->getType()->isFloatTy()) {
          argValue = builder.CreateFPExt(argValue, builder.getDoubleTy());
        }
        llvmArgs.push_back(argValue);
      } else {
        std::cerr << "[ERROR] QJitRunner: Failed to compile argument " << i
                  << " for " << funcName << std::endl;
      }
    }
  }

  builder.CreateCall(targetFunc, llvmArgs);
}

// ============================================================================
// Node Dispatch
// ============================================================================

void QJitRunner::CompileNode(std::shared_ptr<QNode> node) {
  if (!node)
    return;

  if (auto stmt = std::dynamic_pointer_cast<QStatement>(node)) {
    CompileStatement(stmt);
    return;
  }

  if (auto varDecl = std::dynamic_pointer_cast<QVariableDecl>(node)) {
    CompileVariableDecl(varDecl);
    return;
  }

  if (auto forNode = std::dynamic_pointer_cast<QFor>(node)) {
    CompileForLoop(forNode);
    return;
  }

  if (auto ifNode = std::dynamic_pointer_cast<QIf>(node)) {
    CompileIf(ifNode);
    return;
  }

  if (auto classNode = std::dynamic_pointer_cast<QClass>(node)) {
    CompileClass(classNode);
    return;
  }

  if (auto instDecl = std::dynamic_pointer_cast<QInstanceDecl>(node)) {
    CompileInstanceDecl(instDecl);
    return;
  }

  if (auto memberAssign = std::dynamic_pointer_cast<QMemberAssign>(node)) {
    CompileMemberAssign(memberAssign);
    return;
  }

  if (auto assign = std::dynamic_pointer_cast<QAssign>(node)) {
    CompileAssign(assign);
    return;
  }

  if (auto returnNode = std::dynamic_pointer_cast<QReturn>(node)) {
    CompileReturn(returnNode);
    return;
  }

  if (auto methodCall = std::dynamic_pointer_cast<QMethodCall>(node)) {
    CompileMethodCall(methodCall);
    return;
  }

  std::cout << "[DEBUG] QJitRunner: Skipping unsupported node type: "
            << node->GetName() << std::endl;
}

// ============================================================================
// Code Block Compilation
// ============================================================================

void QJitRunner::CompileCodeBlock(std::shared_ptr<QCode> code) {
  if (!code)
    return;

  std::cout << "[DEBUG] QJitRunner: Compiling code block with "
            << code->GetNodes().size() << " nodes." << std::endl;

  auto &builder = QLVM::GetBuilder();

  for (auto node : code->GetNodes()) {
    CompileNode(node);
    // If the block is now terminated (e.g. by a return), stop compiling this
    // block
    if (builder.GetInsertBlock()->getTerminator()) {
      break;
    }
  }
}

// ============================================================================
// Class Compilation
// ============================================================================

void QJitRunner::CompileClass(std::shared_ptr<QClass> classNode) {
  if (!classNode)
    return;

  std::string className = classNode->GetName();
  auto &context = QLVM::GetContext();

  std::cout << "[DEBUG] QJitRunner: Compiling class '" << className << "'"
            << std::endl;

  // Check if already compiled
  if (m_CompiledClasses.find(className) != m_CompiledClasses.end()) {
    std::cout << "[DEBUG] QJitRunner: Class '" << className
              << "' already compiled" << std::endl;
    return;
  }

  // Get or create struct type (don't create duplicate with suffix like .1)
  llvm::StructType *structType =
      llvm::StructType::getTypeByName(context, className);
  if (!structType) {
    structType = llvm::StructType::create(context, className);
  }

  // Collect member types and names
  std::vector<llvm::Type *> memberTypes;
  std::vector<std::string> memberNames;
  std::vector<int> memberTypeTokens;
  std::vector<std::string> memberTypeNames;

  for (const auto &member : classNode->GetMembers()) {
    llvm::Type *memberType = GetLLVMType(static_cast<int>(member->GetVarType()),
                                         member->GetTypeName());
    if (memberType) {
      memberTypes.push_back(memberType);
      memberNames.push_back(member->GetName());
      memberTypeTokens.push_back(static_cast<int>(member->GetVarType()));
      memberTypeNames.push_back(member->GetTypeName());
      std::cout << "[DEBUG]   Member: " << member->GetName() << " (type index "
                << static_cast<int>(member->GetVarType()) << ")" << std::endl;
    }
  }

  // Set struct body
  structType->setBody(memberTypes);

  // Store in registry
  CompiledClass classInfo;
  classInfo.structType = structType;
  classInfo.memberNames = memberNames;
  classInfo.memberTypes = memberTypes;
  classInfo.memberTypeTokens = memberTypeTokens;
  classInfo.memberTypeNames = memberTypeNames;
  classInfo.isStatic = classNode->IsStatic();
  m_CompiledClasses[className] = classInfo;

  if (classNode->IsStatic()) {
    std::cout << "[DEBUG] QJitRunner: Class '" << className
              << "' is STATIC (singleton)" << std::endl;
  }

  std::cout << "[DEBUG] QJitRunner: Class '" << className << "' compiled with "
            << memberTypes.size() << " members" << std::endl;

  // Pass 1: Create method prototypes
  auto *module = QLVM::GetModule();
  auto &builder = QLVM::GetBuilder();
  for (const auto &method : classNode->GetMethods()) {
    std::string methodName = method->GetName();
    std::string fullName = className + "_" + methodName;

    std::vector<llvm::Type *> paramTypes;
    // Add 'this' pointer
    paramTypes.push_back(llvm::PointerType::getUnqual(
        classInfo.structType->getContext())); // this*

    for (const auto &param : method->GetParameters()) {
      llvm::Type *paramType =
          GetLLVMType(static_cast<int>(param.type), param.typeName);
      if (paramType) {
        paramTypes.push_back(paramType);
      }
    }

    // Get return type
    llvm::Type *returnType =
        method->GetReturnType() == TokenType::T_VOID
            ? builder.getVoidTy()
            : GetLLVMType(static_cast<int>(method->GetReturnType()),
                          method->GetReturnTypeName());
    if (!returnType)
      returnType = builder.getVoidTy();

    // Create function type and function
    llvm::FunctionType *funcType =
        llvm::FunctionType::get(returnType, paramTypes, false);
    llvm::Function *func = llvm::cast<llvm::Function>(
        module->getOrInsertFunction(fullName, funcType).getCallee());

    classInfo.methods[methodName] = func;
    if (method->GetReturnType() == TokenType::T_IDENTIFIER) {
      classInfo.methodReturnTypes[methodName] = method->GetReturnTypeName();
    }
  }

  // Update registry with prototypes
  m_CompiledClasses[className] = classInfo;

  // Pass 2: Compile method bodies
  for (const auto &method : classNode->GetMethods()) {
    CompileMethod(className, method);
  }
}

void QJitRunner::CompileMethod(const std::string &className,
                               std::shared_ptr<QMethod> method) {
  if (!method)
    return;

  auto &context = QLVM::GetContext();
  auto &builder = QLVM::GetBuilder();
  auto *module = QLVM::GetModule();

  std::string methodName = method->GetName();
  std::string fullName = className + "_" + methodName;

  std::cout << "[DEBUG] QJitRunner: Compiling method '" << fullName << "'"
            << std::endl;

  // Get class info
  auto classIt = m_CompiledClasses.find(className);
  if (classIt == m_CompiledClasses.end()) {
    std::cerr << "[ERROR] QJitRunner: Class '" << className
              << "' not found for method compilation" << std::endl;
    return;
  }
  CompiledClass &classInfo = classIt->second;

  // Check if function already exists (created in prototype pass)
  llvm::Function *func = module->getFunction(fullName);
  llvm::Type *returnType = nullptr;

  if (!func) {
    // Build parameter types: first is pointer to struct (this*)
    std::vector<llvm::Type *> paramTypes;
    paramTypes.push_back(llvm::PointerType::getUnqual(
        classInfo.structType->getContext())); // this*

    for (const auto &param : method->GetParameters()) {
      llvm::Type *paramType =
          GetLLVMType(static_cast<int>(param.type), param.typeName);
      if (paramType) {
        paramTypes.push_back(paramType);
      }
    }

    // Get return type
    returnType = method->GetReturnType() == TokenType::T_VOID
                     ? builder.getVoidTy()
                     : GetLLVMType(static_cast<int>(method->GetReturnType()),
                                   method->GetReturnTypeName());
    if (!returnType)
      returnType = builder.getVoidTy();

    // Store return type name for class method tracking
    if (method->GetReturnType() == TokenType::T_IDENTIFIER) {
      classInfo.methodReturnTypes[methodName] = method->GetReturnTypeName();
    }

    // Create function type and function
    llvm::FunctionType *funcType =
        llvm::FunctionType::get(returnType, paramTypes, false);
    func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                  fullName, module);
  } else {
    returnType = func->getReturnType();
  }

  // Create entry block
  llvm::BasicBlock *entryBB = llvm::BasicBlock::Create(context, "entry", func);

  // Save current state
  auto savedLocals = m_LocalVariables;
  auto savedVariableTypes = m_VariableTypes;
  auto savedInstance = m_CurrentInstance;
  auto savedClassName = m_CurrentClassName;
  auto savedInsertPoint = builder.GetInsertBlock();

  // Set method context
  builder.SetInsertPoint(entryBB);
  m_LocalVariables.clear();
  m_VariableTypes.clear();
  m_CurrentClassName = className;

  // Name parameters and create allocas
  auto argIt = func->arg_begin();

  // 'this' pointer - store it for member access
  llvm::Value *thisPtr = &*argIt;
  thisPtr->setName("this");
  m_CurrentInstance = thisPtr;
  argIt++;

  // Other parameters
  int paramIdx = 0;
  for (const auto &param : method->GetParameters()) {
    llvm::Value *argVal = &*argIt;
    argVal->setName(param.name);

    llvm::Type *paramType =
        GetLLVMType(static_cast<int>(param.type), param.typeName);
    llvm::AllocaInst *alloca =
        builder.CreateAlloca(paramType, nullptr, param.name + ".addr");
    builder.CreateStore(argVal, alloca);
    m_LocalVariables[param.name] = alloca;

    // If this is a class-type parameter, register it in m_VariableTypes
    if (param.type == TokenType::T_IDENTIFIER && !param.typeName.empty()) {
      if (m_CompiledClasses.find(param.typeName) != m_CompiledClasses.end()) {
        m_VariableTypes[param.name] = param.typeName;
        std::cout << "[DEBUG] QJitRunner: Registered parameter '" << param.name
                  << "' as class type '" << param.typeName << "'" << std::endl;
      }
    }

    argIt++;
    paramIdx++;
  }

  // Compile method body
  CompileCodeBlock(method->GetBody());

  // Add return if not already terminated
  if (!builder.GetInsertBlock()->getTerminator()) {
    if (returnType->isVoidTy()) {
      builder.CreateRetVoid();
    } else {
      builder.CreateRet(llvm::Constant::getNullValue(returnType));
    }
  }

  // Restore state
  m_LocalVariables = savedLocals;
  m_VariableTypes = savedVariableTypes;
  m_CurrentInstance = savedInstance;
  m_CurrentClassName = savedClassName;
  if (savedInsertPoint) {
    builder.SetInsertPoint(savedInsertPoint);
  }

  // Store method in class info
  classInfo.methods[methodName] = func;
  m_CompiledClasses[className] = classInfo; // Update the map entry

  std::cout << "[DEBUG] QJitRunner: Method '" << fullName << "' compiled"
            << std::endl;

  // Generate a universal wrapper function for dynamic calling
  // Wrapper signature: void ClassName_MethodName__wrap(void* this, void** args)
  GenerateMethodWrapper(className, methodName, func, method);
}

void QJitRunner::CompileAssign(std::shared_ptr<QAssign> assign) {
  if (!assign)
    return;

  std::string varName = assign->GetVariableName();
  auto &builder = QLVM::GetBuilder();

  std::cout << "[DEBUG] QJitRunner: Compiling assignment to '" << varName << "'"
            << std::endl;

  // Check if this is an implicit member assignment (inside a method)
  if (m_CurrentInstance && !m_CurrentClassName.empty()) {
    auto classIt = m_CompiledClasses.find(m_CurrentClassName);
    if (classIt != m_CompiledClasses.end()) {
      CompiledClass &classInfo = classIt->second;
      int memberIdx = FindMemberIndex(classInfo, varName);

      if (memberIdx >= 0) {
        // This is a member assignment (this.varName = value)
        std::cout << "[DEBUG] QJitRunner: Treating as member assignment this."
                  << varName << std::endl;

        llvm::Type *memberType = classInfo.memberTypes[memberIdx];
        llvm::Value *value =
            CompileExpression(assign->GetValueExpression(), memberType);
        if (!value) {
          std::cerr << "[ERROR] QJitRunner: Failed to compile value for member "
                       "assignment"
                    << std::endl;
          return;
        }

        llvm::Value *memberPtr = builder.CreateStructGEP(
            classInfo.structType, m_CurrentInstance,
            static_cast<unsigned>(memberIdx), "this." + varName + ".ptr");

        builder.CreateStore(value, memberPtr);
        return;
      }
    }
  }

  // Regular local variable assignment
  auto it = m_LocalVariables.find(varName);
  if (it == m_LocalVariables.end()) {
    std::cerr << "[ERROR] QJitRunner: Undefined variable: " << varName
              << std::endl;
    return;
  }

  std::string className;
  llvm::AllocaInst *alloca = it->second;
  llvm::Value *value = CompileExpression(
      assign->GetValueExpression(), alloca->getAllocatedType(), &className);
  if (!value) {
    std::cerr << "[ERROR] QJitRunner: Failed to compile value expression"
              << std::endl;
    return;
  }

  // Update class type tracking if it's a class instance
  if (!className.empty()) {
    m_VariableTypes[varName] = className;
  } else {
    // If it's no longer a class instance (e.g. reassigned to primitive), remove
    // from tracking.
    // However, if we failed to deduce the type but it's still a pointer
    // assignment (likely a method call returning an object), we should preserve
    // the existing type if possible. In this case, 'value' is a pointer, so we
    // shouldn't erase the type info unless we are sure. For now, let's only
    // erase if we know for sure it's NOT a class result. But since we can't
    // easily know, let's look at the existing type.
    if (m_VariableTypes.find(varName) == m_VariableTypes.end()) {
      // Was not a class, and still no class name -> good.
    } else {
      // Was a class. If the new value is NOT a pointer, it's definitely not a
      // class instance.
      if (!value->getType()->isPointerTy()) {
        m_VariableTypes.erase(varName);
      }
      // If it IS a pointer, we assume type preservation (e.g. pos =
      // pos.GetValue()) unless we have better info.
    }
  }

  builder.CreateStore(value, alloca);
}

void QJitRunner::CompileReturn(std::shared_ptr<QReturn> returnNode) {
  if (!returnNode)
    return;

  auto &builder = QLVM::GetBuilder();
  llvm::Function *currentFunc = builder.GetInsertBlock()->getParent();
  llvm::Type *returnType = currentFunc->getReturnType();

  if (returnNode->HasExpression()) {
    // Check if return type is a pointer (class return type)
    if (returnType->isPointerTy()) {
      // For class return types, we need to return the pointer to the instance
      // Check if the expression is a simple identifier (variable name)
      auto expr = returnNode->GetExpression();
      if (expr) {
        const std::vector<Token> &tokens = expr->GetElements();
        if (tokens.size() == 1 && tokens[0].type == TokenType::T_IDENTIFIER) {
          std::string varName = tokens[0].value;
          auto varIt = m_LocalVariables.find(varName);
          if (varIt != m_LocalVariables.end()) {
            // Check if it's a class instance (stored as a pointer in the
            // alloca)
            auto typeIt = m_VariableTypes.find(varName);
            if (typeIt != m_VariableTypes.end()) {
              // Load the pointer value from the alloca and return it
              llvm::Value *ptrVal =
                  builder.CreateLoad(varIt->second->getAllocatedType(),
                                     varIt->second, varName + ".ret");
              builder.CreateRet(ptrVal);
              return;
            }
          }
        }
      }
    }

    // For primitive types, compile normally
    llvm::Value *retVal =
        CompileExpression(returnNode->GetExpression(), returnType);
    if (retVal) {
      builder.CreateRet(retVal);
    } else {
      std::cerr << "[ERROR] QJitRunner: Failed to compile return expression"
                << std::endl;
      if (!returnType->isVoidTy()) {
        builder.CreateRet(llvm::Constant::getNullValue(returnType));
      } else {
        builder.CreateRetVoid();
      }
    }
  } else {
    if (returnType->isVoidTy()) {
      builder.CreateRetVoid();
    } else {
      std::cerr << "[ERROR] QJitRunner: Return without value in non-void method"
                << std::endl;
      builder.CreateRet(llvm::Constant::getNullValue(returnType));
    }
  }
}

llvm::Value *
QJitRunner::CompileMethodCall(std::shared_ptr<QMethodCall> methodCall) {
  if (!methodCall)
    return nullptr;

  auto &builder = QLVM::GetBuilder();
  std::string instanceName = methodCall->GetInstanceName();
  std::string methodName = methodCall->GetMethodName();

  std::cout << "[DEBUG] QJitRunner: Compiling method call " << instanceName
            << "." << methodName << std::endl;

  llvm::Value *instancePtr = nullptr;
  std::string className;

  if (instanceName == "this" || instanceName.empty()) {
    instancePtr = m_CurrentInstance;
    className = m_CurrentClassName;
  } else {
    auto varIt = m_LocalVariables.find(instanceName);
    if (varIt == m_LocalVariables.end()) {
      std::cerr << "[ERROR] QJitRunner: Undefined instance: " << instanceName
                << std::endl;
      return nullptr;
    }
    instancePtr = varIt->second;

    auto typeIt = m_VariableTypes.find(instanceName);
    if (typeIt == m_VariableTypes.end()) {
      std::cerr << "[ERROR] QJitRunner: Variable '" << instanceName
                << "' is not a class instance" << std::endl;
      return nullptr;
    }
    className = typeIt->second;

    // Load the instance pointer from the stack variable
    instancePtr = builder.CreateLoad(builder.getPtrTy(), varIt->second,
                                     instanceName + ".ptr");
  }

  if (!instancePtr || className.empty()) {
    std::cerr << "[ERROR] QJitRunner: Cannot resolve instance for method call: "
              << methodName << std::endl;
    return nullptr;
  }

  auto classIt = m_CompiledClasses.find(className);
  if (classIt == m_CompiledClasses.end()) {
    std::cerr << "[ERROR] QJitRunner: Class '" << className << "' not found"
              << std::endl;
    return nullptr;
  }

  CompiledClass &classInfo = classIt->second;
  auto methIt = classInfo.methods.find(methodName);
  if (methIt == classInfo.methods.end()) {
    std::cerr << "[ERROR] QJitRunner: Method '" << methodName
              << "' not found in class '" << className << "'" << std::endl;
    return nullptr;
  }

  llvm::Function *targetFunc = methIt->second;

  // Compile arguments
  std::vector<llvm::Value *> callArgs;
  callArgs.push_back(instancePtr); // 'this' pointer

  if (auto args = methodCall->GetArguments()) {
    const auto &params = args->GetParameters();
    for (size_t i = 0; i < params.size(); ++i) {
      // +1 because first param is 'this'
      llvm::Type *paramType = nullptr;
      if (i + 1 < targetFunc->getFunctionType()->getNumParams()) {
        paramType = targetFunc->getFunctionType()->getParamType(
            static_cast<unsigned>(i + 1));
      }

      llvm::Value *argVal = CompileExpression(params[i], paramType);
      if (argVal) {
        // Vararg promotion: float -> double
        if (!paramType && argVal->getType()->isFloatTy()) {
          argVal = builder.CreateFPExt(argVal, builder.getDoubleTy());
        }
        callArgs.push_back(argVal);
      } else {
        std::cerr << "[ERROR] QJitRunner: Failed to compile argument " << i
                  << " for " << methodName << std::endl;
      }
    }
  }

  return builder.CreateCall(targetFunc, callArgs);
}

llvm::Function *
QJitRunner::FindConstructor(const CompiledClass &classInfo,
                            const std::string &className,
                            const std::vector<llvm::Value *> &args) {
  // Constructor has same name as class
  auto it = classInfo.methods.find(className);
  if (it != classInfo.methods.end()) {
    llvm::Function *func = it->second;
    // Check argument count (subtract 1 for 'this' pointer)
    if (func->arg_size() - 1 == args.size()) {
      // TODO: Add type matching for overloaded constructors
      return func;
    }
  }
  return nullptr;
}

void QJitRunner::CompileInstanceDecl(std::shared_ptr<QInstanceDecl> instDecl) {
  if (!instDecl)
    return;

  std::string className = instDecl->GetQClassName();
  std::string instanceName = instDecl->GetInstanceName();
  auto &builder = QLVM::GetBuilder();

  std::cout << "[DEBUG] QJitRunner: Creating instance '" << instanceName
            << "' of class '" << className << "'" << std::endl;

  // Find compiled class
  auto it = m_CompiledClasses.find(className);
  if (it == m_CompiledClasses.end()) {
    std::cerr << "[ERROR] QJitRunner: Class '" << className << "' not found"
              << std::endl;
    return;
  }

  CompiledClass &classInfo = it->second;

  // Create stack storage for the pointer (instance variables are now always
  // pointers)
  llvm::Type *ptrType = llvm::PointerType::getUnqual(builder.getContext());
  llvm::AllocaInst *ptrAlloca =
      builder.CreateAlloca(ptrType, nullptr, instanceName);

  // Allocate memory on heap
  // Allocate memory on heap using malloc and call constructor
  llvm::Function *mallocFunc = m_LVMContext->GetLLVMFunc("malloc");
  if (!mallocFunc) {
    // Fallback: try to find or declare in current module
    mallocFunc = QLVM::GetModule()->getFunction("malloc");
    if (!mallocFunc) {
      std::vector<llvm::Type *> args = {
          llvm::Type::getInt64Ty(QLVM::GetContext())};
      llvm::FunctionType *mallocType = llvm::FunctionType::get(
          llvm::PointerType::getUnqual(QLVM::GetContext()), args, false);
      mallocFunc =
          llvm::Function::Create(mallocType, llvm::Function::ExternalLinkage,
                                 "malloc", QLVM::GetModule());
    }
  }

  if (!mallocFunc) {
    std::cerr << "[ERROR] QJitRunner: malloc not found for InstanceDecl"
              << std::endl;
    return;
  }

  uint64_t size =
      QLVM::GetModule()->getDataLayout().getTypeAllocSize(classInfo.structType);
  llvm::Value *sizeVal =
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(QLVM::GetContext()), size);

  llvm::Value *instancePtr =
      builder.CreateCall(mallocFunc, {sizeVal}, instanceName + ".heap");

  // Store instance pointer in local storage
  builder.CreateStore(instancePtr, ptrAlloca);

  // Initialize all members with default values (zero)
  for (size_t i = 0; i < classInfo.memberTypes.size(); ++i) {
    llvm::Value *memberPtr = builder.CreateStructGEP(
        classInfo.structType, instancePtr, static_cast<unsigned>(i),
        instanceName + "." + classInfo.memberNames[i] + ".ptr");

    llvm::Type *memberType = classInfo.memberTypes[i];
    llvm::Value *defaultVal = nullptr;

    if (memberType->isIntegerTy()) {
      defaultVal = llvm::ConstantInt::get(memberType, 0);
    } else if (memberType->isFloatingPointTy()) {
      defaultVal = llvm::ConstantFP::get(memberType, 0.0);
    } else if (memberType->isPointerTy()) {
      defaultVal = llvm::ConstantPointerNull::get(
          llvm::cast<llvm::PointerType>(memberType));
    }

    if (defaultVal) {
      builder.CreateStore(defaultVal, memberPtr);
    }
  }

  // Store instance alloca in local variables
  m_LocalVariables[instanceName] = ptrAlloca;
  m_VariableTypes[instanceName] = className;

  std::cout << "[DEBUG] QJitRunner: Instance '" << instanceName << "' created"
            << std::endl;

  // Call constructor if one exists, but ONLY if we don't have an initializer
  // expression (e.g., Vec3 pos = obj.GetValue())
  if (auto initExpr = instDecl->GetInitializerExpression()) {
    llvm::Value *initValue = CompileExpression(initExpr, ptrType);
    if (initValue) {
      builder.CreateStore(initValue, ptrAlloca);
    }
    return;
  }

  // Compile constructor arguments
  std::vector<llvm::Value *> constructorArgs;
  if (auto params = instDecl->GetConstructorArgs()) {
    for (const auto &expr : params->GetParameters()) {
      llvm::Value *argVal = CompileExpression(expr);
      if (argVal) {
        constructorArgs.push_back(argVal);
      }
    }
  }

  // Find matching constructor
  llvm::Function *constructor =
      FindConstructor(classInfo, className, constructorArgs);
  if (constructor) {
    std::cout << "[DEBUG] QJitRunner: Calling constructor " << className << "_"
              << className << std::endl;

    // Load instance pointer from alloca (in case it was updated)
    llvm::Value *currentInstancePtr =
        builder.CreateLoad(ptrType, ptrAlloca, instanceName + ".ptr");

    // Build call args: this pointer + constructor args
    std::vector<llvm::Value *> callArgs;
    callArgs.push_back(currentInstancePtr); // this pointer
    for (auto *arg : constructorArgs) {
      callArgs.push_back(arg);
    }

    builder.CreateCall(constructor, callArgs);
  } else if (!constructorArgs.empty()) {
    std::cerr << "[WARNING] QJitRunner: No matching constructor found for "
              << className << " with " << constructorArgs.size() << " arguments"
              << std::endl;
  }
}

void QJitRunner::CompileMemberAssign(
    std::shared_ptr<QMemberAssign> memberAssign) {
  if (!memberAssign)
    return;

  std::string instanceName = memberAssign->GetInstanceName();
  std::string memberName = memberAssign->GetMemberName();
  auto &builder = QLVM::GetBuilder();

  std::cout << "[DEBUG] QJitRunner: Compiling member assignment "
            << instanceName << "." << memberName << std::endl;

  // Look up instance
  llvm::Value *instancePtr = nullptr;
  std::string className;
  CompiledClass *staticClassInfo = nullptr;

  auto varIt = m_LocalVariables.find(instanceName);
  if (varIt != m_LocalVariables.end()) {
    // It's a local variable / instance
    auto typeIt = m_VariableTypes.find(instanceName);
    if (typeIt == m_VariableTypes.end()) {
      std::cerr << "[ERROR] QJitRunner: Variable '" << instanceName
                << "' is not a class instance" << std::endl;
      return;
    }
    className = typeIt->second;

    llvm::AllocaInst *instanceAlloca = varIt->second;
    instancePtr = instanceAlloca;

    // Check if the alloca contains a pointer (parameter case) or struct (local
    // case) For local vars holding pointers (like 'inst'), we need to load the
    // pointer first
    if (instanceAlloca->getAllocatedType()->isPointerTy()) {
      // If it's a pointer to a pointer (which local class vars are), load it
      // But wait, m_VariableTypes implies it's a class instance.
      // Local variables for classes are usually Alloca(Class*).
      instancePtr = builder.CreateLoad(instanceAlloca->getAllocatedType(),
                                       instanceAlloca, instanceName + ".ptr");
    }
  } else {
    // Check if it's a static class
    auto classIt = m_CompiledClasses.find(instanceName);
    if (classIt != m_CompiledClasses.end() && classIt->second.isStatic) {
      // It IS a static class
      className = instanceName;
      staticClassInfo = &classIt->second;

      // Get the singleton instance pointer directly
      // We need to call GetStaticInstance (runtime call) or getting it from
      // registry For JIT, we basically need the pointer      // Since the
      // memory is persistent, we can embed the pointer as a constant int and
      // cast it

      void *rawPtr = QStaticRegistry::Instance().GetInstance(className);
      llvm::Constant *addrInt =
          llvm::ConstantInt::get(llvm::Type::getInt64Ty(QLVM::GetContext()),
                                 reinterpret_cast<uint64_t>(rawPtr));

      instancePtr = builder.CreateIntToPtr(
          addrInt, llvm::PointerType::getUnqual(QLVM::GetContext()));

    } else {
      std::cerr << "[ERROR] QJitRunner: Undefined variable or static class: "
                << instanceName << std::endl;
      return;
    }
  }

  auto classIt = m_CompiledClasses.find(className);
  if (classIt == m_CompiledClasses.end()) {
    std::cerr << "[ERROR] QJitRunner: Class '" << className << "' not found"
              << std::endl;
    return;
  }

  CompiledClass &classInfo = classIt->second;
  int memberIdx = FindMemberIndex(classInfo, memberName);
  if (memberIdx < 0) {
    std::cerr << "[ERROR] QJitRunner: Member '" << memberName
              << "' not found in class '" << className << "'" << std::endl;
    return;
  }

  // Compile the value expression
  llvm::Type *memberType = classInfo.memberTypes[memberIdx];
  llvm::Value *value =
      CompileExpression(memberAssign->GetValueExpression(), memberType);
  if (!value) {
    std::cerr << "[ERROR] QJitRunner: Failed to compile value expression for "
                 "member assignment"
              << std::endl;
    return;
  }

  // GEP to member and store
  // instancePtr is already set above (either from local alloca or static
  // registry)

  llvm::Value *memberPtr = builder.CreateStructGEP(
      classInfo.structType, instancePtr, static_cast<unsigned>(memberIdx),
      instanceName + "." + memberName + ".ptr");

  builder.CreateStore(value, memberPtr);

  std::cout << "[DEBUG] QJitRunner: Member assignment " << instanceName << "."
            << memberName << " completed" << std::endl;
}

int QJitRunner::FindMemberIndex(const CompiledClass &classInfo,
                                const std::string &memberName) {

  for (size_t i = 0; i < classInfo.memberNames.size(); ++i) {
    if (classInfo.memberNames[i] == memberName) {
      return static_cast<int>(i);
    }
  }
  return -1; // Not found
}

// ============================================================================
// Main Entry Point
// ============================================================================

std::shared_ptr<QJitProgram>
QJitRunner::CompileProgram(std::shared_ptr<QProgram> program) {
  if (!program)
    return nullptr;

  std::cout << "[DEBUG] QJitRunner: Compiling program..." << std::endl;

  m_LocalVariables.clear();
  m_VariableTypes.clear();
  m_LoadedModules
      .clear(); // Ensure modules are re-linked into the new LLVM module

  // Automatically import all built modules to support the "Clean API"
  // persistence
  for (const auto &moduleName : m_AutoImportModules) {
    ImportModule(moduleName);
  }

  auto &context = QLVM::GetContext();
  auto &builder = QLVM::GetBuilder();
  auto *module = QLVM::GetModule();

  // Process imports first
  for (const auto &importName : program->GetImports()) {
    if (!ImportModule(importName)) {
      std::cerr << "[ERROR] QJitRunner: Failed to import module '" << importName
                << "'" << std::endl;
    }
  }

  llvm::FunctionType *entryType =
      llvm::FunctionType::get(builder.getVoidTy(), false);
  llvm::Function *entryFunc =
      llvm::Function::Create(entryType, llvm::Function::ExternalLinkage,
                             "__qlang_global_entry", module);

  llvm::BasicBlock *entryBlock =
      llvm::BasicBlock::Create(context, "entry", entryFunc);
  builder.SetInsertPoint(entryBlock);

  // Compile all class definitions first (before code uses them)
  for (const auto &classNode : program->GetClasses()) {
    CompileClass(classNode);
  }

  // Compile the main code block
  if (auto globalCode = program->GetCode()) {
    CompileCodeBlock(globalCode);
  }

  builder.CreateRetVoid();

  std::cout << "--- Generated LLVM IR ---" << std::endl;
  module->print(llvm::errs(), nullptr);
  std::cout << "-------------------------" << std::endl;

  std::string err;
  llvm::raw_string_ostream os(err);
  if (llvm::verifyModule(*module, &os)) {
    std::cerr << "[ERROR] QJitRunner: Module verification failed: " << os.str()
              << std::endl;
    return nullptr;
  }

  auto jitProgram = std::make_shared<QJitProgram>(QLVM::TakeModule());

  // Register all compiled classes with the JIT program for runtime
  // instantiation
  for (const auto &pair : m_CompiledClasses) {
    const std::string &className = pair.first;
    const CompiledClass &classInfo = pair.second;

    uint64_t size =
        module->getDataLayout().getTypeAllocSize(classInfo.structType);
    std::string ctorName = className + "_" + className;

    jitProgram->RegisterClass(className, classInfo.structType, size, ctorName,
                              classInfo.isStatic);

    // Register members with offset info for runtime get/set access
    const llvm::StructLayout *layout =
        module->getDataLayout().getStructLayout(classInfo.structType);
    for (size_t i = 0; i < classInfo.memberNames.size(); i++) {
      const std::string &memberName = classInfo.memberNames[i];
      size_t offset = layout->getElementOffset(i);
      size_t memberSize =
          module->getDataLayout().getTypeAllocSize(classInfo.memberTypes[i]);
      int typeToken = classInfo.memberTypeTokens[i];
      std::string typeName = i < classInfo.memberTypeNames.size()
                                 ? classInfo.memberTypeNames[i]
                                 : "";

      jitProgram->RegisterMember(className, memberName, offset, memberSize,
                                 typeToken, typeName);
    }
  }

  return jitProgram;
}

// ============================================================================
// Module System
// ============================================================================

bool QJitRunner::ImportModule(const std::string &moduleName) {
  // Check if already loaded
  if (m_LoadedModules.find(moduleName) != m_LoadedModules.end()) {
    std::cout << "[DEBUG] QJitRunner: Module '" << moduleName
              << "' already loaded" << std::endl;
    return true;
  }

  // Determine paths
  std::string basePath = m_BasePath.empty() ? "test" : m_BasePath;
  std::string sourcePath = basePath + "/" + moduleName + ".q";
  std::string binaryPath = basePath + "/" + moduleName + ".qm";

  // Check if binary exists and is up to date
  bool needsCompile = false;
  if (!std::filesystem::exists(binaryPath)) {
    std::cout << "[INFO] QJitRunner: Binary not found for '" << moduleName
              << "', compiling from source..." << std::endl;
    needsCompile = true;
  } else if (std::filesystem::exists(sourcePath)) {
    auto sourceTime = std::filesystem::last_write_time(sourcePath);
    auto binaryTime = std::filesystem::last_write_time(binaryPath);
    if (sourceTime > binaryTime) {
      std::cout << "[INFO] QJitRunner: Source file newer than binary for '"
                << moduleName << "', recompiling..." << std::endl;
      needsCompile = true;
    }
  }

  if (needsCompile) {
    if (!std::filesystem::exists(sourcePath)) {
      std::cerr << "[ERROR] QJitRunner: Cannot find source file: " << sourcePath
                << std::endl;
      return false;
    }
    if (!CompileModule(moduleName, sourcePath, binaryPath)) {
      std::cerr << "[ERROR] QJitRunner: Failed to compile module '"
                << moduleName << "'" << std::endl;
      return false;
    }
  }

  // Load the binary module
  QModuleFile moduleFile;
  std::unique_ptr<llvm::Module> loadedModule;
  std::vector<ModuleClassInfo> classes;

  if (!moduleFile.LoadModule(binaryPath, QLVM::GetContext(), loadedModule,
                             classes)) {
    std::cerr << "[ERROR] QJitRunner: Failed to load module: "
              << moduleFile.GetError() << std::endl;
    return false;
  }

  // Link module into current module
  LinkModuleInto(loadedModule.get(), QLVM::GetModule());

  // Register classes from the module
  std::cout << "[INFO] QJitRunner: Importing module '" << moduleName << "'"
            << std::endl;

  for (const auto &classInfo : classes) {
    // Create the compiled class entry
    CompiledClass cc;

    // Find the struct type in the linked module
    cc.structType = llvm::StructType::getTypeByName(QLVM::GetContext(),
                                                    classInfo.className);
    if (!cc.structType) {
      std::cerr << "[WARNING] QJitRunner: Struct type '" << classInfo.className
                << "' not found in module" << std::endl;
      continue;
    }

    // Populate member info
    cc.memberNames = classInfo.memberNames;
    cc.memberTypeTokens = classInfo.memberTypeTokens;
    cc.memberTypeNames = classInfo.memberTypeNames;
    cc.isStatic = classInfo.isStatic;

    // Get member types from the struct
    for (unsigned i = 0; i < cc.structType->getNumElements(); ++i) {
      cc.memberTypes.push_back(cc.structType->getElementType(i));
    }

    // Find methods
    for (const auto &methodName : classInfo.methodNames) {
      std::string fullName = classInfo.className + "_" + methodName;
      llvm::Function *func = QLVM::GetModule()->getFunction(fullName);
      if (func) {
        cc.methods[methodName] = func;
      }
    }
    cc.methodReturnTypes = classInfo.methodReturnTypes;

    m_CompiledClasses[classInfo.className] = cc;
    std::cout << "[DEBUG] QJitRunner: Registered class '" << classInfo.className
              << "' from module with " << cc.memberNames.size()
              << " members and " << cc.methods.size() << " methods"
              << std::endl;
  }

  m_LoadedModules.insert(moduleName);
  std::cout << "[INFO] QJitRunner: Imported " << classes.size()
            << " classes from module '" << moduleName << "'" << std::endl;
  return true;
}

bool QJitRunner::CompileModule(const std::string &moduleName,
                               const std::string &sourcePath,
                               const std::string &binaryPath) {
  // Read source file
  std::ifstream file(sourcePath);
  if (!file) {
    std::cerr << "[ERROR] QJitRunner: Cannot open source file: " << sourcePath
              << std::endl;
    return false;
  }
  std::string source((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
  file.close();

  // Tokenize
  Tokenizer tokenizer(source, true);
  tokenizer.Tokenize();
  std::vector<Token> tokens = tokenizer.GetTokens();

  // Parse
  Parser parser(tokens);
  auto moduleProgram = parser.Parse();
  if (!moduleProgram || parser.HasErrors()) {
    std::cerr
        << "[ERROR] QJitRunner: Failed to parse module source (has errors): "
        << sourcePath << std::endl;
    return false;
  }

  std::cout << "[INFO] QJitRunner: Compiling module '" << moduleName
            << "' to file " << binaryPath << std::endl;

  // Save current state and clear for isolation
  auto &builder = QLVM::GetBuilder();
  llvm::BasicBlock *oldBB = builder.GetInsertBlock();
  llvm::BasicBlock::iterator oldIP;
  if (oldBB)
    oldIP = builder.GetInsertPoint();

  auto oldModule = QLVM::TakeModule();
  auto oldCompiledClasses = m_CompiledClasses;
  m_CompiledClasses.clear();

  // Compile classes into the temporary LLVM module
  for (const auto &classNode : moduleProgram->GetClasses()) {
    CompileClass(classNode);
  }

  // Gather class metadata for serialization
  std::vector<ModuleClassInfo> classInfos;
  for (const auto &classNode : moduleProgram->GetClasses()) {
    std::string className = classNode->GetName();
    auto classIt = m_CompiledClasses.find(className);
    if (classIt == m_CompiledClasses.end())
      continue;

    ModuleClassInfo info;
    info.className = className;
    info.memberNames = classIt->second.memberNames;
    info.memberTypeTokens = classIt->second.memberTypeTokens;
    info.memberTypeNames = classIt->second.memberTypeNames;
    info.isStatic = classIt->second.isStatic;

    for (const auto &mp : classIt->second.methods) {
      info.methodNames.push_back(mp.first);
    }
    info.methodReturnTypes = classIt->second.methodReturnTypes;

    classInfos.push_back(info);
  }

  // Save to binary file
  QModuleFile moduleFile;
  bool success = moduleFile.SaveModule(moduleName, binaryPath,
                                       QLVM::GetModule(), classInfos);

  // Restore global state
  m_CompiledClasses = oldCompiledClasses;
  QLVM::SetModule(std::move(oldModule));
  if (oldBB)
    builder.SetInsertPoint(oldBB, oldIP);

  if (!success) {
    std::cerr << "[ERROR] QJitRunner: Failed to save module: "
              << moduleFile.GetError() << std::endl;
    return false;
  }

  return true;
}

void QJitRunner::LinkModuleInto(llvm::Module *srcModule,
                                llvm::Module *dstModule) {
  if (!srcModule || !dstModule)
    return;

  // Clone the source module before linking (linkModules takes ownership)
  if (llvm::Linker::linkModules(*dstModule, llvm::CloneModule(*srcModule))) {
    std::cerr << "[ERROR] QJitRunner: Failed to link modules" << std::endl;
  }
}

// Generate a universal wrapper function for dynamic method calling
// Wrapper signature: void ClassName_MethodName__wrap(void* this, void** args)
// The wrapper unpacks arguments from the void** array and calls the real method
void QJitRunner::GenerateMethodWrapper(const std::string &className,
                                       const std::string &methodName,
                                       llvm::Function *originalFunc,
                                       std::shared_ptr<QMethod> method) {
  if (!originalFunc || !method)
    return;

  auto &context = QLVM::GetContext();
  auto &builder = QLVM::GetBuilder();
  auto *module = QLVM::GetModule();

  std::string wrapperName = className + "_" + methodName + "__wrap";

  // Check if wrapper already exists
  if (module->getFunction(wrapperName))
    return;

  // Create wrapper function type: void(void*, void**)
  llvm::Type *voidPtrTy = llvm::PointerType::getUnqual(context);
  llvm::Type *voidPtrPtrTy = llvm::PointerType::getUnqual(context);

  std::vector<llvm::Type *> wrapperParams = {voidPtrTy, voidPtrPtrTy};
  llvm::FunctionType *wrapperType =
      llvm::FunctionType::get(builder.getVoidTy(), wrapperParams, false);

  llvm::Function *wrapperFunc = llvm::Function::Create(
      wrapperType, llvm::Function::ExternalLinkage, wrapperName, module);

  // Create entry block
  llvm::BasicBlock *entryBB =
      llvm::BasicBlock::Create(context, "entry", wrapperFunc);

  // Save current insert point
  auto savedInsertPoint = builder.GetInsertBlock();
  builder.SetInsertPoint(entryBB);

  // Get wrapper arguments
  auto argIt = wrapperFunc->arg_begin();
  llvm::Value *thisPtr = &*argIt;
  thisPtr->setName("this");
  argIt++;
  llvm::Value *argsArray = &*argIt;
  argsArray->setName("args");

  // Build call arguments: first is 'this', then unpacked args from array
  std::vector<llvm::Value *> callArgs;
  callArgs.push_back(thisPtr);

  // Unpack arguments from void** array based on the original function signature
  const auto &params = method->GetParameters();
  for (size_t i = 0; i < params.size(); i++) {
    // Get pointer to args[i]
    llvm::Value *idx = llvm::ConstantInt::get(builder.getInt64Ty(), i);
    llvm::Value *argSlotPtr = builder.CreateGEP(voidPtrTy, argsArray, idx);
    llvm::Value *argSlot = builder.CreateLoad(voidPtrTy, argSlotPtr);

    // Get the expected type for this parameter
    llvm::Type *paramType =
        GetLLVMType(static_cast<int>(params[i].type), params[i].typeName);
    if (!paramType)
      paramType = voidPtrTy;

    // Cast and load the value based on type
    if (paramType->isIntegerTy(32)) {
      // For int32: the void* slot contains the value directly (reinterpret as
      // int32)
      llvm::Value *asInt =
          builder.CreatePtrToInt(argSlot, builder.getInt64Ty());
      llvm::Value *truncated = builder.CreateTrunc(asInt, builder.getInt32Ty());
      callArgs.push_back(truncated);
    } else if (paramType->isIntegerTy(64)) {
      llvm::Value *asInt =
          builder.CreatePtrToInt(argSlot, builder.getInt64Ty());
      callArgs.push_back(asInt);
    } else if (paramType->isFloatTy()) {
      // For float: the void* slot contains a bitcast float
      llvm::Value *asInt =
          builder.CreatePtrToInt(argSlot, builder.getInt32Ty());
      llvm::Value *asFloat = builder.CreateBitCast(asInt, builder.getFloatTy());
      callArgs.push_back(asFloat);
    } else if (paramType->isDoubleTy()) {
      llvm::Value *asInt =
          builder.CreatePtrToInt(argSlot, builder.getInt64Ty());
      llvm::Value *asDouble =
          builder.CreateBitCast(asInt, builder.getDoubleTy());
      callArgs.push_back(asDouble);
    } else if (paramType->isPointerTy()) {
      // For pointers (including strings), use directly
      callArgs.push_back(argSlot);
    } else {
      // Default: use as pointer
      callArgs.push_back(argSlot);
    }
  }

  // Call the original method
  builder.CreateCall(originalFunc, callArgs);

  // Return void
  builder.CreateRetVoid();

  // Restore insert point
  if (savedInsertPoint) {
    builder.SetInsertPoint(savedInsertPoint);
  }

  std::cout << "[DEBUG] QJitRunner: Generated wrapper '" << wrapperName << "'"
            << std::endl;
}

std::shared_ptr<QJitProgram> QJitRunner::RunScript(const std::string &path) {
  // Tokenize from file
  Tokenizer tokenizer(path, m_ErrorCollector);
  tokenizer.Tokenize();

  if (m_ErrorCollector->HasErrors()) {
    std::cerr << "[ERROR] QJitRunner: Tokenization errors in " << path << ":"
              << std::endl;
    m_ErrorCollector->ListErrors();
    return nullptr;
  }

  // Parse
  Parser parser(tokenizer.GetTokens(), m_ErrorCollector);
  auto program = parser.Parse();

  if (m_ErrorCollector->HasErrors()) {
    std::cerr << "[ERROR] QJitRunner: Parse errors in " << path << ":"
              << std::endl;
    m_ErrorCollector->ListErrors();
    return nullptr;
  }

  // Create new module for this script (one script = one LLVM module)
  QLVM::CreateNewModule();

  // Reset context cache because the old module is gone, so cached Function* are
  // invalid
  m_LVMContext->ResetCache();

  // Compile
  return CompileProgram(program);
}

bool QJitRunner::BuildModule(const std::string &path) {
  std::filesystem::path p(path);
  std::string moduleName = p.stem().string();
  std::filesystem::path binaryPath = p;
  binaryPath.replace_extension(".qm");

  if (!CompileModule(moduleName, path, binaryPath.string())) {
    return false;
  }

  // After building, import it into the persistent registry so the C++ API
  // can see its classes even if other scripts don't explicitly import it.
  if (ImportModule(moduleName)) {
    m_AutoImportModules.insert(moduleName);
    return true;
  }
  return false;
}
