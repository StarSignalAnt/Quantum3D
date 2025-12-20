#pragma once

#include <QList>
#include <QMap>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace Quantum {

// Represents a symbol in the code
struct QLangSymbol {
  QString name;
  QString symbolType;   // "method", "member", "local", "parameter", "class"
  QString dataType;     // The type (e.g., "int32", "Vec3", "void")
  QString parentClass;  // Which class this belongs to
  QString parentMethod; // Which method this belongs to (for locals/params)
  int line = 0;
};

// Represents an external class definition (from compiled QLang or engine)
struct QLangClassDef {
  QString name;
  QString parentClass;
  QStringList members;                // Member variable names
  QStringList methods;                // Method names
  QMap<QString, QString> memberTypes; // member name -> type
};

// Collects and provides symbols from QLang source code
class QLangSymbolCollector {
public:
  QLangSymbolCollector() = default;

  // Parse source code and extract symbols
  void parse(const QString &source);

  // Get all class names (including external)
  QStringList getClassNames() const;

  // Get methods for a class
  QList<QLangSymbol> getClassMethods(const QString &className) const;

  // Get member variables for a class
  QList<QLangSymbol> getClassMembers(const QString &className) const;

  // Get local variables for a method
  QList<QLangSymbol> getLocalVariables(const QString &className,
                                       const QString &methodName) const;

  // Get parameters for a method
  QList<QLangSymbol> getMethodParameters(const QString &className,
                                         const QString &methodName) const;

  // Get the type of a variable given context
  QString getVariableType(const QString &varName, const QString &className,
                          const QString &methodName) const;

  // Get all symbols (for debugging)
  const QList<QLangSymbol> &getAllSymbols() const { return m_symbols; }

  // Get QLang keywords
  static QStringList getKeywords();

  // Get QLang type keywords
  static QStringList getTypeKeywords();

  // === External Class Support ===

  // Register an external class definition (from compiled code)
  void registerExternalClass(const QLangClassDef &classDef);

  // Clear all external class definitions
  void clearExternalClasses();

  // Get all members and methods for a type (for dot-completion)
  QStringList getMembersForType(const QString &typeName) const;

  // Completion item with type info
  struct CompletionItem {
    QString name;
    bool isMethod; // true = method, false = member variable
  };

  // Get typed completions for a type (members first, then methods)
  QList<CompletionItem> getTypedMembersForType(const QString &typeName) const;

  // Check if a type is known
  bool isKnownType(const QString &typeName) const;

private:
  QList<QLangSymbol> m_symbols;
  QStringList m_classNames;
  QMap<QString, QLangClassDef> m_externalClasses;

  void parseClasses(const QString &source);
  void parseMethods(const QString &source, const QString &className,
                    int classStart, int classEnd);
  void parseMembers(const QString &source, const QString &className,
                    int classStart, int classEnd);
  void parseLocals(const QString &source, const QString &className,
                   const QString &methodName, int methodStart, int methodEnd);
  void parseParameters(const QString &source, const QString &className,
                       const QString &methodName, int methodStart);
};

} // namespace Quantum
