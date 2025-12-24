#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "QJClassInstance.h"

// Forward declarations
class QProgram;
class QCode;
class QNode;
class QStatement;
class QVariableDecl;
class QExpression;
class QFor;
class QIf;
class QClass;
class QInstanceDecl;
class QMemberAssign;
class QAssign;
class QMethod;
class QReturn;
class QMethodCall;
class QLVMContext;
class QErrorCollector;
class QJitProgram;
struct Token;

namespace llvm {
class Value;
class Type;
class StructType;
class AllocaInst;
class Function;
class LLVMContext;
class Module;
} // namespace llvm

// Compiled class information for JIT
struct CompiledClass {
  llvm::StructType *structType;
  std::vector<std::string> memberNames;
  std::vector<llvm::Type *> memberTypes;
  std::vector<int> memberTypeTokens;        // For serialization
  std::vector<std::string> memberTypeNames; // For serialization
  std::unordered_map<std::string, llvm::Function *> methods;
  std::unordered_map<std::string, std::string>
      methodReturnTypes; // For chained ops
  bool isStatic = false; // True if this is a static class (singleton)
};

class QJitRunner {
public:
  QJitRunner(std::shared_ptr<QLVMContext> lvmContext,
             std::shared_ptr<QErrorCollector> errorCollector);
  ~QJitRunner();

  // Compiles a QProgram into LLVM IR and returns a JIT program
  std::shared_ptr<QJitProgram>
  CompileProgram(std::shared_ptr<QProgram> program);

  // Set base path for module resolution
  void SetBasePath(const std::string &basePath) { m_BasePath = basePath; }

  // High-level API
  std::shared_ptr<QJitProgram> RunScript(const std::string &path);
  bool BuildModule(const std::string &path);

  // Module system
  bool ImportModule(const std::string &moduleName);

public:
  bool CompileModule(const std::string &moduleName,
                     const std::string &sourcePath,
                     const std::string &binaryPath);

private:
  std::shared_ptr<QLVMContext> m_LVMContext;
  std::shared_ptr<QErrorCollector> m_ErrorCollector;

  // Base path for module resolution
  std::string m_BasePath = "test";

  // Variable storage for current scope
  std::unordered_map<std::string, llvm::AllocaInst *> m_LocalVariables;

  // Track variable types for class instances
  std::unordered_map<std::string, std::string>
      m_VariableTypes; // varName -> className

  // Compiled class registry
  std::unordered_map<std::string, CompiledClass> m_CompiledClasses;

  // Loaded modules (to prevent double loading in the current LLVM module)
  std::unordered_set<std::string> m_LoadedModules;

  // Modules that should be automatically imported into every new program
  std::unordered_set<std::string> m_AutoImportModules;

  // Method context - for implicit member access (this pointer)
  llvm::Value *m_CurrentInstance = nullptr;
  std::string m_CurrentClassName;

  // Reusable compilation methods
  void CompileCodeBlock(std::shared_ptr<QCode> code);
  void CompileNode(std::shared_ptr<QNode> node);
  void CompileStatement(std::shared_ptr<QStatement> stmt);
  void CompileVariableDecl(std::shared_ptr<QVariableDecl> varDecl);
  void CompileForLoop(std::shared_ptr<QFor> forNode);
  void CompileIf(std::shared_ptr<QIf> ifNode);

  // Class compilation
  void CompileClass(std::shared_ptr<QClass> classNode);
  void CompileInstanceDecl(std::shared_ptr<QInstanceDecl> instDecl);
  void CompileMemberAssign(std::shared_ptr<QMemberAssign> memberAssign);
  void CompileMethod(const std::string &className,
                     std::shared_ptr<QMethod> method);
  void CompileAssign(std::shared_ptr<QAssign> assign);
  void CompileReturn(std::shared_ptr<QReturn> returnNode);

  llvm::Value *CompileMethodCall(std::shared_ptr<QMethodCall> methodCall);

  // Expression evaluation - main entry point
  llvm::Value *CompileExpression(std::shared_ptr<QExpression> expr,
                                 llvm::Type *expectedType = nullptr,
                                 std::string *outClassName = nullptr);

  // Recursive expression parsing helpers
  llvm::Value *CompilePrimaryExpr(const std::vector<Token> &tokens, size_t &pos,
                                  llvm::Type *expectedType,
                                  std::string *outClassName = nullptr);
  llvm::Value *CompileExprTokens(const std::vector<Token> &tokens, size_t &pos,
                                 llvm::Type *expectedType,
                                 std::string *outClassName = nullptr);
  llvm::Value *CompileExprTokensRecursive(const std::vector<Token> &tokens,
                                          size_t &pos, llvm::Type *expectedType,
                                          int minPrecedence,
                                          std::string *outClassName = nullptr);
  llvm::Value *ApplyBinaryOp(const std::string &op, llvm::Value *left,
                             llvm::Value *right);

  // Precedence helper
  int GetOperatorPrecedence(const std::string &op);

  // Type mapping helper
  llvm::Type *GetLLVMType(int tokenType, const std::string &typeName = "");

  // Class helpers
  int FindMemberIndex(const CompiledClass &classInfo,
                      const std::string &memberName);
  llvm::Function *FindConstructor(const CompiledClass &classInfo,
                                  const std::string &className,
                                  const std::vector<llvm::Value *> &args);

  void LinkModuleInto(llvm::Module *srcModule, llvm::Module *dstModule);

  // Generate wrapper function for dynamic method calling
  void GenerateMethodWrapper(const std::string &className,
                             const std::string &methodName,
                             llvm::Function *originalFunc,
                             std::shared_ptr<QMethod> method);
};
