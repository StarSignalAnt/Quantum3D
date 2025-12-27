#include "QJitRunner.h"
#include "QAssign.h"
#include "QClass.h"
#include "QCode.h"
#include "QEnum.h"
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

  // Check for generic type parameter substitution (e.g., T -> int32)
  if (!typeName.empty() && !m_CurrentTypeMap.empty()) {
    auto subIt = m_CurrentTypeMap.find(typeName);
    if (subIt != m_CurrentTypeMap.end()) {
      std::string concreteType = subIt->second;
      std::cout << "[DEBUG] QJitRunner: Type substitution " << typeName
                << " -> " << concreteType << std::endl;

      // Map concrete type name to LLVM type
      if (concreteType == "int32") {
        return llvm::Type::getInt32Ty(context);
      } else if (concreteType == "int64") {
        return llvm::Type::getInt64Ty(context);
      } else if (concreteType == "float32") {
        return llvm::Type::getFloatTy(context);
      } else if (concreteType == "float64") {
        return llvm::Type::getDoubleTy(context);
      } else if (concreteType == "string") {
        return llvm::PointerType::getUnqual(context);
      } else if (concreteType == "bool") {
        return llvm::Type::getInt1Ty(context);
      } else if (concreteType == "byte") {
        return llvm::Type::getInt8Ty(context);
      } else if (concreteType == "iptr" || concreteType == "fptr" ||
                 concreteType == "bptr" || concreteType == "cptr") {
        return llvm::PointerType::getUnqual(context);
      }
      // Could be a class type
      auto classIt = m_CompiledClasses.find(concreteType);
      if (classIt != m_CompiledClasses.end()) {
        return llvm::PointerType::getUnqual(context);
      }
    }
  }

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
  case TokenType::T_IPTR:
    return llvm::PointerType::getUnqual(context); // int32* pointer
  case TokenType::T_FPTR:
    return llvm::PointerType::getUnqual(context); // float32* pointer
  case TokenType::T_BYTE:
    return llvm::Type::getInt8Ty(context); // unsigned 8-bit
  case TokenType::T_BPTR:
    return llvm::PointerType::getUnqual(context); // byte* pointer
  case TokenType::T_STRING_TYPE:
    return llvm::PointerType::getUnqual(context);
  case TokenType::T_IDENTIFIER:
    // This could be a class type - look it up
    // This could be a class type - look it up
    if (!typeName.empty()) {
      auto classIt = m_CompiledClasses.find(typeName);
      if (classIt != m_CompiledClasses.end()) {
        // Return pointer to the struct type for class return values
        return llvm::PointerType::getUnqual(context);
      }

      // AUTO-FORWARD DECLARATION SUPPORT
      // If we don't know the type, assume it's a forward declaration
      // and allow it as a generic pointer. We track the dependency.
      if (!m_CurrentScriptPath.empty()) {
        m_ScriptsPendingType[typeName].push_back(m_CurrentScriptPath);
        std::cout
            << "[INFO] QJitRunner: Deferring resolution for type '" << typeName
            << "' in "
            << std::filesystem::path(m_CurrentScriptPath).filename().string()
            << std::endl;
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

    // Check for .ToString() on integer literal: 12.ToString()
    if (pos < tokens.size() && tokens[pos].type == TokenType::T_DOT) {
      size_t savedPos = pos;
      pos++; // consume '.'
      if (pos < tokens.size() && tokens[pos].type == TokenType::T_IDENTIFIER &&
          tokens[pos].value == "ToString") {
        pos++; // consume 'ToString'
        if (pos < tokens.size() && tokens[pos].type == TokenType::T_LPAREN) {
          pos++; // consume '('
          if (pos < tokens.size() && tokens[pos].type == TokenType::T_RPAREN) {
            pos++; // consume ')'
            std::cout << "[DEBUG] QJitRunner: Integer.ToString() "
                      << token.value << " = \"" << token.value << "\""
                      << std::endl;
            return builder.CreateGlobalStringPtr(token.value);
          }
        }
      }
      pos = savedPos; // restore if not a valid ToString() call
    }

    if (expectedType && expectedType->isFloatingPointTy()) {
      return llvm::ConstantFP::get(expectedType, static_cast<double>(value));
    }
    llvm::Type *intType = expectedType ? expectedType : builder.getInt32Ty();
    return llvm::ConstantInt::get(intType, value);
  }

  case TokenType::T_FLOAT: {
    double value = std::stod(token.value);

    // Check for .ToString() on float literal: 3.14.ToString()
    if (pos < tokens.size() && tokens[pos].type == TokenType::T_DOT) {
      size_t savedPos = pos;
      pos++; // consume '.'
      if (pos < tokens.size() && tokens[pos].type == TokenType::T_IDENTIFIER &&
          tokens[pos].value == "ToString") {
        pos++; // consume 'ToString'
        if (pos < tokens.size() && tokens[pos].type == TokenType::T_LPAREN) {
          pos++; // consume '('
          if (pos < tokens.size() && tokens[pos].type == TokenType::T_RPAREN) {
            pos++; // consume ')'
            std::cout << "[DEBUG] QJitRunner: Float.ToString() " << token.value
                      << " = \"" << token.value << "\"" << std::endl;
            return builder.CreateGlobalStringPtr(token.value);
          }
        }
      }
      pos = savedPos;
    }

    if (expectedType && expectedType->isIntegerTy()) {
      return llvm::ConstantInt::get(expectedType, static_cast<int64_t>(value));
    }
    llvm::Type *floatType = expectedType ? expectedType : builder.getDoubleTy();
    return llvm::ConstantFP::get(floatType, value);
  }

  case TokenType::T_STRING:
    return builder.CreateGlobalStringPtr(token.value);

  case TokenType::T_TRUE: {
    // Check for .ToString(): true.ToString()
    if (pos < tokens.size() && tokens[pos].type == TokenType::T_DOT) {
      size_t savedPos = pos;
      pos++;
      if (pos < tokens.size() && tokens[pos].type == TokenType::T_IDENTIFIER &&
          tokens[pos].value == "ToString") {
        pos++;
        if (pos < tokens.size() && tokens[pos].type == TokenType::T_LPAREN) {
          pos++;
          if (pos < tokens.size() && tokens[pos].type == TokenType::T_RPAREN) {
            pos++;
            return builder.CreateGlobalStringPtr("true");
          }
        }
      }
      pos = savedPos;
    }
    return llvm::ConstantInt::getTrue(QLVM::GetContext());
  }

  case TokenType::T_FALSE: {
    // Check for .ToString(): false.ToString()
    if (pos < tokens.size() && tokens[pos].type == TokenType::T_DOT) {
      size_t savedPos = pos;
      pos++;
      if (pos < tokens.size() && tokens[pos].type == TokenType::T_IDENTIFIER &&
          tokens[pos].value == "ToString") {
        pos++;
        if (pos < tokens.size() && tokens[pos].type == TokenType::T_LPAREN) {
          pos++;
          if (pos < tokens.size() && tokens[pos].type == TokenType::T_RPAREN) {
            pos++;
            return builder.CreateGlobalStringPtr("false");
          }
        }
      }
      pos = savedPos;
    }
    return llvm::ConstantInt::getFalse(QLVM::GetContext());
  }

  case TokenType::T_NULL: {
    // Return a null pointer constant for class instance variables
    std::cout << "[DEBUG] QJitRunner: Null literal" << std::endl;
    return llvm::ConstantPointerNull::get(
        llvm::PointerType::getUnqual(QLVM::GetContext()));
  }

  case TokenType::T_NEW: {
    // Check for array allocation: new int32[N], new float32[N], new byte[N]
    if (pos < tokens.size() && (tokens[pos].type == TokenType::T_INT32 ||
                                tokens[pos].type == TokenType::T_FLOAT32 ||
                                tokens[pos].type == TokenType::T_BYTE)) {
      TokenType elemType = tokens[pos].type;
      pos++; // consume element type

      // Expect '['
      if (pos >= tokens.size() || tokens[pos].type != TokenType::T_LBRACKET) {
        std::cerr << "[ERROR] QJitRunner: Expected '[' after type in new"
                  << std::endl;
        return nullptr;
      }
      pos++; // consume '['

      // Parse array size (expect integer constant for now)
      if (pos >= tokens.size() || tokens[pos].type != TokenType::T_INTEGER) {
        std::cerr << "[ERROR] QJitRunner: Expected integer array size"
                  << std::endl;
        return nullptr;
      }
      int arraySize = std::stoi(tokens[pos].value);
      pos++; // consume size

      // Expect ']'
      if (pos >= tokens.size() || tokens[pos].type != TokenType::T_RBRACKET) {
        std::cerr << "[ERROR] QJitRunner: Expected ']' after array size"
                  << std::endl;
        return nullptr;
      }
      pos++; // consume ']'

      // Calculate element size
      int elemSize = (elemType == TokenType::T_BYTE) ? 1 : 4;
      int totalBytes = arraySize * elemSize;

      std::cout << "[DEBUG] QJitRunner: Allocating array: new "
                << (elemType == TokenType::T_INT32     ? "int32"
                    : elemType == TokenType::T_FLOAT32 ? "float32"
                                                       : "byte")
                << "[" << arraySize << "] (" << totalBytes << " bytes)"
                << std::endl;

      // Get or declare malloc
      llvm::Function *mallocFunc = m_LVMContext->GetLLVMFunc("malloc");
      if (!mallocFunc) {
        mallocFunc = QLVM::GetModule()->getFunction("malloc");
        if (!mallocFunc) {
          std::vector<llvm::Type *> args = {
              llvm::Type::getInt64Ty(QLVM::GetContext())};
          llvm::FunctionType *mallocType = llvm::FunctionType::get(
              llvm::PointerType::getUnqual(QLVM::GetContext()), args, false);
          mallocFunc = llvm::Function::Create(mallocType,
                                              llvm::Function::ExternalLinkage,
                                              "malloc", QLVM::GetModule());
        }
      }

      if (!mallocFunc) {
        std::cerr << "[ERROR] QJitRunner: malloc not available" << std::endl;
        return nullptr;
      }

      llvm::Value *sizeVal = llvm::ConstantInt::get(
          llvm::Type::getInt64Ty(QLVM::GetContext()), totalBytes);
      llvm::Value *ptr =
          builder.CreateCall(mallocFunc, {sizeVal}, "array.heap");

      return ptr;
    }

    // Class instantiation: new ClassName()
    if (pos >= tokens.size() || tokens[pos].type != TokenType::T_IDENTIFIER) {
      std::cerr << "[ERROR] QJitRunner: Expected class name after 'new'"
                << std::endl;
      return nullptr;
    }
    std::string className = tokens[pos].value;
    pos++; // consume class name

    // Parse constructor arguments
    std::vector<llvm::Value *> ctorArgs;
    std::vector<std::string> argTypeNames;

    if (pos < tokens.size() && tokens[pos].type == TokenType::T_LPAREN) {
      pos++; // consume '('

      // Parse arguments until closing paren
      while (pos < tokens.size() && tokens[pos].type != TokenType::T_RPAREN) {
        // Find end of this argument (comma or closing paren)
        size_t argStart = pos;
        int depth = 0;
        while (pos < tokens.size()) {
          if (tokens[pos].type == TokenType::T_LPAREN)
            depth++;
          else if (tokens[pos].type == TokenType::T_RPAREN) {
            if (depth == 0)
              break;
            depth--;
          } else if (tokens[pos].type == TokenType::T_COMMA && depth == 0)
            break;
          pos++;
        }

        // Extract argument tokens
        std::vector<Token> argTokens(tokens.begin() + argStart,
                                     tokens.begin() + pos);

        if (!argTokens.empty()) {
          // Create a temporary expression for this argument
          auto argExpr = std::make_shared<QExpression>();
          for (const auto &t : argTokens) {
            argExpr->AddElement(t);
          }

          llvm::Value *argVal = CompileExpression(argExpr);
          if (argVal) {
            ctorArgs.push_back(argVal);

            // Determine type name for mangling
            llvm::Type *argType = argVal->getType();
            if (argType->isFloatTy()) {
              argTypeNames.push_back("float32");
            } else if (argType->isDoubleTy()) {
              argTypeNames.push_back("float64");
            } else if (argType->isIntegerTy(32)) {
              argTypeNames.push_back("int32");
            } else if (argType->isIntegerTy(64)) {
              argTypeNames.push_back("int64");
            } else if (argType->isIntegerTy(1)) {
              argTypeNames.push_back("bool");
            } else if (argType->isPointerTy()) {
              argTypeNames.push_back("ptr");
            } else {
              argTypeNames.push_back("unknown");
            }
          }
        }

        // Skip comma if present
        if (pos < tokens.size() && tokens[pos].type == TokenType::T_COMMA) {
          pos++;
        }
      }

      // Consume closing paren
      if (pos < tokens.size() && tokens[pos].type == TokenType::T_RPAREN) {
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

    // Build mangled constructor name based on argument types
    std::string ctorName = className;
    if (!argTypeNames.empty()) {
      for (const auto &typeName : argTypeNames) {
        ctorName += "$" + typeName;
      }
    }

    // Try to find the constructor with matching signature
    std::string fullCtorName = className + "_" + ctorName;
    std::cout << "[DEBUG] T_NEW: Looking for constructor '" << fullCtorName
              << "'" << std::endl;
    llvm::Function *ctorFunc = QLVM::GetModule()->getFunction(fullCtorName);

    // If not found and we have float64 args, try with float32 (common case:
    // literals are double, params are float)
    if (!ctorFunc && !argTypeNames.empty()) {
      std::string altCtorName = className;
      for (const auto &typeName : argTypeNames) {
        if (typeName == "float64") {
          altCtorName += "$float32";
        } else {
          altCtorName += "$" + typeName;
        }
      }
      std::string altFullCtorName = className + "_" + altCtorName;
      std::cout << "[DEBUG] T_NEW: Trying float32 alternative '"
                << altFullCtorName << "'" << std::endl;
      ctorFunc = QLVM::GetModule()->getFunction(altFullCtorName);
      if (ctorFunc) {
        fullCtorName = altFullCtorName;
        std::cout << "[DEBUG] T_NEW: Found float32 alternative!" << std::endl;
      }
    }

    if (!ctorFunc) {
      // Fallback to default constructor if mangled not found
      fullCtorName = className + "_" + className;
      std::cout << "[DEBUG] T_NEW: Falling back to default constructor '"
                << fullCtorName << "'" << std::endl;
      ctorFunc = QLVM::GetModule()->getFunction(fullCtorName);
      // Clear args since we're using default constructor
      ctorArgs.clear();
    }

    if (ctorFunc) {
      std::vector<llvm::Value *> callArgs;
      callArgs.push_back(mallocPtr); // 'this' pointer

      // Only add constructor arguments if function can accept them
      size_t expectedArgs = ctorFunc->arg_size(); // includes 'this'

      for (size_t i = 0; i < ctorArgs.size() && i + 1 < expectedArgs; i++) {
        llvm::Value *arg = ctorArgs[i];

        // Type conversion if needed (e.g., double literal to float param)
        llvm::Type *paramType = ctorFunc->getArg(i + 1)->getType();
        if (paramType->isFloatTy() && arg->getType()->isDoubleTy()) {
          arg = builder.CreateFPTrunc(arg, builder.getFloatTy(), "fptrunc");
        } else if (paramType->isDoubleTy() && arg->getType()->isFloatTy()) {
          arg = builder.CreateFPExt(arg, builder.getDoubleTy(), "fpext");
        }
        callArgs.push_back(arg);
      }

      std::cout << "[TRACE] Calling constructor '" << fullCtorName << "' with "
                << callArgs.size() - 1 << " args" << std::endl;
      builder.CreateCall(ctorFunc, callArgs);
    } else {
      std::cerr << "[WARNING] Constructor '" << fullCtorName << "' not found"
                << std::endl;
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

      // Special case: primitive variable.ToString() - intercept before method
      // call handling
      if (memberName == "ToString" && pos < tokens.size() &&
          tokens[pos].type == TokenType::T_LPAREN) {
        // Check if this is a primitive variable (not a class instance)
        auto varIt = m_LocalVariables.find(varName);
        auto typeIt = m_VariableTypes.find(varName);

        // If variable exists and is NOT a class instance, handle ToString
        if (varIt != m_LocalVariables.end() &&
            (typeIt == m_VariableTypes.end() || typeIt->second == "iptr" ||
             typeIt->second == "fptr" || typeIt->second == "bptr")) {
          pos++; // consume '('
          if (pos < tokens.size() && tokens[pos].type == TokenType::T_RPAREN) {
            pos++; // consume ')'

            // Load the variable value
            llvm::Value *loadedVal = builder.CreateLoad(
                varIt->second->getAllocatedType(), varIt->second, varName);

            // Determine which helper to call based on variable type
            llvm::Type *varType = varIt->second->getAllocatedType();
            std::string helperName;
            llvm::Value *argVal = loadedVal;

            if (varType->isIntegerTy(32)) {
              helperName = "__int32_to_string";
            } else if (varType->isIntegerTy(64)) {
              helperName = "__int64_to_string";
            } else if (varType->isFloatTy()) {
              helperName = "__float32_to_string";
            } else if (varType->isDoubleTy()) {
              helperName = "__float64_to_string";
            } else if (varType->isIntegerTy(1)) {
              helperName = "__bool_to_string";
            } else if (varType->isIntegerTy(8)) {
              helperName = "__int32_to_string";
              argVal = builder.CreateZExt(loadedVal, builder.getInt32Ty());
            } else {
              std::cerr << "[ERROR] QJitRunner: ToString() not supported for "
                           "this type"
                        << std::endl;
              return nullptr;
            }

            llvm::Function *helperFunc = m_LVMContext->GetLLVMFunc(helperName);
            if (!helperFunc) {
              helperFunc = QLVM::GetModule()->getFunction(helperName);
            }

            if (helperFunc) {
              std::cout
                  << "[DEBUG] QJitRunner: PrimitiveVar.ToString() calling "
                  << helperName << std::endl;
              return builder.CreateCall(helperFunc, {argVal}, varName + ".str");
            } else {
              std::cerr << "[ERROR] QJitRunner: Helper function " << helperName
                        << " not found" << std::endl;
              return nullptr;
            }
          }
        }
      }

      // Special case: string variable.ToInt() / ToFloat() - convert string to
      // number
      if ((memberName == "ToInt" || memberName == "ToInt32" ||
           memberName == "ToInt64" || memberName == "ToFloat" ||
           memberName == "ToFloat32" || memberName == "ToFloat64") &&
          pos < tokens.size() && tokens[pos].type == TokenType::T_LPAREN) {
        auto varIt = m_LocalVariables.find(varName);
        auto typeIt = m_VariableTypes.find(varName);

        // Check if this is a string variable (pointer type in LLVM)
        if (varIt != m_LocalVariables.end() &&
            (typeIt == m_VariableTypes.end() || typeIt->second == "string")) {
          llvm::Type *varType = varIt->second->getAllocatedType();

          // String variables are stored as pointers (i8*)
          if (varType->isPointerTy()) {
            pos++; // consume '('
            if (pos < tokens.size() &&
                tokens[pos].type == TokenType::T_RPAREN) {
              pos++; // consume ')'

              // Load the string pointer
              llvm::Value *strPtr =
                  builder.CreateLoad(varType, varIt->second, varName);

              // Determine which helper to call based on method name
              std::string helperName;
              if (memberName == "ToInt" || memberName == "ToInt32") {
                helperName = "__string_to_int32";
              } else if (memberName == "ToInt64") {
                helperName = "__string_to_int64";
              } else if (memberName == "ToFloat" || memberName == "ToFloat32") {
                helperName = "__string_to_float32";
              } else if (memberName == "ToFloat64") {
                helperName = "__string_to_float64";
              }

              llvm::Function *helperFunc =
                  m_LVMContext->GetLLVMFunc(helperName);
              if (!helperFunc) {
                helperFunc = QLVM::GetModule()->getFunction(helperName);
              }

              if (helperFunc) {
                std::cout << "[DEBUG] QJitRunner: String." << memberName
                          << "() calling " << helperName << std::endl;
                return builder.CreateCall(helperFunc, {strPtr},
                                          varName + ".num");
              } else {
                std::cerr << "[ERROR] QJitRunner: Helper function "
                          << helperName << " not found" << std::endl;
                return nullptr;
              }
            }
          }
        }
      }

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

      // Check if varName is an ENUM (not a class or variable)
      auto enumIt = m_CompiledEnums.find(varName);
      if (enumIt != m_CompiledEnums.end()) {
        // This is an enum - look up the value
        auto valueIt = enumIt->second.find(memberName);
        if (valueIt != enumIt->second.end()) {
          int enumValue = valueIt->second;

          // Check for .ToString() chain: Api.OpenGL.ToString()
          if (pos < tokens.size() && tokens[pos].type == TokenType::T_DOT) {
            size_t savedPos = pos;
            pos++; // consume '.'
            if (pos < tokens.size() &&
                tokens[pos].type == TokenType::T_IDENTIFIER &&
                tokens[pos].value == "ToString") {
              pos++; // consume 'ToString'
              // Check for ()
              if (pos < tokens.size() &&
                  tokens[pos].type == TokenType::T_LPAREN) {
                pos++; // consume '('
                if (pos < tokens.size() &&
                    tokens[pos].type == TokenType::T_RPAREN) {
                  pos++; // consume ')'
                  std::cout << "[DEBUG] QJitRunner: Enum ToString " << varName
                            << "." << memberName << " = \"" << memberName
                            << "\"" << std::endl;
                  return builder.CreateGlobalStringPtr(memberName);
                }
              }
            }
            // Not a valid ToString() call, restore position
            pos = savedPos;
          }

          std::cout << "[DEBUG] QJitRunner: Enum access " << varName << "."
                    << memberName << " = " << enumValue << std::endl;
          return llvm::ConstantInt::get(builder.getInt32Ty(), enumValue);
        } else {
          std::cerr << "[ERROR] QJitRunner: Enum value '" << memberName
                    << "' not found in enum '" << varName << "'" << std::endl;
          return nullptr;
        }
      }

      // Look up instance (check local variables first, then class members)
      llvm::Value *instancePtr = nullptr;
      std::string className;

      auto varIt = m_LocalVariables.find(varName);
      if (varIt != m_LocalVariables.end()) {
        // Found as local variable - get class type
        auto typeIt = m_VariableTypes.find(varName);
        if (typeIt == m_VariableTypes.end()) {
          std::cerr << "[ERROR] QJitRunner: Variable '" << varName
                    << "' is not a class instance" << std::endl;
          return nullptr;
        }
        className = typeIt->second;

        // Load the instance pointer from the stack variable
        instancePtr = builder.CreateLoad(builder.getPtrTy(), varIt->second,
                                         varName + ".ptr");
      } else if (m_CurrentInstance && !m_CurrentClassName.empty()) {
        // Check if it's a member of the current class (implicit this.member
        // access)
        auto currentClassIt = m_CompiledClasses.find(m_CurrentClassName);
        if (currentClassIt != m_CompiledClasses.end()) {
          int outerMemberIdx = FindMemberIndex(currentClassIt->second, varName);
          if (outerMemberIdx >= 0) {
            // It's a class member! Load it from this->varName
            std::cout << "[DEBUG] QJitRunner: '" << varName
                      << "' is a class member of '" << m_CurrentClassName
                      << "', loading from this for member access ."
                      << memberName << std::endl;

            // Get the member's type name (this is the class type like "player")
            if (outerMemberIdx <
                static_cast<int>(
                    currentClassIt->second.memberTypeNames.size())) {
              className =
                  currentClassIt->second.memberTypeNames[outerMemberIdx];
            }

            // Load this->varName (which is a pointer to another class instance)
            llvm::Value *outerMemberPtr = builder.CreateStructGEP(
                currentClassIt->second.structType, m_CurrentInstance,
                static_cast<unsigned>(outerMemberIdx),
                "this." + varName + ".ptr");

            // Load the class instance pointer from the member
            instancePtr = builder.CreateLoad(builder.getPtrTy(), outerMemberPtr,
                                             "this." + varName);
          }
        }
      }

      if (!instancePtr || className.empty()) {
        std::cerr << "[ERROR] QJitRunner: Undefined variable: " << varName
                  << std::endl;
        return nullptr;
      }

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

      std::cout << "[DEBUG] QJitRunner: Accessing " << varName << "."
                << memberName << " - class '" << className
                << "' memberIdx=" << memberIdx << std::endl;

      // GEP to the target member and load
      llvm::Value *memberPtr = builder.CreateStructGEP(
          classInfo.structType, instancePtr, static_cast<unsigned>(memberIdx),
          varName + "." + memberName + ".ptr");

      return builder.CreateLoad(classInfo.memberTypes[memberIdx], memberPtr,
                                varName + "." + memberName);
    }

    // Regular variable access
    auto it = m_LocalVariables.find(varName);
    if (it != m_LocalVariables.end()) {
      // Check for indexed access: var[expr]
      if (pos < tokens.size() && tokens[pos].type == TokenType::T_LBRACKET) {
        pos++; // consume '['

        std::cout << "[DEBUG] QJitRunner: Parsing indexed access " << varName
                  << "[...]" << std::endl;

        // Parse index expression until ']'
        auto indexExpr = std::make_shared<QExpression>();
        int bracketDepth = 1;
        while (pos < tokens.size() && bracketDepth > 0) {
          if (tokens[pos].type == TokenType::T_LBRACKET) {
            bracketDepth++;
            indexExpr->AddElement(tokens[pos]);
          } else if (tokens[pos].type == TokenType::T_RBRACKET) {
            bracketDepth--;
            if (bracketDepth > 0) {
              indexExpr->AddElement(tokens[pos]);
            }
          } else {
            indexExpr->AddElement(tokens[pos]);
          }
          pos++;
        }

        // Load base pointer from alloca
        llvm::Value *basePtr =
            builder.CreateLoad(llvm::PointerType::getUnqual(QLVM::GetContext()),
                               it->second, varName + ".base");

        // Compile index expression
        llvm::Value *indexVal =
            CompileExpression(indexExpr, builder.getInt64Ty());
        if (!indexVal) {
          std::cerr << "[ERROR] QJitRunner: Failed to compile index expression"
                    << std::endl;
          return nullptr;
        }

        // Ensure index is i64 for GEP
        if (indexVal->getType()->isIntegerTy(32)) {
          indexVal = builder.CreateSExt(indexVal, builder.getInt64Ty());
        }

        // Determine element type based on pointer type (iptr->i32, fptr->float,
        // bptr->i8) Look up the pointer type from m_VariableTypes
        llvm::Type *elementType = builder.getInt32Ty(); // Default to int32
        std::string elemTypeName = "int32";
        auto typeIt = m_VariableTypes.find(varName);
        if (typeIt != m_VariableTypes.end()) {
          if (typeIt->second == "fptr") {
            elementType = builder.getFloatTy();
            elemTypeName = "float";
          } else if (typeIt->second == "bptr") {
            elementType = builder.getInt8Ty();
            elemTypeName = "byte";
          }
        }
        std::cout << "[DEBUG] QJitRunner: Indexed read element type: "
                  << elemTypeName << std::endl;

        // GEP to get element pointer
        llvm::Value *elemPtr = builder.CreateGEP(elementType, basePtr, indexVal,
                                                 varName + ".elem");

        // Load and return the value
        return builder.CreateLoad(elementType, elemPtr, varName + ".val");
      }

      // Check if this is a class instance OR a pointer type
      auto typeIt = m_VariableTypes.find(varName);
      if (typeIt != m_VariableTypes.end()) {
        // Check if it's a pointer type (iptr/fptr/bptr) - load the pointer
        // value
        if (typeIt->second == "iptr" || typeIt->second == "fptr" ||
            typeIt->second == "bptr") {
          // Load and return the pointer value from the alloca
          return builder.CreateLoad(
              llvm::PointerType::getUnqual(QLVM::GetContext()), it->second,
              varName + ".ptrval");
        }
        // It's a class instance - load the pointer value from the alloca
        // (not returning the alloca itself, which is the stack address)
        if (outClassName)
          *outClassName = typeIt->second;
        return builder.CreateLoad(
            llvm::PointerType::getUnqual(QLVM::GetContext()), it->second,
            varName + ".instanceptr");
      }
      // Primitive type - load the value
      llvm::Value *loadedVal = builder.CreateLoad(
          it->second->getAllocatedType(), it->second, varName);

      // Check for .ToString() on primitive variable
      if (pos < tokens.size() && tokens[pos].type == TokenType::T_DOT) {
        size_t savedPos = pos;
        pos++; // consume '.'
        if (pos < tokens.size() &&
            tokens[pos].type == TokenType::T_IDENTIFIER &&
            tokens[pos].value == "ToString") {
          pos++; // consume 'ToString'
          if (pos < tokens.size() && tokens[pos].type == TokenType::T_LPAREN) {
            pos++; // consume '('
            if (pos < tokens.size() &&
                tokens[pos].type == TokenType::T_RPAREN) {
              pos++; // consume ')'

              // Determine which helper to call based on variable type
              llvm::Type *varType = it->second->getAllocatedType();
              std::string helperName;
              llvm::Value *argVal = loadedVal;

              if (varType->isIntegerTy(32)) {
                helperName = "__int32_to_string";
              } else if (varType->isIntegerTy(64)) {
                helperName = "__int64_to_string";
              } else if (varType->isFloatTy()) {
                helperName = "__float32_to_string";
              } else if (varType->isDoubleTy()) {
                helperName = "__float64_to_string";
              } else if (varType->isIntegerTy(1)) {
                helperName = "__bool_to_string";
              } else if (varType->isIntegerTy(8)) {
                // Byte - treat as int32
                helperName = "__int32_to_string";
                argVal = builder.CreateZExt(loadedVal, builder.getInt32Ty());
              } else {
                std::cerr << "[ERROR] QJitRunner: ToString() not supported for "
                             "this type"
                          << std::endl;
                return nullptr;
              }

              // Get or declare the helper function
              llvm::Function *helperFunc =
                  m_LVMContext->GetLLVMFunc(helperName);
              if (!helperFunc) {
                helperFunc = QLVM::GetModule()->getFunction(helperName);
              }

              if (helperFunc) {
                std::cout << "[DEBUG] QJitRunner: Variable.ToString() calling "
                          << helperName << std::endl;
                return builder.CreateCall(helperFunc, {argVal},
                                          varName + ".str");
              } else {
                std::cerr << "[ERROR] QJitRunner: Helper function "
                          << helperName << " not found" << std::endl;
                return nullptr;
              }
            }
          }
        }
        pos = savedPos; // restore if not a valid ToString() call
      }

      return loadedVal;
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

            // Check if this is an indexed access on a pointer member (ages[0])
            if (pos < tokens.size() &&
                tokens[pos].type == TokenType::T_LBRACKET) {
              pos++; // consume '['

              // Parse index expression
              auto indexExpr = std::make_shared<QExpression>();
              int depth = 0;
              while (pos < tokens.size()) {
                if (tokens[pos].type == TokenType::T_LBRACKET)
                  depth++;
                else if (tokens[pos].type == TokenType::T_RBRACKET) {
                  if (depth == 0)
                    break;
                  depth--;
                }
                indexExpr->AddElement(tokens[pos]);
                pos++;
              }

              if (pos < tokens.size() &&
                  tokens[pos].type == TokenType::T_RBRACKET) {
                pos++; // consume ']'
              }

              std::cout << "[DEBUG] QJitRunner: Indexed read on member pointer "
                        << varName << std::endl;

              // Load the pointer from the member
              llvm::Value *basePtr = builder.CreateLoad(
                  llvm::PointerType::getUnqual(QLVM::GetContext()), memberPtr,
                  "this." + varName + ".base");

              // Compile index expression
              llvm::Value *indexVal =
                  CompileExpression(indexExpr, builder.getInt64Ty());
              if (!indexVal) {
                std::cerr << "[ERROR] QJitRunner: Failed to compile index"
                          << std::endl;
                return nullptr;
              }

              // Ensure index is i64 for GEP
              if (indexVal->getType()->isIntegerTy(32)) {
                indexVal = builder.CreateSExt(indexVal, builder.getInt64Ty());
              }

              // Determine element type based on member type token
              llvm::Type *elementType =
                  builder.getInt32Ty(); // Default to int32
              std::string elemTypeName = "int32";
              if (memberIdx <
                  static_cast<int>(classInfo.memberTypeTokens.size())) {
                int token = classInfo.memberTypeTokens[memberIdx];
                std::cout << "[DEBUG] QJitRunner: Member " << varName
                          << " type token = " << token
                          << " (T_IPTR=" << static_cast<int>(TokenType::T_IPTR)
                          << ", T_FPTR=" << static_cast<int>(TokenType::T_FPTR)
                          << ", T_BPTR=" << static_cast<int>(TokenType::T_BPTR)
                          << ")" << std::endl;
                if (token == static_cast<int>(TokenType::T_FPTR)) {
                  elementType = builder.getFloatTy();
                  elemTypeName = "float32";
                } else if (token == static_cast<int>(TokenType::T_BPTR)) {
                  elementType = builder.getInt8Ty();
                  elemTypeName = "byte";
                } else if (token == static_cast<int>(TokenType::T_IPTR)) {
                  elementType = builder.getInt32Ty();
                  elemTypeName = "int32";
                }
              }
              std::cout
                  << "[DEBUG] QJitRunner: Indexed read using element type: "
                  << elemTypeName << std::endl;

              // GEP to get element pointer
              llvm::Value *elemPtr = builder.CreateGEP(
                  elementType, basePtr, indexVal, "this." + varName + ".elem");

              // Load and return the value
              return builder.CreateLoad(elementType, elemPtr,
                                        "this." + varName + ".val");
            }

            // Simple member read (no index)
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

  // Promote if both are floating point but different precision (float32 <->
  // float64)
  if (left->getType()->isFloatTy() && right->getType()->isDoubleTy()) {
    left = builder.CreateFPExt(left, right->getType(), "fpext");
  } else if (left->getType()->isDoubleTy() && right->getType()->isFloatTy()) {
    right = builder.CreateFPExt(right, left->getType(), "fpext");
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
      // But allow comparison (== != for null checks) and string concat (+)
      if (result->getType()->isPointerTy() && right->getType()->isPointerTy()) {
        // Allow: + (string concat), == != (null/pointer comparison)
        if (op != "+" && op != "==" && op != "=" && op != "!=" && op != "<>") {
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
    } else if (expectedType->isPointerTy() && val->getType()->isPointerTy()) {
      // Cast pointer types (e.g. i8* from malloc to Class*)
      if (val->getType() != expectedType) {
        return builder.CreateBitCast(val, expectedType, "ptr_cast_tmp");
      }
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
    auto expr = varDecl->GetInitializer();
    const auto &elements = expr->GetElements();

    // Check for array allocation: new int32[N], new float32[N], or new byte[N]
    // Pattern: T_NEW, (T_INT32|T_FLOAT32|T_BYTE), T_LBRACKET, T_INTEGER,
    // T_RBRACKET
    if ((varDecl->GetVarType() == TokenType::T_IPTR ||
         varDecl->GetVarType() == TokenType::T_FPTR ||
         varDecl->GetVarType() == TokenType::T_BPTR) &&
        elements.size() >= 5 && elements[0].type == TokenType::T_NEW &&
        (elements[1].type == TokenType::T_INT32 ||
         elements[1].type == TokenType::T_FLOAT32 ||
         elements[1].type == TokenType::T_BYTE) &&
        elements[2].type == TokenType::T_LBRACKET &&
        elements[3].type == TokenType::T_INTEGER &&
        elements[4].type == TokenType::T_RBRACKET) {

      // Extract array size
      int arraySize = std::stoi(elements[3].value);

      // Determine element size: 1 for byte, 4 for int32/float32
      int elementSize = (elements[1].type == TokenType::T_BYTE) ? 1 : 4;
      std::string elementTypeName;
      if (elements[1].type == TokenType::T_INT32)
        elementTypeName = "int32";
      else if (elements[1].type == TokenType::T_FLOAT32)
        elementTypeName = "float32";
      else
        elementTypeName = "byte";

      int totalBytes = arraySize * elementSize;

      std::cout << "[DEBUG] QJitRunner: Allocating array: new "
                << elementTypeName << "[" << arraySize << "] (" << totalBytes
                << " bytes)" << std::endl;

      // Get or declare malloc function
      llvm::Function *mallocFunc = m_LVMContext->GetLLVMFunc("malloc");
      if (!mallocFunc) {
        mallocFunc = QLVM::GetModule()->getFunction("malloc");
        if (!mallocFunc) {
          std::vector<llvm::Type *> args = {
              llvm::Type::getInt64Ty(QLVM::GetContext())};
          llvm::FunctionType *mallocType = llvm::FunctionType::get(
              llvm::PointerType::getUnqual(QLVM::GetContext()), args, false);
          mallocFunc = llvm::Function::Create(mallocType,
                                              llvm::Function::ExternalLinkage,
                                              "malloc", QLVM::GetModule());
        }
      }

      if (mallocFunc) {
        llvm::Value *sizeVal = llvm::ConstantInt::get(
            llvm::Type::getInt64Ty(QLVM::GetContext()), totalBytes);
        llvm::Value *ptr =
            builder.CreateCall(mallocFunc, {sizeVal}, varName + ".ptr");
        builder.CreateStore(ptr, alloca);

        // Register the pointer type for indexed access (iptr, fptr, or bptr)
        std::string ptrType;
        if (varDecl->GetVarType() == TokenType::T_IPTR)
          ptrType = "iptr";
        else if (varDecl->GetVarType() == TokenType::T_FPTR)
          ptrType = "fptr";
        else
          ptrType = "bptr";
        m_VariableTypes[varName] = ptrType;

        std::cout << "[DEBUG] QJitRunner: Array allocated for '" << varName
                  << "' (type: " << ptrType << ")" << std::endl;
      } else {
        std::cerr << "[ERROR] QJitRunner: malloc not found for array allocation"
                  << std::endl;
      }

      return; // Array allocation handled, skip normal initializer processing
    }

    // Normal initializer processing
    llvm::Value *initValue =
        CompileExpression(varDecl->GetInitializer(), varType);
    if (initValue) {
      builder.CreateStore(initValue, alloca);

      // Deduce class name if unknown
      if (varDecl->GetVarType() == TokenType::T_UNKNOWN ||
          varDecl->GetVarType() == TokenType::T_IDENTIFIER) {
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
        // Compile arguments first to determine their types for overload
        // resolution
        std::vector<llvm::Value *> compiledArgs;
        if (auto params = stmt->GetParameters()) {
          const auto &exprs = params->GetParameters();
          for (size_t i = 0; i < exprs.size(); ++i) {
            llvm::Value *argValue = CompileExpression(exprs[i]);
            if (argValue) {
              compiledArgs.push_back(argValue);
            } else {
              std::cerr << "[ERROR] QJitRunner: Failed to compile argument "
                        << i << " for implicit method call " << funcName
                        << std::endl;
            }
          }
        }

        // Use FindMethodOverload to find the best matching overload
        llvm::Function *methodFunc =
            FindMethodOverload(classIt->second, funcName, compiledArgs);
        if (methodFunc) {
          std::cout << "[DEBUG] QJitRunner: Found implicit method call for "
                    << funcName << std::endl;

          // Build call arguments with 'this' pointer and type conversions
          std::vector<llvm::Value *> callArgs;
          callArgs.push_back(m_CurrentInstance); // this

          llvm::FunctionType *ft = methodFunc->getFunctionType();
          for (size_t i = 0; i < compiledArgs.size(); ++i) {
            llvm::Value *argValue = compiledArgs[i];

            // Apply type conversions if needed
            if (i + 1 < ft->getNumParams()) {
              llvm::Type *paramType =
                  ft->getParamType(static_cast<unsigned>(i + 1));

              // Integer type conversion
              if (paramType->isIntegerTy() &&
                  argValue->getType()->isIntegerTy()) {
                if (argValue->getType()->getIntegerBitWidth() <
                    paramType->getIntegerBitWidth()) {
                  argValue = builder.CreateSExt(argValue, paramType);
                } else if (argValue->getType()->getIntegerBitWidth() >
                           paramType->getIntegerBitWidth()) {
                  argValue = builder.CreateTrunc(argValue, paramType);
                }
              }
              // Float type conversion
              else if (paramType->isFloatingPointTy() &&
                       argValue->getType()->isFloatingPointTy()) {
                if (argValue->getType()->isFloatTy() &&
                    paramType->isDoubleTy()) {
                  argValue = builder.CreateFPExt(argValue, paramType);
                } else if (argValue->getType()->isDoubleTy() &&
                           paramType->isFloatTy()) {
                  argValue = builder.CreateFPTrunc(argValue, paramType);
                }
              }
              // Int to float
              else if (paramType->isFloatingPointTy() &&
                       argValue->getType()->isIntegerTy()) {
                argValue = builder.CreateSIToFP(argValue, paramType);
              }
              // Float to int
              else if (paramType->isIntegerTy() &&
                       argValue->getType()->isFloatingPointTy()) {
                argValue = builder.CreateFPToSI(argValue, paramType);
              }
            }

            callArgs.push_back(argValue);
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

      const auto &argTokens = exprs[i]->GetElements();
      if (!argTokens.empty()) {
        std::cout << "[DEBUG] Compiling Arg " << i
                  << " First Token: " << argTokens[0].value << std::endl;
      }

      llvm::Value *argValue = CompileExpression(exprs[i], paramType);
      if (argValue) {
        std::cout << "[DEBUG] Compiling Arg " << i
                  << " generated Value: " << argValue->getName().str() << " ("
                  << (void *)argValue << ")" << std::endl;

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

  if (auto enumNode = std::dynamic_pointer_cast<QEnum>(node)) {
    // Compile enum definition - store in m_CompiledEnums
    std::string enumName = enumNode->GetName();
    std::unordered_map<std::string, int> valueMap;
    for (const auto &valueName : enumNode->GetValues()) {
      valueMap[valueName] = enumNode->GetValueIndex(valueName);
    }
    m_CompiledEnums[enumName] = valueMap;
    std::cout << "[DEBUG] QJitRunner: Compiled inline enum '" << enumName
              << "' with " << valueMap.size() << " values" << std::endl;
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
  bool isRecompile = false;
  if (m_CompiledClasses.find(className) != m_CompiledClasses.end()) {
    std::cout << "[INFO] QJitRunner: Recompiling class '" << className << "'"
              << std::endl;
    isRecompile = true;
  }

  // Check if this is a generic class template (has type parameters)
  if (classNode->IsGeneric()) {
    std::cout << "[DEBUG] QJitRunner: Storing generic template '" << className
              << "' with " << classNode->GetTypeParameters().size()
              << " type parameters" << std::endl;
    m_GenericClassTemplates[className] = classNode;
    return; // Don't compile template directly, wait for specialization
  }

  // Handle inheritance - compile parent class first if it exists
  std::string parentClassName;
  if (classNode->HasParent()) {
    parentClassName = classNode->GetParentClassName();

    // Check if parent is compiled
    if (m_CompiledClasses.find(parentClassName) == m_CompiledClasses.end()) {
      std::cerr << "[ERROR] QJitRunner: Parent class '" << parentClassName
                << "' not compiled yet. Skipping '" << className << "'."
                << std::endl;
      return;
    }
  }

  CompiledClass classInfo;
  llvm::StructType *structType = nullptr;

  if (isRecompile) {
    // Reuse existing class info and struct type
    classInfo = m_CompiledClasses[className];
    structType = classInfo.structType;
  } else {
    // Get or create struct type (don't create duplicate with suffix like .1)
    structType = llvm::StructType::getTypeByName(context, className);
    if (!structType) {
      structType = llvm::StructType::create(context, className);
    }

    // Collect member types and names
    std::vector<llvm::Type *> memberTypes;
    std::vector<std::string> memberNames;
    std::vector<int> memberTypeTokens;
    std::vector<std::string> memberTypeNames;

    // First: include parent class members (if any)
    if (!parentClassName.empty()) {
      const CompiledClass &parentInfo = m_CompiledClasses[parentClassName];

      // Copy parent members first
      for (size_t i = 0; i < parentInfo.memberNames.size(); i++) {
        memberTypes.push_back(parentInfo.memberTypes[i]);
        memberNames.push_back(parentInfo.memberNames[i]);
        memberTypeTokens.push_back(parentInfo.memberTypeTokens[i]);
        memberTypeNames.push_back(parentInfo.memberTypeNames[i]);
      }
    }

    // Then: include child class's own members
    for (const auto &member : classNode->GetMembers()) {
      llvm::Type *memberType = GetLLVMType(
          static_cast<int>(member->GetVarType()), member->GetTypeName());
      if (memberType) {
        memberTypes.push_back(memberType);
        memberNames.push_back(member->GetName());
        memberTypeTokens.push_back(static_cast<int>(member->GetVarType()));
        memberTypeNames.push_back(member->GetTypeName());
      }
    }

    // Set struct body only if new
    if (structType->isOpaque()) {
      structType->setBody(memberTypes);
    }

    // Store in registry
    classInfo.structType = structType;
    classInfo.memberNames = memberNames;
    classInfo.memberTypes = memberTypes;
    classInfo.memberTypeTokens = memberTypeTokens;
    classInfo.memberTypeNames = memberTypeNames;
    classInfo.isStatic = classNode->IsStatic();
    classInfo.parentClassName = parentClassName; // Store inheritance info

    // Inherit parent methods if applicable
    if (!parentClassName.empty()) {
      const CompiledClass &parentInfo = m_CompiledClasses[parentClassName];
      for (const auto &method : parentInfo.methods) {
        classInfo.methods[method.first] = method.second;
      }
      for (const auto &retType : parentInfo.methodReturnTypes) {
        classInfo.methodReturnTypes[retType.first] = retType.second;
      }
    }

    m_CompiledClasses[className] = classInfo;
  }

  if (classNode->IsStatic()) {
    std::cout << "[DEBUG] QJitRunner: Class '" << className
              << "' is STATIC (singleton)" << std::endl;
  }

  std::cout << "[DEBUG] QJitRunner: Class '" << className << "' compiled with "
            << classInfo.memberTypes.size() << " members" << std::endl;

  // Pass 1: Create method prototypes
  auto *module = QLVM::GetModule();
  auto &builder = QLVM::GetBuilder();
  for (const auto &method : classNode->GetMethods()) {
    std::string methodName = method->GetName();

    // Generate mangled name for method overloading support
    std::string mangledName = MangleMethodName(methodName, method);
    std::string fullName = className + "_" + mangledName;

    std::vector<llvm::Type *> paramTypes;
    // Add 'this' pointer
    paramTypes.push_back(llvm::PointerType::getUnqual(
        classInfo.structType->getContext())); // this*

    for (const auto &param : method->GetParameters()) {
      llvm::Type *paramType =
          GetLLVMType(static_cast<int>(param.type), param.typeName);
      if (paramType) {
        paramTypes.push_back(paramType);
      } else {
        std::cerr << "[ERROR] QJitRunner: Failed to resolve parameter type '"
                  << param.typeName << "' for method '" << methodName << "'"
                  << std::endl;
        // Skip this method to avoid creating a malformed function
        continue;
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

    // Fix for recompilation: If function already exists and has a body,
    // we must clear it to avoid duplicate basic blocks or stale code execution.
    // preserving the Function* pointer keeps existing references/aliases valid.
    if (!func->empty()) {
      func->deleteBody();
    }

    // Check if this method overrides an inherited one (only for non-mangled
    // names) Overloading: same class, different signatures -> different mangled
    // names Overriding: child class redefines parent method -> same mangled
    // name exists in parent
    if (!parentClassName.empty() &&
        classInfo.methods.find(mangledName) != classInfo.methods.end()) {
      std::cout << "[DEBUG]   Method '" << methodName
                << "' OVERRIDES parent method" << std::endl;
    } else if (mangledName != methodName) {
      std::cout << "[DEBUG]   Method '" << methodName
                << "' overload registered as '" << mangledName << "'"
                << std::endl;
    }

    classInfo.methods[mangledName] = func;
    if (method->GetReturnType() == TokenType::T_IDENTIFIER) {
      classInfo.methodReturnTypes[mangledName] = method->GetReturnTypeName();
    }
  }

  // Update registry with prototypes
  m_CompiledClasses[className] = classInfo;

  // Pass 2: Compile method bodies
  for (const auto &method : classNode->GetMethods()) {
    CompileMethod(className, method);
  }

  // Pass 3: Create aliases for inherited methods that weren't overridden
  // This ensures CallMethod can find "Child_ParentMethod" resolving to
  // "Parent_ParentMethod"
  for (const auto &mp : classInfo.methods) {
    std::string mangledName = mp.first;
    llvm::Function *func = mp.second;
    std::string funcName = func->getName().str();

    // If function name doesn't start with current class name, it's inherited
    std::string prefix = className + "_";
    if (funcName.find(prefix) != 0) {
      // Inherited!
      std::string aliasName = prefix + mangledName;

      // Alias function (Mangled -> Mangled)
      if (!module->getNamedValue(aliasName)) {
        llvm::GlobalAlias::create(llvm::GlobalValue::ExternalLinkage, aliasName,
                                  func);
      }

      // Alias wrapper (Unmangled -> Unmangled)
      // Wrappers use UNMANGLED method names: Class_Method__wrap
      // We need to extract the unmangled method name from our local map key
      // (which matches function suffix)
      std::string unmangledMethodName = mangledName;
      size_t dollarPos = mangledName.find('$');
      if (dollarPos != std::string::npos) {
        unmangledMethodName = mangledName.substr(0, dollarPos);
      }

      if (!classInfo.parentClassName.empty()) {
        std::string parentWrap =
            classInfo.parentClassName + "_" + unmangledMethodName + "__wrap";
        std::string childWrap =
            className + "_" + unmangledMethodName + "__wrap";

        llvm::Function *pWrap = module->getFunction(parentWrap);
        if (pWrap && !module->getNamedValue(childWrap)) {
          llvm::GlobalAlias::create(llvm::GlobalValue::ExternalLinkage,
                                    childWrap, pWrap);
        }
      }
    }
  }
}

// ============================================================================
// Generic Class Specialization
// ============================================================================

std::string
QJitRunner::GetSpecializedClassName(const std::string &baseName,
                                    const std::vector<std::string> &typeArgs) {
  std::string result = baseName;
  for (const auto &arg : typeArgs) {
    result += "_" + arg;
  }
  return result;
}

void QJitRunner::CompileGenericClass(const std::string &baseName,
                                     std::shared_ptr<QClass> classTemplate,
                                     const std::vector<std::string> &typeArgs) {
  if (!classTemplate)
    return;

  std::string specializedName = GetSpecializedClassName(baseName, typeArgs);

  // Check if already compiled
  if (m_CompiledSpecializations.find(specializedName) !=
      m_CompiledSpecializations.end()) {
    std::cout << "[DEBUG] QJitRunner: Specialization '" << specializedName
              << "' already compiled" << std::endl;
    return;
  }

  std::cout << "[DEBUG] QJitRunner: Compiling generic specialization '"
            << specializedName << "'" << std::endl;

  m_CompiledSpecializations.insert(specializedName);

  // Build type parameter to concrete type mapping
  const auto &typeParams = classTemplate->GetTypeParameters();
  std::unordered_map<std::string, std::string> typeMap;
  for (size_t i = 0; i < typeParams.size() && i < typeArgs.size(); i++) {
    typeMap[typeParams[i]] = typeArgs[i];
    std::cout << "[DEBUG]   Type mapping: " << typeParams[i] << " -> "
              << typeArgs[i] << std::endl;
  }

  auto &context = QLVM::GetContext();

  // Create specialized struct type
  llvm::StructType *structType =
      llvm::StructType::create(context, specializedName);

  CompiledClass classInfo;
  classInfo.structType = structType;
  classInfo.isStatic = classTemplate->IsStatic();

  // Process members with type substitution
  std::vector<llvm::Type *> memberLLVMTypes;
  for (const auto &member : classTemplate->GetMembers()) {
    std::string memberTypeName = member->GetTypeName();
    TokenType memberType = member->GetVarType();

    // Check if member type is a type parameter that needs substitution
    if (typeMap.find(memberTypeName) != typeMap.end()) {
      std::string concreteType = typeMap[memberTypeName];
      std::cout << "[DEBUG]   Member '" << member->GetName()
                << "' type substitution: " << memberTypeName << " -> "
                << concreteType << std::endl;

      // Convert concrete type name to LLVM type
      llvm::Type *llvmType = nullptr;
      if (concreteType == "int32") {
        llvmType = llvm::Type::getInt32Ty(context);
        memberType = TokenType::T_INT32;
      } else if (concreteType == "int64") {
        llvmType = llvm::Type::getInt64Ty(context);
        memberType = TokenType::T_INT64;
      } else if (concreteType == "float32") {
        llvmType = llvm::Type::getFloatTy(context);
        memberType = TokenType::T_FLOAT32;
      } else if (concreteType == "float64") {
        llvmType = llvm::Type::getDoubleTy(context);
        memberType = TokenType::T_FLOAT64;
      } else if (concreteType == "string") {
        llvmType = llvm::PointerType::getUnqual(context);
        memberType = TokenType::T_STRING_TYPE;
      } else if (concreteType == "bool") {
        llvmType = llvm::Type::getInt1Ty(context);
        memberType = TokenType::T_BOOL;
      } else if (concreteType == "byte") {
        llvmType = llvm::Type::getInt8Ty(context);
        memberType = TokenType::T_BYTE;
      } else if (concreteType == "iptr") {
        llvmType = llvm::PointerType::getUnqual(context);
        memberType = TokenType::T_IPTR;
      } else if (concreteType == "fptr") {
        llvmType = llvm::PointerType::getUnqual(context);
        memberType = TokenType::T_FPTR;
      } else if (concreteType == "bptr") {
        llvmType = llvm::PointerType::getUnqual(context);
        memberType = TokenType::T_BPTR;
      } else if (concreteType == "cptr") {
        llvmType = llvm::PointerType::getUnqual(context);
        memberType = TokenType::T_CPTR;
      } else {
        // Assume it's a class type
        llvmType = llvm::PointerType::getUnqual(context);
        memberType = TokenType::T_IDENTIFIER;
      }

      memberLLVMTypes.push_back(llvmType);
      classInfo.memberTypes.push_back(llvmType);
      classInfo.memberTypeTokens.push_back(static_cast<int>(memberType));
      classInfo.memberTypeNames.push_back(concreteType);
    } else {
      // Not a type parameter, use original type
      llvm::Type *llvmType =
          GetLLVMType(static_cast<int>(memberType), memberTypeName);
      memberLLVMTypes.push_back(llvmType);
      classInfo.memberTypes.push_back(llvmType);
      classInfo.memberTypeTokens.push_back(static_cast<int>(memberType));
      classInfo.memberTypeNames.push_back(memberTypeName);
    }

    classInfo.memberNames.push_back(member->GetName());
    std::cout << "[DEBUG]   Specialized member: " << member->GetName()
              << std::endl;
  }

  // Set struct body
  structType->setBody(memberLLVMTypes);

  // Register the specialized class
  m_CompiledClasses[specializedName] = classInfo;

  // Set current type map for method compilation (for parameter type
  // substitution)
  m_CurrentTypeMap = typeMap;

  // Compile methods with specialized class name
  for (const auto &method : classTemplate->GetMethods()) {
    CompileMethod(specializedName, method);
  }

  // Clear type map after compilation
  m_CurrentTypeMap.clear();

  std::cout << "[DEBUG] QJitRunner: Specialization '" << specializedName
            << "' compiled with " << classInfo.memberNames.size() << " members"
            << std::endl;
}

void QJitRunner::CompileMethod(const std::string &className,
                               std::shared_ptr<QMethod> method) {
  if (!method)
    return;

  auto &context = QLVM::GetContext();
  auto &builder = QLVM::GetBuilder();
  auto *module = QLVM::GetModule();

  std::string methodName = method->GetName();

  // Generate mangled name for method overloading support
  std::string mangledName = MangleMethodName(methodName, method);
  std::string fullName = className + "_" + mangledName;

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
      classInfo.methodReturnTypes[mangledName] = method->GetReturnTypeName();
    }

    // Create function type and function
    llvm::FunctionType *funcType =
        llvm::FunctionType::get(returnType, paramTypes, false);
    func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                  fullName, module);
  } else {
    returnType = func->getReturnType();

    // Safety check: if the function already has basic blocks, it was already
    // compiled (e.g., from an imported module). Skip recompilation.
    if (!func->empty()) {
      std::cout << "[DEBUG] QJitRunner: Method '" << fullName
                << "' already has a body, skipping recompilation" << std::endl;
      return;
    }
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
    if (argIt == func->arg_end()) {
      std::cerr << "[CRITICAL ERROR] QJitRunner: Parameter count mismatch for "
                << fullName << std::endl;
      return;
    }
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

  // Auto-call parent constructor if this is a constructor method
  if (methodName == className && !classInfo.parentClassName.empty()) {
    std::string parentName = classInfo.parentClassName;
    auto parentClassIt = m_CompiledClasses.find(parentName);

    if (parentClassIt != m_CompiledClasses.end()) {
      const auto &parentInfo = parentClassIt->second;
      // Look for default constructor in parent
      // Use mangled name for constructor: ClassName_ClassName
      // But methods map uses mangled names. The default constructor mangled
      // name is usually just ClassName if no args, or ClassName_Args...
      // Actually, logic in CompileClass Pass 1 generates mangled names.
      // For default constructor: MangleMethodName("Parent", method) -> "Parent"
      // (if no args). Let's look up "ParentName" in parent methods.

      // We need to find the compatible parent constructor.
      // For now, we only support default constructor chaining.
      // TODO: Support super(...) calls.

      // Try to find default constructor "ParentName"
      // But wait, MangleMethodName might change it?
      // If Parent() has no args, mangled name is "Parent".
      // BUT strict lookup might be tricky.

      // Let's iterate parent methods to find one that matches ParentName and
      // has 0 args (except 'this')
      llvm::Function *parentCtor = nullptr;

      for (const auto &pm : parentInfo.methods) {
        // Check if this is the constructor (starts with ParentName)
        // And check arg count. 'this' is arg 0.
        if (pm.first.find(parentName) == 0) { // Simple check
          if (pm.second->arg_size() == 1) {   // Only 'this'
            parentCtor = pm.second;
            break;
          }
        }
      }

      if (parentCtor) {
        std::cout
            << "[DEBUG] QJitRunner: Injecting parent constructor call to '"
            << parentName << "'" << std::endl;
        // Cast 'this' to parent type
        // Parent type is struct pointer. Child struct includes Parent struct as
        // first member? Or bitcast? LLVM struct inheritance usually implies
        // pointer cast is safe if layout compatible. If we use Opaque Pointers,
        // no cast needed. If Typed Pointers, we need bitcast to Parent*.

        llvm::Value *parentThis = thisPtr;
        llvm::Type *parentPtrTy =
            llvm::PointerType::getUnqual(parentInfo.structType->getContext());
        if (thisPtr->getType() != parentPtrTy) {
          parentThis =
              builder.CreateBitCast(thisPtr, parentPtrTy, "parent.this");
        }

        builder.CreateCall(parentCtor, {parentThis});
      }
    }
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

  // Store method in class info using mangled name
  classInfo.methods[mangledName] = func;
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
        llvm::Value *memberPtr = builder.CreateStructGEP(
            classInfo.structType, m_CurrentInstance,
            static_cast<unsigned>(memberIdx), "this." + varName + ".ptr");

        // Check for indexed access on a pointer member (ages[0] = 25)
        if (assign->HasIndex()) {
          std::cout << "[DEBUG] QJitRunner: Member is indexed pointer access"
                    << std::endl;

          // Load the pointer from the member
          llvm::Value *basePtr = builder.CreateLoad(
              llvm::PointerType::getUnqual(QLVM::GetContext()), memberPtr,
              "this." + varName + ".base");

          // Compile the index expression
          llvm::Value *indexVal = CompileExpression(
              assign->GetIndexExpression(), builder.getInt64Ty());
          if (!indexVal) {
            std::cerr
                << "[ERROR] QJitRunner: Failed to compile index expression"
                << std::endl;
            return;
          }

          // Ensure index is i64 for GEP
          if (indexVal->getType()->isIntegerTy(32)) {
            indexVal = builder.CreateSExt(indexVal, builder.getInt64Ty());
          }

          // Determine element type based on member type name (look up from
          // memberTypeNames)
          llvm::Type *elementType = builder.getInt32Ty(); // Default to int32
          std::string elemTypeName = "int32";
          if (memberIdx < static_cast<int>(classInfo.memberTypeNames.size())) {
            std::string typeNameStr = classInfo.memberTypeNames[memberIdx];
            if (typeNameStr == "fptr" || typeNameStr == "float32") {
              elementType = builder.getFloatTy();
              elemTypeName = "float32";
            } else if (typeNameStr == "bptr" || typeNameStr == "byte") {
              elementType = builder.getInt8Ty();
              elemTypeName = "byte";
            }
          }
          // Also check memberTypeTokens
          if (memberIdx < static_cast<int>(classInfo.memberTypeTokens.size())) {
            int token = classInfo.memberTypeTokens[memberIdx];
            if (token == static_cast<int>(TokenType::T_FPTR)) {
              elementType = builder.getFloatTy();
              elemTypeName = "float32";
            } else if (token == static_cast<int>(TokenType::T_BPTR)) {
              elementType = builder.getInt8Ty();
              elemTypeName = "byte";
            }
          }

          std::cout << "[DEBUG] QJitRunner: Indexed member element type: "
                    << elemTypeName << std::endl;

          // Use GEP to get element pointer
          llvm::Value *elemPtr = builder.CreateGEP(
              elementType, basePtr, indexVal, "this." + varName + ".elem");

          // Compile the value expression
          llvm::Value *value = CompileExpression(assign->GetValueExpression());
          if (!value) {
            std::cerr
                << "[ERROR] QJitRunner: Failed to compile value for indexed "
                   "member assignment"
                << std::endl;
            return;
          }

          // Cast value to element type if needed
          if (value->getType()->isIntegerTy() &&
              elementType->isFloatingPointTy()) {
            value = builder.CreateSIToFP(value, elementType);
          } else if (value->getType()->isFloatingPointTy() &&
                     elementType->isIntegerTy()) {
            value = builder.CreateFPToSI(value, elementType);
          } else if (value->getType()->isFloatingPointTy() &&
                     elementType->isFloatingPointTy() &&
                     value->getType() != elementType) {
            // Float size conversion (double <-> float)
            if (value->getType()->isDoubleTy() && elementType->isFloatTy()) {
              value = builder.CreateFPTrunc(value, elementType);
            } else if (value->getType()->isFloatTy() &&
                       elementType->isDoubleTy()) {
              value = builder.CreateFPExt(value, elementType);
            }
          } else if (value->getType()->isIntegerTy() &&
                     elementType->isIntegerTy() &&
                     value->getType()->getIntegerBitWidth() !=
                         elementType->getIntegerBitWidth()) {
            if (value->getType()->getIntegerBitWidth() >
                elementType->getIntegerBitWidth()) {
              value = builder.CreateTrunc(value, elementType);
            } else {
              value = builder.CreateSExt(value, elementType);
            }
          }

          builder.CreateStore(value, elemPtr);
          return;
        }

        // Simple member assignment (no index)
        llvm::Value *value =
            CompileExpression(assign->GetValueExpression(), memberType);
        if (!value) {
          std::cerr << "[ERROR] QJitRunner: Failed to compile value for member "
                       "assignment"
                    << std::endl;
          return;
        }

        builder.CreateStore(value, memberPtr);
        return;
      }
    }
  }

  // Check for indexed assignment: ptr[index] = value
  if (assign->HasIndex()) {
    std::cout << "[DEBUG] QJitRunner: Compiling indexed assignment " << varName
              << "[...]" << std::endl;

    auto it = m_LocalVariables.find(varName);
    if (it == m_LocalVariables.end()) {
      std::cerr << "[ERROR] QJitRunner: Undefined variable for indexed assign: "
                << varName << std::endl;
      return;
    }

    llvm::AllocaInst *alloca = it->second;

    // Load the base pointer from the alloca (iptr/fptr store a pointer)
    llvm::Value *basePtr =
        builder.CreateLoad(llvm::PointerType::getUnqual(QLVM::GetContext()),
                           alloca, varName + ".base");

    // Compile the index expression
    llvm::Value *indexVal =
        CompileExpression(assign->GetIndexExpression(), builder.getInt64Ty());
    if (!indexVal) {
      std::cerr << "[ERROR] QJitRunner: Failed to compile index expression"
                << std::endl;
      return;
    }

    // Ensure index is i64 for GEP
    if (indexVal->getType()->isIntegerTy(32)) {
      indexVal = builder.CreateSExt(indexVal, builder.getInt64Ty());
    }

    // Determine element type based on pointer type (iptr->i32, fptr->float,
    // bptr->i8) Look up the pointer type from m_VariableTypes
    llvm::Type *elementType = builder.getInt32Ty(); // Default to int32
    std::string elemTypeName = "int32";
    auto typeIt = m_VariableTypes.find(varName);
    if (typeIt != m_VariableTypes.end()) {
      if (typeIt->second == "fptr") {
        elementType = builder.getFloatTy();
        elemTypeName = "float";
      } else if (typeIt->second == "bptr") {
        elementType = builder.getInt8Ty();
        elemTypeName = "byte";
      }
    }

    std::cout << "[DEBUG] QJitRunner: Indexed assign element type: "
              << elemTypeName << std::endl;

    // Use GEP to get element pointer
    llvm::Value *elemPtr =
        builder.CreateGEP(elementType, basePtr, indexVal, varName + ".elem");

    // Compile the value expression (don't pass expected type so we get the
    // natural type)
    llvm::Value *value = CompileExpression(assign->GetValueExpression());
    if (!value) {
      std::cerr << "[ERROR] QJitRunner: Failed to compile value for indexed "
                   "assignment"
                << std::endl;
      return;
    }

    // Automatic type conversion based on element type
    if (elemTypeName == "float" && value->getType()->isIntegerTy()) {
      // Converting int to float
      value = builder.CreateSIToFP(value, builder.getFloatTy(), "itof");
      std::cout << "[DEBUG] QJitRunner: Auto-cast int to float" << std::endl;
    } else if (elemTypeName != "float" &&
               value->getType()->isFloatingPointTy()) {
      // Converting float to int/byte
      value = builder.CreateFPToSI(value, elementType, "ftoi");
      std::cout << "[DEBUG] QJitRunner: Auto-cast float to int" << std::endl;
    } else if (elemTypeName == "byte" && value->getType()->isIntegerTy() &&
               value->getType()->getIntegerBitWidth() > 8) {
      // Truncate larger int to i8
      value = builder.CreateTrunc(value, builder.getInt8Ty(), "trunc8");
      std::cout << "[DEBUG] QJitRunner: Truncate to byte" << std::endl;
    } else if (elemTypeName == "int32" && value->getType()->isIntegerTy(8)) {
      // Extend i8 to i32 (zero-extend since byte is unsigned)
      value = builder.CreateZExt(value, builder.getInt32Ty(), "zext32");
      std::cout << "[DEBUG] QJitRunner: Zero-extend byte to int32" << std::endl;
    }

    builder.CreateStore(value, elemPtr);
    std::cout << "[DEBUG] QJitRunner: Indexed assignment complete" << std::endl;
    return;
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

  // Check for array initializer: ptr = {1, 2, 3, 4, 5}
  if (assign->HasArrayInitializer()) {
    std::cout << "[DEBUG] QJitRunner: Compiling array initializer with "
              << assign->GetArrayInitializer().size() << " elements"
              << std::endl;

    // Load the base pointer
    llvm::Value *basePtr =
        builder.CreateLoad(llvm::PointerType::getUnqual(QLVM::GetContext()),
                           alloca, varName + ".base");

    // Determine element type based on pointer type
    llvm::Type *elementType = builder.getInt32Ty(); // Default to int32
    auto typeIt = m_VariableTypes.find(varName);
    if (typeIt != m_VariableTypes.end()) {
      if (typeIt->second == "fptr") {
        elementType = builder.getFloatTy();
      } else if (typeIt->second == "bptr") {
        elementType = builder.getInt8Ty();
      }
    }

    // Store each element
    const auto &initExprs = assign->GetArrayInitializer();
    for (size_t i = 0; i < initExprs.size(); i++) {
      // Compile element value
      llvm::Value *elemVal = CompileExpression(initExprs[i], elementType);
      if (!elemVal) {
        std::cerr << "[ERROR] QJitRunner: Failed to compile array initializer "
                     "element "
                  << i << std::endl;
        continue;
      }

      // Cast if needed (e.g., int to float, double to float)
      if (elemVal->getType() != elementType) {
        if (elemVal->getType()->isIntegerTy() &&
            elementType->isFloatingPointTy()) {
          elemVal = builder.CreateSIToFP(elemVal, elementType);
        } else if (elemVal->getType()->isFloatingPointTy() &&
                   elementType->isIntegerTy()) {
          elemVal = builder.CreateFPToSI(elemVal, elementType);
        } else if (elemVal->getType()->isDoubleTy() &&
                   elementType->isFloatTy()) {
          elemVal = builder.CreateFPTrunc(elemVal, elementType);
        } else if (elemVal->getType()->isIntegerTy() &&
                   elementType->isIntegerTy() &&
                   elemVal->getType()->getIntegerBitWidth() !=
                       elementType->getIntegerBitWidth()) {
          if (elemVal->getType()->getIntegerBitWidth() >
              elementType->getIntegerBitWidth()) {
            elemVal = builder.CreateTrunc(elemVal, elementType);
          } else {
            elemVal = builder.CreateSExt(elemVal, elementType);
          }
        }
      }

      // Calculate element pointer
      llvm::Value *indexVal = llvm::ConstantInt::get(builder.getInt64Ty(), i);
      llvm::Value *elemPtr =
          builder.CreateGEP(elementType, basePtr, indexVal,
                            varName + ".elem" + std::to_string(i));

      // Store the value
      builder.CreateStore(elemVal, elemPtr);
    }

    std::cout << "[DEBUG] QJitRunner: Array initializer stored" << std::endl;
    return;
  }

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
  } else if (instanceName == "super") {
    // super::MethodName() - call parent class method with current instance
    instancePtr = m_CurrentInstance;
    // Get parent class name from current class
    auto classIt = m_CompiledClasses.find(m_CurrentClassName);
    if (classIt != m_CompiledClasses.end() &&
        !classIt->second.parentClassName.empty()) {
      className = classIt->second.parentClassName;
      std::cout << "[DEBUG] QJitRunner: super:: call to parent class '"
                << className << "'" << std::endl;
    } else {
      std::cerr << "[ERROR] QJitRunner: Cannot use super:: - class '"
                << m_CurrentClassName << "' has no parent" << std::endl;
      return nullptr;
    }
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

  // Compile arguments first to determine their types for overload resolution
  std::vector<llvm::Value *> compiledArgs;
  if (auto args = methodCall->GetArguments()) {
    const auto &params = args->GetParameters();
    for (size_t i = 0; i < params.size(); ++i) {
      llvm::Value *argVal = CompileExpression(params[i]);
      if (argVal) {
        compiledArgs.push_back(argVal);
      } else {
        std::cerr << "[ERROR] QJitRunner: Failed to compile argument " << i
                  << " for " << methodName << std::endl;
      }
    }
  }

  // Use FindMethodOverload to find the best matching overload
  llvm::Function *targetFunc =
      FindMethodOverload(classInfo, methodName, compiledArgs);
  if (!targetFunc) {
    std::cerr << "[ERROR] QJitRunner: Method '" << methodName
              << "' not found in class '" << className
              << "' (or no matching overload for " << compiledArgs.size()
              << " arguments)" << std::endl;
    return nullptr;
  }

  // Build final call arguments with 'this' pointer and type conversions
  std::vector<llvm::Value *> callArgs;
  callArgs.push_back(instancePtr); // 'this' pointer

  llvm::FunctionType *ft = targetFunc->getFunctionType();
  for (size_t i = 0; i < compiledArgs.size(); ++i) {
    llvm::Value *argVal = compiledArgs[i];

    // Apply type conversions if needed
    if (i + 1 < ft->getNumParams()) {
      llvm::Type *paramType = ft->getParamType(static_cast<unsigned>(i + 1));

      // Integer type conversion
      if (paramType->isIntegerTy() && argVal->getType()->isIntegerTy()) {
        if (argVal->getType()->getIntegerBitWidth() <
            paramType->getIntegerBitWidth()) {
          argVal = builder.CreateSExt(argVal, paramType);
        } else if (argVal->getType()->getIntegerBitWidth() >
                   paramType->getIntegerBitWidth()) {
          argVal = builder.CreateTrunc(argVal, paramType);
        }
      }
      // Float type conversion
      else if (paramType->isFloatingPointTy() &&
               argVal->getType()->isFloatingPointTy()) {
        if (argVal->getType()->isFloatTy() && paramType->isDoubleTy()) {
          argVal = builder.CreateFPExt(argVal, paramType);
        } else if (argVal->getType()->isDoubleTy() && paramType->isFloatTy()) {
          argVal = builder.CreateFPTrunc(argVal, paramType);
        }
      }
      // Int to float
      else if (paramType->isFloatingPointTy() &&
               argVal->getType()->isIntegerTy()) {
        argVal = builder.CreateSIToFP(argVal, paramType);
      }
      // Float to int
      else if (paramType->isIntegerTy() &&
               argVal->getType()->isFloatingPointTy()) {
        argVal = builder.CreateFPToSI(argVal, paramType);
      }
    }

    callArgs.push_back(argVal);
  }

  return builder.CreateCall(targetFunc, callArgs);
}

// ============================================================================
// Method Overloading Helpers
// ============================================================================

std::string
QJitRunner::MangleMethodName(const std::string &methodName,
                             const std::vector<std::string> &paramTypeNames) {
  if (paramTypeNames.empty()) {
    return methodName; // No parameters, use base name
  }

  std::string mangled = methodName;
  for (const auto &typeName : paramTypeNames) {
    mangled += "$" + typeName;
  }
  return mangled;
}

std::string QJitRunner::MangleMethodName(const std::string &methodName,
                                         std::shared_ptr<QMethod> method) {
  std::vector<std::string> paramTypeNames;
  for (const auto &param : method->GetParameters()) {
    paramTypeNames.push_back(param.typeName.empty()
                                 ? std::to_string(static_cast<int>(param.type))
                                 : param.typeName);
  }
  return MangleMethodName(methodName, paramTypeNames);
}

llvm::Function *
QJitRunner::FindMethodOverload(const CompiledClass &classInfo,
                               const std::string &methodName,
                               const std::vector<llvm::Value *> &args) {
  // First try exact match (for non-overloaded methods or if we have complete
  // type info)
  auto exactIt = classInfo.methods.find(methodName);
  if (exactIt != classInfo.methods.end()) {
    llvm::Function *func = exactIt->second;
    // Check argument count (subtract 1 for 'this' pointer in method calls)
    if (func->arg_size() == args.size() + 1) {
      return func;
    }
  }

  // Search for overloads by iterating through all methods
  // Methods are stored as "MethodName$type1$type2..." for overloads
  std::string prefix = methodName + "$";
  llvm::Function *bestMatch = nullptr;
  int bestScore = -1;

  for (const auto &pair : classInfo.methods) {
    // Check if this is an overload of our target method
    // Either exact match or starts with "MethodName$"
    bool isMatch = (pair.first == methodName) ||
                   (pair.first.length() > prefix.length() &&
                    pair.first.compare(0, prefix.length(), prefix) == 0);
    if (!isMatch) {
      continue;
    }
    llvm::Function *func = pair.second;

    // Check argument count (method has 'this' as first param)
    if (func->arg_size() != args.size() + 1) {
      continue;
    }

    // Score this overload based on type compatibility
    int score = 0;
    bool compatible = true;
    llvm::FunctionType *ft = func->getFunctionType();

    for (size_t i = 0; i < args.size() && compatible; ++i) {
      llvm::Type *paramType = ft->getParamType(static_cast<unsigned>(i + 1));
      llvm::Type *argType = args[i]->getType();

      if (paramType == argType) {
        score += 10; // Perfect match
      } else if (paramType->isFloatingPointTy() &&
                 argType->isFloatingPointTy()) {
        score += 5; // Float type conversion possible
      } else if (paramType->isIntegerTy() && argType->isIntegerTy()) {
        score += 5; // Integer type conversion possible
      } else if (paramType->isPointerTy() && argType->isPointerTy()) {
        score += 5; // Pointer types (class instances, etc.)
      } else {
        compatible = false;
      }
    }

    if (compatible && score > bestScore) {
      bestScore = score;
      bestMatch = func;
    }
  }

  return bestMatch;
}

llvm::Function *
QJitRunner::FindConstructor(const CompiledClass &classInfo,
                            const std::string &className,
                            const std::vector<llvm::Value *> &args) {
  // Use FindMethodOverload which handles the iteration through overloads
  // For constructors, the method name is the same as the class name
  llvm::Function *result = FindMethodOverload(classInfo, className, args);
  if (result) {
    return result;
  }

  // For specialized generic classes (e.g., Test_int32_string), try the base
  // class name (e.g., Test) as the constructor name
  size_t underscorePos = className.find('_');
  if (underscorePos != std::string::npos) {
    std::string baseName = className.substr(0, underscorePos);
    result = FindMethodOverload(classInfo, baseName, args);
    if (result) {
      return result;
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

  // Check if this is a generic class instantiation (e.g., Test<int32,string>)
  if (instDecl->HasTypeArguments()) {
    const auto &typeArgs = instDecl->GetTypeArguments();
    std::cout << "[DEBUG] QJitRunner: Instance has " << typeArgs.size()
              << " type arguments" << std::endl;

    // Look up generic template
    auto templateIt = m_GenericClassTemplates.find(className);
    if (templateIt != m_GenericClassTemplates.end()) {
      // Compile specialization if not already done
      CompileGenericClass(className, templateIt->second, typeArgs);

      // Use specialized class name
      className = GetSpecializedClassName(className, typeArgs);
      std::cout << "[DEBUG] QJitRunner: Using specialized class '" << className
                << "'" << std::endl;
    } else {
      std::cerr << "[ERROR] QJitRunner: Generic template '" << className
                << "' not found" << std::endl;
      return;
    }
  }

  // Find compiled class (now might be the specialized version)
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

  // Store instance alloca in local variables FIRST (needed for expressions)
  m_LocalVariables[instanceName] = ptrAlloca;
  m_VariableTypes[instanceName] = className;

  // Check if there's an initializer expression (e.g., = new TestClass() or =
  // null)
  if (auto initExpr = instDecl->GetInitializerExpression()) {
    std::cout << "[DEBUG] QJitRunner: Instance '" << instanceName
              << "' has initializer expression" << std::endl;
    llvm::Value *initValue = CompileExpression(initExpr, ptrType);
    if (initValue) {
      builder.CreateStore(initValue, ptrAlloca);
    }
    std::cout << "[DEBUG] QJitRunner: Instance '" << instanceName
              << "' initialized" << std::endl;
    return;
  }

  // Check if there's a "= new" constructor call (even with empty args)
  // GetConstructorArgs() returns non-null when "= new ClassName()" was parsed
  auto params = instDecl->GetConstructorArgs();
  bool hasNewExpression = (params != nullptr);

  std::cout << "[DEBUG] QJitRunner: Instance '" << instanceName
            << "' hasNewExpression=" << hasNewExpression << std::endl;

  // If no initializer and no "= new" expression, default to null
  if (!hasNewExpression) {
    std::cout << "[DEBUG] QJitRunner: Instance '" << instanceName
              << "' DEFAULTING TO NULL (no initializer, no args)" << std::endl;
    llvm::Value *nullPtr = llvm::ConstantPointerNull::get(
        llvm::PointerType::getUnqual(builder.getContext()));
    builder.CreateStore(nullPtr, ptrAlloca);
    std::cout << "[DEBUG] QJitRunner: Instance '" << instanceName
              << "' stored null and returning" << std::endl;
    return;
  }

  // Has constructor args - allocate memory and call constructor
  std::cout << "[DEBUG] QJitRunner: Instance '" << instanceName
            << "' allocating with constructor args" << std::endl;

  // Allocate memory on heap using malloc
  llvm::Function *mallocFunc = m_LVMContext->GetLLVMFunc("malloc");
  if (!mallocFunc) {
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

  std::cout << "[DEBUG] QJitRunner: Instance '" << instanceName << "' created"
            << std::endl;

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

  // Load instance pointer from alloca for constructor calls
  llvm::Value *currentInstancePtr =
      builder.CreateLoad(ptrType, ptrAlloca, instanceName + ".ptr");

  // Call parent constructor first (if class has a parent)
  if (!classInfo.parentClassName.empty()) {
    const std::string &parentName = classInfo.parentClassName;
    std::cout << "[DEBUG] QJitRunner: Calling parent constructor " << parentName
              << "_" << parentName << std::endl;
    auto parentMethIt = classInfo.methods.find(parentName);
    if (parentMethIt != classInfo.methods.end()) {
      builder.CreateCall(parentMethIt->second, {currentInstancePtr});
    } else {
      std::cerr << "[WARNING] Parent constructor '" << parentName
                << "' not found!" << std::endl;
    }
  }

  // Then call child's own constructor
  if (constructor) {
    std::cout << "[DEBUG] QJitRunner: Calling constructor " << className << "_"
              << className << std::endl;

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
QJitRunner::CompileProgram(std::shared_ptr<QProgram> program, bool accumulate) {
  if (!program)
    return nullptr;

  std::cout << "[DEBUG] QJitRunner: Compiling program..." << std::endl;

  m_LocalVariables.clear();
  m_VariableTypes.clear();
  m_LoadedModules
      .clear(); // Ensure modules are re-linked into the new LLVM module

  // Clear compiled classes ONLY if not accumulating
  if (!accumulate) {
    m_CompiledClasses.clear();
  }

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

  // Compile all enum definitions first (before code uses them)
  for (const auto &enumDef : program->GetEnums()) {
    if (enumDef) {
      std::string enumName = enumDef->GetName();
      std::unordered_map<std::string, int> valueMap;
      for (const auto &valueName : enumDef->GetValues()) {
        valueMap[valueName] = enumDef->GetValueIndex(valueName);
      }
      m_CompiledEnums[enumName] = valueMap;
      std::cout << "[DEBUG] QJitRunner: Compiled enum '" << enumName
                << "' with " << valueMap.size() << " values" << std::endl;
    }
  }

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

  // If we are accumulating, we don't want to take the module yet because
  // we want to keep adding to it. We mark it as needing recompile.
  if (accumulate) {
    m_MasterModuleNeedsRecompile = true;
    return nullptr;
  }

  const llvm::DataLayout &dataLayout = module->getDataLayout();
  auto jitProgram = std::make_shared<QJitProgram>(QLVM::TakeModule());

  // Register all compiled classes with the JIT program for runtime
  // instantiation
  for (const auto &pair : m_CompiledClasses) {
    const std::string &className = pair.first;
    const CompiledClass &classInfo = pair.second;

    uint64_t size = dataLayout.getTypeAllocSize(classInfo.structType);
    std::string ctorName = className + "_" + className;

    jitProgram->RegisterClass(className, classInfo.structType, size, ctorName,
                              classInfo.isStatic);

    // Register members with offset info for runtime get/set access
    const llvm::StructLayout *layout =
        dataLayout.getStructLayout(classInfo.structType);
    for (size_t i = 0; i < classInfo.memberNames.size(); i++) {
      const std::string &memberName = classInfo.memberNames[i];
      size_t offset = layout->getElementOffset(i);
      size_t memberSize = dataLayout.getTypeAllocSize(classInfo.memberTypes[i]);
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

bool QJitRunner::BuildModule(const std::string &path) {
  // Clear previous errors
  if (m_ErrorCollector) {
    m_ErrorCollector->ClearErrors();
  }

  // Tokenize
  Tokenizer tokenizer(path, m_ErrorCollector);
  tokenizer.Tokenize();

  if (m_ErrorCollector->HasErrors()) {
    std::cerr << "[ERROR] QJitRunner: Tokenization errors in " << path << ":"
              << std::endl;
    m_ErrorCollector->ListErrors();
    return false;
  }

  // Parse
  Parser parser(tokenizer.GetTokens(), m_ErrorCollector);
  auto program = parser.Parse();

  if (m_ErrorCollector->HasErrors()) {
    std::cerr << "[ERROR] QJitRunner: Parse errors in " << path << ":"
              << std::endl;
    m_ErrorCollector->ListErrors();
    return false;
  }

  // Compile into current modules (ACCUMULATE)
  // We pass 'true' to accumulate, so we don't clear m_CompiledClasses
  // In accumulation mode, CompileProgram returns nullptr if successful (doesn't
  // take module).
  CompileProgram(program, true);

  if (m_ErrorCollector->HasErrors()) {
    std::cerr << "[ERROR] QJitRunner: Compilation errors in " << path << ":"
              << std::endl;
    m_ErrorCollector->ListErrors();
    return false;
  }

  // Mark master module as needing recompile since we added new code
  m_MasterModuleNeedsRecompile = true;

  return true;
}

std::string QJitRunner::CompileScriptIntoMaster(const std::string &path) {
  m_CurrentScriptPath = path;
  bool success = BuildModule(path);
  m_CurrentScriptPath.clear();

  if (success) {
    // Extract class name from path stem
    std::filesystem::path p(path);
    std::string className = p.stem().string();

    // Check if any scripts were waiting for this type
    auto pendingIt = m_ScriptsPendingType.find(className);
    if (pendingIt != m_ScriptsPendingType.end()) {
      std::vector<std::string> dependents = pendingIt->second;
      m_ScriptsPendingType.erase(pendingIt); // Remove from pending list

      for (const auto &depPath : dependents) {
        std::cout << "[INFO] QJitRunner: Auto-recompiling dependent script: "
                  << std::filesystem::path(depPath).filename().string()
                  << std::endl;

        // Recompile dependent script
        m_CurrentScriptPath = depPath;
        BuildModule(depPath);
        m_CurrentScriptPath.clear();
      }
    }

    return className;
  }
  return "";
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

  // Save current state for the temporary compilation module
  auto &builder = QLVM::GetBuilder();
  llvm::BasicBlock *oldBB = builder.GetInsertBlock();
  llvm::BasicBlock::iterator oldIP;
  if (oldBB)
    oldIP = builder.GetInsertPoint();

  // Take the main module and create a fresh one for compilation
  auto oldModule = QLVM::TakeModule();

  // CRITICAL: Reset context cache immediately after TakeModule!
  // The cached LLVM function pointers (like qprintf) point to the OLD module.
  // With a fresh module, we need fresh function declarations.
  // GetLLVMFunc will recreate them on demand using the stored function types.
  m_LVMContext->ResetCache();

  // Save loaded modules - we need to clear this for the temp module compilation
  // so that ImportModule actually links (not just returns "already loaded")
  auto oldLoadedModules = m_LoadedModules;
  m_LoadedModules.clear();

  // IMPORTANT: Save m_CompiledClasses - we'll use this for type lookup during
  // compilation, but the original entries point to the MAIN module's types,
  // not the temp module's. We need to clear and re-import for this compilation.
  auto savedCompiledClasses = m_CompiledClasses;
  m_CompiledClasses.clear();

  // Process imports BEFORE compiling classes
  // This links dependencies into the TEMP module and creates their types there
  for (const auto &importName : moduleProgram->GetImports()) {
    std::cout << "[DEBUG] QJitRunner: Module '" << moduleName
              << "' importing: " << importName << std::endl;

    if (!ImportModule(importName)) {
      std::cerr << "[ERROR] QJitRunner: Failed to import module '" << importName
                << "' for module '" << moduleName << "'. Aborting."
                << std::endl;
      return false;
    }
  }

  // Compile classes into the temporary LLVM module
  for (const auto &classNode : moduleProgram->GetClasses()) {
    CompileClass(classNode);
  }

  // Gather class metadata for serialization - ONLY for classes defined in this
  // module
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

  // Restore LLVM module and builder state
  QLVM::SetModule(std::move(oldModule));
  if (oldBB)
    builder.SetInsertPoint(oldBB, oldIP);

  // Reset context cache - cached LLVM function pointers from the temp module
  // are now invalid. The context will recreate them on demand for the main
  // module.
  m_LVMContext->ResetCache();

  // Restore m_LoadedModules so subsequent BuildModule calls don't skip imports
  m_LoadedModules = oldLoadedModules;

  // CRITICAL: Restore m_CompiledClasses - the temp module types are now invalid
  // The ImportModule call in BuildModule will properly re-register classes
  // with types from the MAIN module
  m_CompiledClasses = savedCompiledClasses;

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
  // Use OverrideFromSrc flag to allow overwriting existing symbols
  // This is needed because compiled modules may include their dependencies,
  // and those dependencies may already exist in the destination module
  unsigned flags = llvm::Linker::Flags::OverrideFromSrc;
  if (llvm::Linker::linkModules(*dstModule, llvm::CloneModule(*srcModule),
                                flags)) {
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
  auto program_result = CompileProgram(program);
  if (!program_result) {
    std::cerr << "[ERROR] QJitRunner: Compilation failed" << std::endl;
    return nullptr;
  }

  // Execute top-level code if compilation succeeded
  if (program_result) {
    program_result->Run();
  }

  return program_result;
}

std::shared_ptr<QJitProgram> QJitRunner::GetMasterProgram() {
  // If we have a valid master program and no changes, return it
  if (m_MasterProgram && !m_MasterModuleNeedsRecompile) {
    return m_MasterProgram;
  }

  std::cout << "[INFO] QJitRunner: Building master program..." << std::endl;

  auto *module = QLVM::GetModule();
  if (!module) {
    std::cerr << "[ERROR] QJitRunner: No module available" << std::endl;
    return nullptr;
  }

  // Verify the module
  std::string errorStr;
  llvm::raw_string_ostream os(errorStr);
  if (llvm::verifyModule(*module, &os)) {
    std::cerr << "[ERROR] QJitRunner: Master module verification failed: "
              << os.str() << std::endl;
    return nullptr;
  }

  // Capture DataLayout
  const llvm::DataLayout &dataLayout = module->getDataLayout();

  // Create the master program with a CLONE of the accumulated module
  // This keeps the original module in QLVM for future script accumulation
  m_MasterProgram = std::make_shared<QJitProgram>(llvm::CloneModule(*module));

  // Register all compiled classes with the master program
  for (const auto &pair : m_CompiledClasses) {
    const std::string &clsName = pair.first;
    const CompiledClass &classInfo = pair.second;

    uint64_t size = dataLayout.getTypeAllocSize(classInfo.structType);
    std::string ctorName = clsName + "_" + clsName;

    m_MasterProgram->RegisterClass(clsName, classInfo.structType, size,
                                   ctorName, classInfo.isStatic);

    // Register members with offset info
    const llvm::StructLayout *layout =
        dataLayout.getStructLayout(classInfo.structType);
    for (size_t i = 0; i < classInfo.memberNames.size(); i++) {
      const std::string &memberName = classInfo.memberNames[i];
      size_t offset = layout->getElementOffset(i);
      size_t memberSize = dataLayout.getTypeAllocSize(classInfo.memberTypes[i]);
      int typeToken = classInfo.memberTypeTokens[i];
      std::string typeName = i < classInfo.memberTypeNames.size()
                                 ? classInfo.memberTypeNames[i]
                                 : "";

      m_MasterProgram->RegisterMember(clsName, memberName, offset, memberSize,
                                      typeToken, typeName);
    }
  }

  // Reset the recompile flag
  m_MasterModuleNeedsRecompile = false;

  std::cout << "[INFO] QJitRunner: Master program updated from module snapshot"
            << std::endl;

  // Set this as the global instance for QJClassInstance to use
  QJitProgram::SetInstance(m_MasterProgram.get());

  return m_MasterProgram;
}
