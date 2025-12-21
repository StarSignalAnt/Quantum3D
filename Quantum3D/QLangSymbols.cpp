#include "QLangSymbols.h"
#include <QTextStream>

namespace Quantum {

QStringList QLangSymbolCollector::getKeywords() {
  return QStringList{"if",   "else", "elseif", "end",   "then",
                     "for",  "to",   "next",   "step",  "while",
                     "wend", "do",   "method", "class", "return",
                     "new",  "this", "true",   "false", "null"};
}

QStringList QLangSymbolCollector::getTypeKeywords() {
  return QStringList{"int32", "int64", "float32", "float64", "string",
                     "bool",  "void",  "short",   "cptr"};
}

void QLangSymbolCollector::parse(const QString &source) {
  m_symbols.clear();
  m_classNames.clear();

  parseClasses(source);
}

void QLangSymbolCollector::parseClasses(const QString &source) {
  // Find all class definitions using regex
  // Pattern: class ClassName or class ClassName(ParentClass)
  QRegularExpression classRe(R"(^\s*class\s+(\w+))",
                             QRegularExpression::MultilineOption);

  QRegularExpressionMatchIterator it = classRe.globalMatch(source);

  while (it.hasNext()) {
    QRegularExpressionMatch match = it.next();
    QString className = match.captured(1);
    int classStart = match.capturedStart();

    // Find the 'end' that closes this class
    // Simple approach: count nested ends
    int depth = 1;
    int pos = match.capturedEnd();
    int classEnd = source.length();

    // Find matching 'end' by looking for class/method/if/for/while blocks
    QRegularExpression blockRe(R"(\b(class|method|if|for|while|end)\b)");
    QRegularExpressionMatchIterator blockIt = blockRe.globalMatch(source, pos);

    while (blockIt.hasNext() && depth > 0) {
      QRegularExpressionMatch blockMatch = blockIt.next();
      QString keyword = blockMatch.captured(1).toLower();

      if (keyword == "class" || keyword == "method" || keyword == "if" ||
          keyword == "for" || keyword == "while") {
        depth++;
      } else if (keyword == "end") {
        depth--;
        if (depth == 0) {
          classEnd = blockMatch.capturedStart();
          break;
        }
      }
    }

    // Register the class
    QLangSymbol classSym;
    classSym.name = className;
    classSym.symbolType = "class";
    classSym.line = source.left(classStart).count('\n') + 1;
    m_symbols.append(classSym);
    m_classNames.append(className);

    // Parse methods and members within the class
    parseMethods(source, className, classStart, classEnd);
    parseMembers(source, className, classStart, classEnd);
  }
}

void QLangSymbolCollector::parseMethods(const QString &source,
                                        const QString &className,
                                        int classStart, int classEnd) {
  QString classBody = source.mid(classStart, classEnd - classStart);

  // Pattern: method ReturnType MethodName(params)
  QRegularExpression methodRe(R"(\bmethod\s+(\w+)\s+(\w+)\s*\()");
  QRegularExpressionMatchIterator it = methodRe.globalMatch(classBody);

  while (it.hasNext()) {
    QRegularExpressionMatch match = it.next();
    QString returnType = match.captured(1);
    QString methodName = match.captured(2);
    int methodStart = classStart + match.capturedStart();

    // Find the 'end' that closes this method
    int depth = 1;
    int pos = match.capturedEnd();
    int methodEnd = classBody.length();

    QRegularExpression blockRe(R"(\b(method|if|for|while|end)\b)");
    QRegularExpressionMatchIterator blockIt =
        blockRe.globalMatch(classBody, pos);

    while (blockIt.hasNext() && depth > 0) {
      QRegularExpressionMatch blockMatch = blockIt.next();
      QString keyword = blockMatch.captured(1).toLower();

      if (keyword == "method" || keyword == "if" || keyword == "for" ||
          keyword == "while") {
        depth++;
      } else if (keyword == "end") {
        depth--;
        if (depth == 0) {
          methodEnd = blockMatch.capturedStart();
          break;
        }
      }
    }

    QLangSymbol sym;
    sym.name = methodName;
    sym.symbolType = "method";
    sym.dataType = returnType;
    sym.parentClass = className;
    sym.line = source.left(methodStart).count('\n') + 1;
    m_symbols.append(sym);

    // Parse parameters and locals within the method
    parseParameters(classBody, className, methodName, match.capturedStart());
    parseLocals(classBody, className, methodName, match.capturedEnd(),
                methodEnd);
  }
}

void QLangSymbolCollector::parseMembers(const QString &source,
                                        const QString &className,
                                        int classStart, int classEnd) {
  QString classBody = source.mid(classStart, classEnd - classStart);

  // Find member declarations at class level (not inside methods)
  // Pattern: Type Name or Type Name = value
  // Types: int32, float32, string, bool, or ClassName

  // Split by lines and check each line
  QStringList lines = classBody.split('\n');
  int lineNum = source.left(classStart).count('\n');

  // Track when we're inside a method (skip those lines)
  int methodDepth = 0;

  for (const QString &line : lines) {
    lineNum++;
    QString trimmed = line.trimmed();

    // Track method boundaries
    if (trimmed.startsWith("method ")) {
      methodDepth++;
      continue;
    }
    if (trimmed == "end" || trimmed.startsWith("end ")) {
      if (methodDepth > 0)
        methodDepth--;
      continue;
    }

    // Skip if inside a method
    if (methodDepth > 0)
      continue;

    // Skip empty lines, class declaration, and keywords
    if (trimmed.isEmpty() || trimmed.startsWith("class "))
      continue;

    // Try to match: Type Name or Type Name = ...
    QRegularExpression memberRe(R"(^(\w+)\s+(\w+)\s*(=|;|$))");
    QRegularExpressionMatch match = memberRe.match(trimmed);

    if (match.hasMatch()) {
      QString type = match.captured(1);
      QString name = match.captured(2);

      // Skip if type is a keyword
      if (getKeywords().contains(type.toLower()))
        continue;

      QLangSymbol sym;
      sym.name = name;
      sym.symbolType = "member";
      sym.dataType = type;
      sym.parentClass = className;
      sym.line = lineNum;
      m_symbols.append(sym);
    }
  }
}

void QLangSymbolCollector::parseParameters(const QString &source,
                                           const QString &className,
                                           const QString &methodName,
                                           int methodStart) {
  // Find the parameter list after methodStart
  int parenStart = source.indexOf('(', methodStart);
  int parenEnd = source.indexOf(')', parenStart);

  if (parenStart == -1 || parenEnd == -1)
    return;

  QString params =
      source.mid(parenStart + 1, parenEnd - parenStart - 1).trimmed();
  if (params.isEmpty())
    return;

  // Split by comma
  QStringList paramList = params.split(',');
  for (const QString &param : paramList) {
    QString trimmed = param.trimmed();
    // Pattern: Type Name
    QRegularExpression paramRe(R"((\w+)\s+(\w+))");
    QRegularExpressionMatch match = paramRe.match(trimmed);

    if (match.hasMatch()) {
      QLangSymbol sym;
      sym.name = match.captured(2);
      sym.symbolType = "parameter";
      sym.dataType = match.captured(1);
      sym.parentClass = className;
      sym.parentMethod = methodName;
      m_symbols.append(sym);
    }
  }
}

void QLangSymbolCollector::parseLocals(const QString &source,
                                       const QString &className,
                                       const QString &methodName,
                                       int methodStart, int methodEnd) {
  QString methodBody = source.mid(methodStart, methodEnd - methodStart);

  // Find local variable declarations
  // Pattern: Type Name = ... or Type Name;
  QStringList lines = methodBody.split('\n');

  for (const QString &line : lines) {
    QString trimmed = line.trimmed();

    // Skip control structures
    if (trimmed.startsWith("if ") || trimmed.startsWith("for ") ||
        trimmed.startsWith("while ") || trimmed.startsWith("return") ||
        trimmed == "else" || trimmed == "end")
      continue;

    // Try to match: Type Name = new ... or Type Name = ...
    QRegularExpression localRe(R"(^(\w+)\s+(\w+)\s*=)");
    QRegularExpressionMatch match = localRe.match(trimmed);

    if (match.hasMatch()) {
      QString type = match.captured(1);
      QString name = match.captured(2);

      // Skip if type is a keyword or looks like an assignment
      if (getKeywords().contains(type.toLower()))
        continue;

      QLangSymbol sym;
      sym.name = name;
      sym.symbolType = "local";
      sym.dataType = type;
      sym.parentClass = className;
      sym.parentMethod = methodName;
      m_symbols.append(sym);
    }
  }
}

QStringList QLangSymbolCollector::getClassNames() const {
  QStringList result = m_classNames;
  // Include external class names
  for (auto it = m_externalClasses.constBegin();
       it != m_externalClasses.constEnd(); ++it) {
    if (!result.contains(it.key())) {
      result.append(it.key());
    }
  }
  return result;
}

QList<QLangSymbol>
QLangSymbolCollector::getClassMethods(const QString &className) const {
  QList<QLangSymbol> result;
  for (const auto &sym : m_symbols) {
    if (sym.symbolType == "method" && sym.parentClass == className) {
      result.append(sym);
    }
  }
  return result;
}

QList<QLangSymbol>
QLangSymbolCollector::getClassMembers(const QString &className) const {
  QList<QLangSymbol> result;
  for (const auto &sym : m_symbols) {
    if (sym.symbolType == "member" && sym.parentClass == className) {
      result.append(sym);
    }
  }
  return result;
}

QList<QLangSymbol>
QLangSymbolCollector::getLocalVariables(const QString &className,
                                        const QString &methodName) const {
  QList<QLangSymbol> result;
  for (const auto &sym : m_symbols) {
    if (sym.symbolType == "local" && sym.parentClass == className &&
        sym.parentMethod == methodName) {
      result.append(sym);
    }
  }
  return result;
}

QList<QLangSymbol>
QLangSymbolCollector::getMethodParameters(const QString &className,
                                          const QString &methodName) const {
  QList<QLangSymbol> result;
  for (const auto &sym : m_symbols) {
    if (sym.symbolType == "parameter" && sym.parentClass == className &&
        sym.parentMethod == methodName) {
      result.append(sym);
    }
  }
  return result;
}

QString QLangSymbolCollector::getVariableType(const QString &varName,
                                              const QString &className,
                                              const QString &methodName) const {
  // Check locals first, then parameters, then members
  for (const auto &sym : m_symbols) {
    if (sym.name == varName) {
      if (sym.symbolType == "local" && sym.parentClass == className &&
          sym.parentMethod == methodName) {
        return sym.dataType;
      }
    }
  }

  for (const auto &sym : m_symbols) {
    if (sym.name == varName) {
      if (sym.symbolType == "parameter" && sym.parentClass == className &&
          sym.parentMethod == methodName) {
        return sym.dataType;
      }
    }
  }

  for (const auto &sym : m_symbols) {
    if (sym.name == varName) {
      if (sym.symbolType == "member" && sym.parentClass == className) {
        return sym.dataType;
      }
    }
  }

  return QString();
}

// === External Class Support ===

void QLangSymbolCollector::registerExternalClass(
    const QLangClassDef &classDef) {
  m_externalClasses[classDef.name] = classDef;
}

void QLangSymbolCollector::clearExternalClasses() { m_externalClasses.clear(); }

QStringList
QLangSymbolCollector::getMembersForType(const QString &typeName) const {
  QStringList result;

  // Check external classes first
  if (m_externalClasses.contains(typeName)) {
    const QLangClassDef &classDef = m_externalClasses[typeName];
    result << classDef.members;
    for (const QString &method : classDef.methods) {
      result << method + "()"; // Add () to indicate it's a method
    }

    // Also include parent class members if available
    if (!classDef.parentClass.isEmpty() &&
        m_externalClasses.contains(classDef.parentClass)) {
      result << getMembersForType(classDef.parentClass);
    }
  }

  // Check symbols from current file
  for (const auto &sym : m_symbols) {
    if (sym.parentClass == typeName) {
      if (sym.symbolType == "member") {
        result << sym.name;
      } else if (sym.symbolType == "method") {
        result << sym.name + "()";
      }
    }
  }

  result.removeDuplicates();
  return result;
}

bool QLangSymbolCollector::isKnownType(const QString &typeName) const {
  // Check if it's a primitive type
  if (getTypeKeywords().contains(typeName.toLower())) {
    return true;
  }

  // Check external classes
  if (m_externalClasses.contains(typeName)) {
    return true;
  }

  // Check classes in current file
  if (m_classNames.contains(typeName)) {
    return true;
  }

  return false;
}

QString QLangSymbolCollector::getMemberType(const QString &className,
                                            const QString &memberName) const {
  // First check external classes
  if (m_externalClasses.contains(className)) {
    const QLangClassDef &classDef = m_externalClasses[className];

    // Check memberTypes map
    if (classDef.memberTypes.contains(memberName)) {
      return classDef.memberTypes[memberName];
    }

    // Also check parent class if available
    if (!classDef.parentClass.isEmpty()) {
      QString parentType = getMemberType(classDef.parentClass, memberName);
      if (!parentType.isEmpty()) {
        return parentType;
      }
    }
  }

  // Check symbols from current file
  for (const auto &sym : m_symbols) {
    if (sym.parentClass == className && sym.name == memberName &&
        sym.symbolType == "member") {
      return sym.dataType;
    }
  }

  return QString();
}

QList<QLangSymbolCollector::CompletionItem>
QLangSymbolCollector::getTypedMembersForType(const QString &typeName) const {
  QList<CompletionItem> result;
  QSet<QString> addedNames; // Avoid duplicates

  // First add member variables
  if (m_externalClasses.contains(typeName)) {
    const QLangClassDef &classDef = m_externalClasses[typeName];
    for (const QString &member : classDef.members) {
      if (!addedNames.contains(member)) {
        result.append({member, false}); // false = not a method
        addedNames.insert(member);
      }
    }
  }

  // Check symbols from current file - members
  for (const auto &sym : m_symbols) {
    if (sym.parentClass == typeName && sym.symbolType == "member") {
      if (!addedNames.contains(sym.name)) {
        result.append({sym.name, false});
        addedNames.insert(sym.name);
      }
    }
  }

  // Then add methods
  if (m_externalClasses.contains(typeName)) {
    const QLangClassDef &classDef = m_externalClasses[typeName];
    for (const QString &method : classDef.methods) {
      QString methodName = method + "()";
      if (!addedNames.contains(methodName)) {
        result.append({methodName, true}); // true = method
        addedNames.insert(methodName);
      }
    }

    // Also include parent class members/methods
    if (!classDef.parentClass.isEmpty() &&
        m_externalClasses.contains(classDef.parentClass)) {
      QList<CompletionItem> parentItems =
          getTypedMembersForType(classDef.parentClass);
      for (const auto &item : parentItems) {
        if (!addedNames.contains(item.name)) {
          result.append(item);
          addedNames.insert(item.name);
        }
      }
    }
  }

  // Check symbols from current file - methods
  for (const auto &sym : m_symbols) {
    if (sym.parentClass == typeName && sym.symbolType == "method") {
      QString methodName = sym.name + "()";
      if (!addedNames.contains(methodName)) {
        result.append({methodName, true});
        addedNames.insert(methodName);
      }
    }
  }

  return result;
}

} // namespace Quantum
