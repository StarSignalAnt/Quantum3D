#include "Parser.h"
#include "QAssign.h"
#include <algorithm>
#include <iostream>

Parser::Parser(const std::vector<Token> &tokens) : m_Tokens(tokens) {
#if QLANG_DEBUG
  std::cout << "[DEBUG] Parser created with " << tokens.size() << " tokens"
            << std::endl;
#endif
}

Parser::Parser(const std::vector<Token> &tokens,
               std::shared_ptr<QErrorCollector> errorCollector)
    : m_Tokens(tokens), m_ErrorCollector(errorCollector) {
#if QLANG_DEBUG
  std::cout << "[DEBUG] Parser created with " << tokens.size()
            << " tokens and error collector" << std::endl;
#endif
}

void Parser::ReportError(const std::string &message, QErrorSeverity severity) {
  if (m_ErrorCollector) {
    Token current = Peek();
    m_ErrorCollector->ReportError(severity, message, current.line,
                                  current.column, 0, "parser",
                                  m_CurrentContext);
  } else {
    // Fallback if no collector provided (legacy behavior)
    std::cerr << "[ERROR] " << message << std::endl;
  }
}

void Parser::RecoverToNextStatement() {
  Advance(); // consume the problematic token

  while (!IsAtEnd()) {
    if (Previous().type == TokenType::T_END_OF_LINE) {
      return;
    }

    switch (Peek().type) {
    case TokenType::T_CLASS:
    case TokenType::T_METHOD:
    case TokenType::T_IF:
    case TokenType::T_WHILE:
    case TokenType::T_FOR:
    case TokenType::T_RETURN:
    case TokenType::T_END:
      return;
    default:
      // Keep skipping
      Advance();
    }
  }
}

Parser::~Parser() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] Parser destroyed" << std::endl;
#endif
}

std::shared_ptr<QProgram> Parser::Parse() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] Parse() called - starting parse" << std::endl;
#endif
  return ParseProgram();
}

std::shared_ptr<QProgram> Parser::ParseProgram() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseProgram() - creating QProgram node" << std::endl;
#endif

  auto program = std::make_shared<QProgram>();

  // Parse imports and classes at program level
  while (!IsAtEnd()) {
    Token current = Peek();

    if (current.type == TokenType::T_MODULE) {
      Advance(); // consume 'module'
      if (Check(TokenType::T_IDENTIFIER)) {
        Advance(); // consume module name
      } else {
        ReportError("Expected module name after 'module'");
      }
      // Skip optional newline
      while (Check(TokenType::T_END_OF_LINE)) {
        Advance();
      }
      continue;
    }

    if (current.type == TokenType::T_IMPORT) {
      Advance(); // consume 'import'
      // Expect module name (identifier)
      if (Check(TokenType::T_IDENTIFIER)) {
        Token moduleToken = Advance();
        program->AddImport(moduleToken.value);
#if QLANG_DEBUG
        std::cout << "[DEBUG] ParseProgram() - parsed import: "
                  << moduleToken.value << std::endl;
#endif
      } else {
        ReportError("Expected module name after 'import'");
      }
      // Skip optional newline
      while (Check(TokenType::T_END_OF_LINE)) {
        Advance();
      }
    } else if (current.type == TokenType::T_STATIC) {
      // static class ClassName
      Advance(); // consume 'static'
      if (Check(TokenType::T_CLASS)) {
        auto cls = ParseClass();
        if (cls) {
          cls->SetStatic(true);
          m_ClassNames.insert(cls->GetName());
          program->AddClass(cls);
          std::cout << "[DEBUG] Parser: Parsed static class '" << cls->GetName()
                    << "'" << std::endl;
        }
      } else {
        ReportError("Expected 'class' after 'static'");
      }
    } else if (current.type == TokenType::T_CLASS) {
      auto cls = ParseClass();
      if (cls) {
        // Register class name for instance detection
        m_ClassNames.insert(cls->GetName());
        program->AddClass(cls);
      }
    } else if (current.type == TokenType::T_EOF) {
      break;
    } else if (current.type == TokenType::T_END_OF_LINE) {
      Advance(); // Skip newlines at program level
      continue;
    } else {
      // Parse code block for program-level statements
      break;
    }
  }

  // Parse program code block
  ParseCode(program->GetCode());

  // Post-Parsing Validation
  if (m_ErrorCollector) {
    program->CheckForErrors(m_ErrorCollector);
  }

#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseProgram() - parsed successfully" << std::endl;
#endif
  return program;
}

void Parser::ParseCode(std::shared_ptr<QCode> code) {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseCode() - parsing code block" << std::endl;
#endif

  while (!IsAtEnd()) {
    Token current = Peek();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseCode() - current token: " << current.value
              << " at line " << current.line << std::endl;
#endif

    // Check for block end markers
    if (current.type == TokenType::T_END || current.type == TokenType::T_EOF ||
        current.type == TokenType::T_ELSEIF ||
        current.type == TokenType::T_ELSE ||
        current.type == TokenType::T_NEXT ||
        current.type == TokenType::T_WEND) {
#if QLANG_DEBUG
      std::cout << "[DEBUG] ParseCode() - reached end of block" << std::endl;
#endif
      break;
    }

    // Check for if statement
    if (current.type == TokenType::T_IF) {
      auto ifStmt = ParseIf();
      if (ifStmt) {
        code->AddNode(ifStmt);
      }
    }
    // Check for for loop
    else if (current.type == TokenType::T_FOR) {
      auto forStmt = ParseFor();
      if (forStmt) {
        code->AddNode(forStmt);
      }
    }
    // Check for while loop
    else if (current.type == TokenType::T_WHILE) {
      auto whileStmt = ParseWhile();
      if (whileStmt) {
        code->AddNode(whileStmt);
      }
    }
    // Check for return statement
    else if (current.type == TokenType::T_RETURN) {
      auto returnStmt = ParseReturn();
      if (returnStmt) {
        code->AddNode(returnStmt);
      }
    }
    // Check for super::MethodName() call
    else if (current.type == TokenType::T_SUPER) {
      Advance(); // consume 'super'
      if (Check(TokenType::T_SCOPE)) {
        Advance(); // consume '::'
        if (Check(TokenType::T_IDENTIFIER)) {
          std::string methodName = Peek().value;
          Advance(); // consume method name

          // Create a special method call node for super
          auto superCall = std::make_shared<QMethodCall>("super", methodName);

          // Parse parameters if any
          if (Check(TokenType::T_LPAREN)) {
            Advance(); // consume '('
            // Parse argument expressions if not immediately closing
            while (!Check(TokenType::T_RPAREN) && !IsAtEnd()) {
              auto params = ParseParameters();
              if (params) {
                superCall->SetArguments(params);
              }
              break;
            }
            if (Check(TokenType::T_RPAREN)) {
              Advance(); // consume ')'
            }
          }

          code->AddNode(superCall);
        } else {
          ReportError("expected method name after 'super::'");
        }
      } else {
        ReportError("expected '::' after 'super'");
      }
    }
    // Check for variable declaration (starts with type token)
    else if (IsTypeToken(current.type)) {
      auto varDecl = ParseVariableDecl();
      if (varDecl) {
        code->AddNode(varDecl);
      }
      // Check for class instance declaration (ClassName instanceName = new ...)
    } else if (current.type == TokenType::T_IDENTIFIER &&
               IsClassName(current.value)) {
      auto instanceDecl = ParseInstanceDecl();
      if (instanceDecl) {
        code->AddNode(instanceDecl);
      }
    } else if (current.type == TokenType::T_IDENTIFIER ||
               current.type == TokenType::T_THIS) {
      // Check if this is a dot notation (method call or member assign)
      Token next = PeekNext();
      if (next.type == TokenType::T_DOT) {
        // Need to look further ahead to determine if method call or assignment
        // Could be chained like t1.ot.check = value or t1.ot.Value()

        // Save position
        int savedPos = m_Current;
        Advance(); // consume first identifier

        // Keep consuming .identifier patterns until we hit ( or =
        while (Check(TokenType::T_DOT)) {
          Advance(); // consume dot
          if (!Check(TokenType::T_IDENTIFIER)) {
            m_Current = savedPos;
            std::cerr << "[ERROR] ParseCode() - expected identifier after '.'"
                      << std::endl;
            Advance();
            break;
          }
          Advance(); // consume identifier
        }

        // Now check what follows the chain
        if (Check(TokenType::T_LPAREN)) {
          // It's a method call - restore and parse properly
          m_Current = savedPos;
          auto methodCall = ParseMethodCall();
          if (methodCall) {
            code->AddNode(methodCall);
          }
        } else if (Check(TokenType::T_OPERATOR) && Peek().value == "=") {
          // It's a member assignment - restore and parse properly
          m_Current = savedPos;
          auto memberAssign = ParseMemberAssign();
          if (memberAssign) {
            code->AddNode(memberAssign);
          }
        } else {
          // Unknown, restore position
          m_Current = savedPos;
          std::cerr << "[ERROR] ParseCode() - expected '(' or '=' after "
                       "member access chain"
                    << std::endl;
          Advance();
        }
      } else if (next.type == TokenType::T_LBRACKET) {
        // Indexed assignment: var[expr] = value;
        auto assign = ParseAssign();
        if (assign) {
          code->AddNode(assign);
        }
      } else if (next.type == TokenType::T_OPERATOR && next.value == "=") {
        // Simple variable assignment: var = value;
        auto assign = ParseAssign();
        if (assign) {
          code->AddNode(assign);
        }
      } else if (next.type == TokenType::T_OPERATOR &&
                 (next.value == "++" || next.value == "--")) {
        // Increment or decrement: var++ or var--
        auto increment = ParseIncrement();
        if (increment) {
          code->AddNode(increment);
        }
      } else if (Check(TokenType::T_IDENTIFIER)) {
        // Variable Declaration Check: Type Name
        // We detect this by checking if the NEXT token is an identifier or '<'
        Token next = PeekNext();
        if (next.type == TokenType::T_IDENTIFIER ||
            next.type == TokenType::T_LESS) {

          // Name validation is deferred to QRunner::EnsureNames()
          // Parse the variable declaration - validation happens after all
          // classes are registered
          auto varDecl = ParseVariableDecl();
          if (varDecl) {
            code->AddNode(varDecl);
          }

        } else {
          // Function call or other statement
          auto statement = ParseStatement();
          if (statement) {
            code->AddNode(statement);
          }
        }
      }
    }

    else if (current.type == TokenType::T_END_OF_LINE) {
      Advance(); // Skip newlines
    } else {
      // Report error for unexpected tokens
      ReportError("Unexpected token '" + current.value + "'");
      Advance();
    }
  } // End while

#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseCode() - finished parsing block" << std::endl;
#endif
}

std::shared_ptr<QStatement> Parser::ParseStatement() {
  Token identifier = Peek();
#if QLANG_DEBUG
  std::cout << "Parsing " << identifier.value << std::endl;
#endif

  // Consume the identifier
  Advance();

  auto statement = std::make_shared<QStatement>(identifier.value);

  // Check for parameters - look for '('
  if (Check(TokenType::T_LPAREN)) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseStatement() - found '(', parsing parameters"
              << std::endl;
#endif
    auto params = ParseParameters();
    statement->SetParameters(params);
  } else {
    // ENFORCE STRICT SYNTAX: Function calls MUST have parentheses.
    // If we parsed an identifier as a statement (and it wasn't a variable
    // decl), it must be a function call.
    ReportError("Expected '(' after function or method name '" +
                identifier.value + "'");
  }

  // Consume end of line if present
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseStatement() - consumed end of line" << std::endl;
#endif
  }

  return statement;
}

std::shared_ptr<QParameters> Parser::ParseParameters() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseParameters() - starting" << std::endl;
#endif

  auto params = std::make_shared<QParameters>();

  // Consume '('
  if (Check(TokenType::T_LPAREN)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseParameters() - consumed '('" << std::endl;
#endif
  }

  // Check for empty parameters ()
  if (Check(TokenType::T_RPAREN)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseParameters() - empty parameters, consumed ')'"
              << std::endl;
#endif
    return params;
  }

  // Parse first expression
  auto expr = ParseExpression();
  if (expr) {
    params->AddParameter(expr);
  }

  // Parse remaining expressions separated by ','
  while (Check(TokenType::T_COMMA)) {
    Advance(); // consume ','
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseParameters() - consumed ','" << std::endl;
#endif

    expr = ParseExpression();
    if (expr) {
      params->AddParameter(expr);
    }
  }

  // Consume ')'
  if (Check(TokenType::T_RPAREN)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseParameters() - consumed ')'" << std::endl;
#endif
  } else {
    std::cerr << "[ERROR] ParseParameters() - expected ')'" << std::endl;
  }

  return params;
}

std::shared_ptr<QExpression> Parser::ParseExpression() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseExpression() - starting" << std::endl;
#endif

  auto expr = std::make_shared<QExpression>();
  int parenDepth = 0;

  // Collect tokens until we hit ',' or ')' (at depth 0) or ';' or EOF
  // Also stop at 'to' and ':' for for-loop range parsing (for x = 0 to 10 : 1)
  while (!IsAtEnd() && !Check(TokenType::T_END_OF_LINE) &&
         !Check(TokenType::T_EOF) && !Check(TokenType::T_TO) &&
         !Check(TokenType::T_COLON)) {
    Token current = Peek();

    // Track parenthesis depth
    if (current.type == TokenType::T_LPAREN) {
      parenDepth++;
      expr->AddElement(current);
      Advance();
    } else if (current.type == TokenType::T_RPAREN) {
      if (parenDepth > 0) {
        // This closes a paren we opened - include it
        parenDepth--;
        expr->AddElement(current);
        Advance();
      } else {
        // This is the function call's closing paren - stop
        break;
      }
    } else if (current.type == TokenType::T_COMMA && parenDepth == 0) {
      // Comma at top level - stop (next parameter)
      break;
    } else {
      // Name validation (undeclared variables) is deferred to
      // QRunner::EnsureNames()

      expr->AddElement(current);
      Advance();
    }
  }

#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseExpression() - finished with "
            << expr->GetElements().size() << " elements" << std::endl;
#endif

  return expr;
}

Token Parser::Consume(TokenType type, const std::string &message) {
  if (Check(type)) {
    return Advance();
  }

  ReportError(message);
  return Peek();
}

// Helper methods
Token Parser::Peek() const {
  if (m_Current >= m_Tokens.size()) {
    Token eof;
    eof.type = TokenType::T_EOF;
    eof.value = "";
    eof.line = -1;
    eof.column = -1;
    return eof;
  }
  return m_Tokens[m_Current];
}

Token Parser::Previous() const {
  if (m_Current <= 0) {
    return m_Tokens[0];
  }
  return m_Tokens[m_Current - 1];
}

Token Parser::PeekNext() const {
  if (m_Current + 1 >= m_Tokens.size()) {
    return m_Tokens.back(); // Return EOF
  }
  return m_Tokens[m_Current + 1];
}

Token Parser::Advance() {
  if (!IsAtEnd()) {
    m_Current++;
  }
  return Previous();
}

bool Parser::IsAtEnd() const {
  return m_Current >= m_Tokens.size() || Peek().type == TokenType::T_EOF;
}

bool Parser::Check(TokenType type) const {
  if (IsAtEnd())
    return false;
  return Peek().type == type;
}

bool Parser::Match(TokenType type) {
  if (Check(type)) {
    Advance();
    return true;
  }
  return false;
}

bool Parser::IsTypeToken(TokenType type) const {
  return type == TokenType::T_INT32 || type == TokenType::T_INT64 ||
         type == TokenType::T_FLOAT32 || type == TokenType::T_FLOAT64 ||
         type == TokenType::T_SHORT || type == TokenType::T_STRING_TYPE ||
         type == TokenType::T_BOOL || type == TokenType::T_CPTR ||
         type == TokenType::T_IPTR || type == TokenType::T_FPTR ||
         type == TokenType::T_BYTE || type == TokenType::T_BPTR;
}

std::shared_ptr<QVariableDecl> Parser::ParseVariableDecl() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseVariableDecl() - parsing variable declaration"
            << std::endl;
#endif

  // Get the type token
  Token typeToken = Advance();
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseVariableDecl() - type: " << typeToken.value
            << std::endl;
#endif

  // Type validation is deferred to runtime
  // Accept any identifier as a potential type (primitives, classes, generics)
  // Runtime will validate when the variable is used

  // Parse generic type parameters if present: Type<T, U> Name
  std::vector<std::string> typeParams;
  if (Check(TokenType::T_LESS)) { // '<'
    Advance();                    // consume '<'
    while (!IsAtEnd() && !Check(TokenType::T_GREATER)) {
      if (Check(TokenType::T_IDENTIFIER) || IsTypeToken(Peek().type)) {
        typeParams.push_back(Peek().value);
        Advance();
      } else {
        ReportError("Expected type parameter");
      }
      if (Check(TokenType::T_COMMA))
        Advance();
    }
    if (Check(TokenType::T_GREATER))
      Advance();
    else
      ReportError("Expected '>' to close type parameters");
  }

  // Expect identifier (variable name)
  if (!Check(TokenType::T_IDENTIFIER)) {
    // This catches "i2f Val" where "i2f" is type, "Val" is name.
    // But if we have "i2f" alone, Peek() might be EOL or EOF.
    ReportError("Expected variable name (identifier) after type '" +
                typeToken.value + "'");
    return nullptr;
  }

  Token nameToken = Advance();
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseVariableDecl() - name: " << nameToken.value
            << std::endl;
#endif

  auto varDecl = std::make_shared<QVariableDecl>(
      typeToken.type, nameToken.value, typeToken.value);
  varDecl->SetTypeParameters(typeParams);

  // Register this variable as declared
  m_DeclaredVariables.insert(nameToken.value);

  // Check for initializer (= expression)
  if (Check(TokenType::T_OPERATOR) && Peek().value == "=") {
    Advance(); // consume '='
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseVariableDecl() - parsing initializer"
              << std::endl;
#endif

    auto initializer = ParseExpression();
    varDecl->SetInitializer(initializer);
  }

  // Consume end of line (Mandatory for declarations)
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseVariableDecl() - consumed semicolon"
              << std::endl;
#endif
  } else if (!Check(TokenType::T_EOF)) {
    // If NOT at EOF, report error (missing semicolon)
    // This will catch "i2f Val == null" cases where '==' is found instead of
    // EOL
    ReportError("Expected end of line (or ';') after variable declaration");
  }

  return varDecl;
}

std::shared_ptr<QClass> Parser::ParseClass() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseClass() - parsing class definition" << std::endl;
#endif

  // Consume 'class' keyword
  Advance();

  // Expect class name (identifier)
  if (!Check(TokenType::T_IDENTIFIER)) {
    std::cerr << "[ERROR] ParseClass() - expected class name after 'class'"
              << std::endl;
    return nullptr;
  }

  Token nameToken = Advance();
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseClass() - class name: " << nameToken.value
            << std::endl;
#endif

  // Register class name immediately so self-referential members (e.g. Node
  // next) works
  m_ClassNames.insert(nameToken.value);

  // Set current context header (Class Name)
  std::string previousContext = m_CurrentContext;
  m_CurrentContext = nameToken.value;

  // Clear class member tracking for this class
  m_ClassMemberVariables.clear();

  auto cls = std::make_shared<QClass>(nameToken.value);

  // Check for inheritance syntax: class Name(ParentClass)
  if (Check(TokenType::T_LPAREN)) {
    Advance(); // consume '('
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseClass() - parsing parent class" << std::endl;
#endif

    if (!Check(TokenType::T_IDENTIFIER)) {
      ReportError("expected parent class name after '('");
    } else {
      Token parentToken = Advance();

      // Defer parent class validation to runtime (CreateInstance time)
      // This allows child classes to be defined before parent classes
      cls->SetParentClass(parentToken.value);
#if QLANG_DEBUG
      std::cout << "[DEBUG] ParseClass() - parent class: " << parentToken.value
                << std::endl;
#endif
    }

    // Consume ')'
    if (Check(TokenType::T_RPAREN)) {
      Advance();
    } else {
      ReportError("expected ')' after parent class name");
    }
  }

  // Check for generic type parameters <T, U, V, ...>
  if (Check(TokenType::T_LESS)) {
    Advance(); // consume '<'
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseClass() - parsing generic type parameters"
              << std::endl;
#endif

    std::vector<std::string> typeParams;
    while (!IsAtEnd() && !Check(TokenType::T_GREATER)) {
      // Expect identifier for type parameter
      if (!Check(TokenType::T_IDENTIFIER)) {
        std::cerr << "[ERROR] ParseClass() - expected type parameter name"
                  << std::endl;
        break;
      }
      Token typeParam = Advance();
      typeParams.push_back(typeParam.value);
#if QLANG_DEBUG
      std::cout << "[DEBUG] ParseClass() - type parameter: " << typeParam.value
                << std::endl;
#endif

      // Check for comma (more parameters) or end
      if (Check(TokenType::T_COMMA)) {
        Advance(); // consume ','
      }
    }

    // Consume '>'
    if (Check(TokenType::T_GREATER)) {
      Advance();
    } else {
      ReportError("ParseClass() - expected '>' to close type parameters");
    }

    cls->SetTypeParameters(typeParams);
  }

  // Parse class body until END token
  // Get type parameters for checking generic types
  const auto &typeParams = cls->GetTypeParameters();

  // Set current scope type parameters for member/method parsing
  std::vector<std::string> previousTypeParams = m_CurrentTypeParams;
  m_CurrentTypeParams = typeParams;

  while (!IsAtEnd() && !Check(TokenType::T_END)) {
    Token current = Peek();

    // Parse methods
    if (current.type == TokenType::T_METHOD) {
      auto method = ParseMethod();
      if (method) {
        cls->AddMethod(method);
      }
      // Parse member variables (primitive type declarations)
    } else if (IsTypeToken(current.type)) {
      auto member = ParseVariableDecl();
      if (member) {
        cls->AddMember(member);
        // Register as class member variable
        m_ClassMemberVariables.insert(member->GetName());
      }
      // Parse generic type parameter members (T, K, V, etc.)
    } else if (current.type == TokenType::T_IDENTIFIER &&
               std::find(m_CurrentTypeParams.begin(), m_CurrentTypeParams.end(),
                         current.value) != m_CurrentTypeParams.end()) {
      // This is a generic type parameter used as a member type
#if QLANG_DEBUG
      std::cout << "[DEBUG] ParseClass() - parsing generic type member: "
                << current.value << std::endl;
#endif
      auto member = ParseVariableDecl();
      if (member) {
        cls->AddMember(member);
        // Register as class member variable
        m_ClassMemberVariables.insert(member->GetName());
      }
      // Parse class-type member variables (ClassName varName = new ...)
      // Parse class-type member variables (ClassName varName = new ...)
    } else if (current.type == TokenType::T_IDENTIFIER &&
               (IsClassName(current.value) ||
                PeekNext().type == TokenType::T_IDENTIFIER ||
                PeekNext().type == TokenType::T_LESS)) {
      // This is a class-type member declaration
#if QLANG_DEBUG
      std::cout << "[DEBUG] ParseClass() - parsing class-type member: "
                << current.value << std::endl;
#endif
      auto member = ParseClassTypeMember();
      if (member) {
        cls->AddMember(member);
        // Register as class member variable
        m_ClassMemberVariables.insert(member->GetName());
      }
    } else if (current.type == TokenType::T_END_OF_LINE) {
      Advance(); // Skip newlines in class body
    } else {
      // Skip unknown tokens inside class
#if QLANG_DEBUG
      std::cout << "[DEBUG] ParseClass() - skipping token: " << current.value
                << std::endl;
#endif
      Advance();
    }
  }

  // Consume 'end' keyword
  if (Check(TokenType::T_END)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseClass() - consumed 'end'" << std::endl;
#endif
  } else {
    ReportError("expected 'end' to close class");
    RecoverToNextStatement();
  }

  // Restore previous type params and context
  m_CurrentTypeParams = previousTypeParams;
  m_CurrentContext = previousContext;

  return cls;
}

std::shared_ptr<QMethod> Parser::ParseMethod() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseMethod() - parsing method" << std::endl;
#endif

  // Consume 'method' keyword
  Advance();

  // Expect return type (void, primitive type token, or class identifier)
  TokenType returnType = TokenType::T_VOID;
  std::string returnTypeName = "void";
  Token typeToken = Peek();

  if (Check(TokenType::T_VOID) || IsTypeToken(typeToken.type)) {
    returnType = typeToken.type;
    returnTypeName = typeToken.value;
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseMethod() - return type: " << returnTypeName
              << std::endl;
#endif
  } else if (typeToken.type == TokenType::T_IDENTIFIER &&
             PeekNext().type == TokenType::T_IDENTIFIER) {
    // Return type is a class name (identifier)
    returnType = typeToken.type;
    returnTypeName = typeToken.value;
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseMethod() - class return type: " << returnTypeName
              << std::endl;
#endif
  }

  // Expect method name (identifier)
  if (!Check(TokenType::T_IDENTIFIER)) {
    std::cerr << "[ERROR] ParseMethod() - expected method name" << std::endl;
    return nullptr;
  }

  Token nameToken = Advance();
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseMethod() - method name: " << nameToken.value
            << std::endl;
#endif

  auto method = std::make_shared<QMethod>(nameToken.value);
  method->SetReturnType(returnType, returnTypeName);

  // Context Management
  std::string methodName = nameToken.value;
  std::string fullContext = m_CurrentContext.empty()
                                ? methodName
                                : m_CurrentContext + "." + methodName;
  int startLine = nameToken.line;

  std::string previousContext = m_CurrentContext;
  m_CurrentContext = fullContext;

  // Expect '(' for parameters
  if (Check(TokenType::T_LPAREN)) {
    Advance(); // consume '('

    // Clear declared variables for this method scope
    m_DeclaredVariables.clear();

    // Parse parameter list: type name, type name, ...
    while (!IsAtEnd() && !Check(TokenType::T_RPAREN)) {
      // Expect type token (primitive type OR identifier for generics/class
      // types)
      if (IsTypeToken(Peek().type) || Peek().type == TokenType::T_IDENTIFIER) {
        Token typeToken = Advance(); // consume type
        TokenType paramType = typeToken.type;
        std::string paramTypeName = typeToken.value;

        // Expect parameter name
        if (Check(TokenType::T_IDENTIFIER)) {
          std::string paramName = Peek().value;
          Advance(); // consume name

          method->AddParameter(paramType, paramName, paramTypeName);

          // Register parameter as a declared variable
          m_DeclaredVariables.insert(paramName);
#if QLANG_DEBUG
          std::cout << "[DEBUG] ParseMethod() - parsed param: " << paramName
                    << " (type: " << paramTypeName << ")" << std::endl;
#endif
        } else {
          ReportError("expected parameter name");
        }
      } else {
        ReportError("expected parameter type");
        Advance(); // skip unknown token
      }

      // Check for comma (more parameters) or end
      if (Check(TokenType::T_COMMA)) {
        Advance(); // consume ','
      }
    }

    if (Check(TokenType::T_RPAREN)) {
      Advance(); // consume ')'
#if QLANG_DEBUG
      std::cout << "[DEBUG] ParseMethod() - consumed parameters, count: "
                << method->GetParameters().size() << std::endl;
#endif
    }
  }

  // Check for 'virtual' or 'override' keywords after parameters
  if (Check(TokenType::T_VIRTUAL)) {
    Advance(); // consume 'virtual'
    method->SetVirtual(true);
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseMethod() - method is VIRTUAL" << std::endl;
#endif
  } else if (Check(TokenType::T_OVERRIDE)) {
    Advance(); // consume 'override'
    method->SetOverride(true);
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseMethod() - method is OVERRIDE" << std::endl;
#endif
  }

  // Parse method body until 'end'
  ParseCode(method->GetBody());

  // Consume 'end' keyword for method
  if (Check(TokenType::T_END)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseMethod() - consumed 'end'" << std::endl;
#endif
  } else {
    ReportError("expected 'end' to close method");
  }

  if (m_ErrorCollector) {
    // Register the full range of the method
    // Use current line (or previous token line) as end
    int endLine = Previous().line;
    m_ErrorCollector->RegisterContext(fullContext, startLine, endLine);
  }

  m_CurrentContext = previousContext;

  return method;
}

bool Parser::IsClassName(const std::string &name) const {
  return m_ClassNames.find(name) != m_ClassNames.end();
}

std::shared_ptr<QInstanceDecl> Parser::ParseInstanceDecl() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseInstanceDecl() - parsing instance declaration"
            << std::endl;
#endif

  // Get class name
  Token classNameToken = Advance();
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseInstanceDecl() - class: " << classNameToken.value
            << std::endl;
#endif

  // Check for generic type arguments <type1, type2, ...>
  std::vector<std::string> typeArgs;
  if (Check(TokenType::T_LESS)) {
    Advance(); // consume '<'
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseInstanceDecl() - parsing type arguments"
              << std::endl;
#endif

    while (!IsAtEnd() && !Check(TokenType::T_GREATER)) {
      // Get the type name (could be identifier or type keyword)
      Token typeArg = Advance();
      typeArgs.push_back(typeArg.value);
#if QLANG_DEBUG
      std::cout << "[DEBUG] ParseInstanceDecl() - type arg: " << typeArg.value
                << std::endl;
#endif

      // Check for comma or end
      if (Check(TokenType::T_COMMA)) {
        Advance();
      }
    }

    // Consume '>'
    if (Check(TokenType::T_GREATER)) {
      Advance();
    }
  }

  // Expect instance name (identifier)
  if (!Check(TokenType::T_IDENTIFIER)) {
    ReportError("expected instance name");
    return nullptr;
  }

  Token instanceNameToken = Advance();
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseInstanceDecl() - instance: "
            << instanceNameToken.value << std::endl;
#endif

  auto instanceDecl = std::make_shared<QInstanceDecl>(classNameToken.value,
                                                      instanceNameToken.value);

  // Register this instance as a declared variable
  m_DeclaredVariables.insert(instanceNameToken.value);

  // Set type arguments if any
  if (!typeArgs.empty()) {
    instanceDecl->SetTypeArguments(typeArgs);
  }

  // Check for optional initialization '= new ClassName()'
  // If just a semicolon, this is an uninitialized declaration (null reference)
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseInstanceDecl() - uninitialized declaration"
              << std::endl;
#endif
    return instanceDecl;
  }

  // Expect '=' for initialization
  if (!Check(TokenType::T_OPERATOR) || Peek().value != "=") {
    ReportError("expected '=' or ';'");
    return nullptr;
  }
  Advance(); // consume '='

  // Check if this is a 'new' expression or a general expression (like method
  // call)
  if (Check(TokenType::T_NEW)) {
    // Standard 'new ClassName()' initialization
    Advance(); // consume 'new'

    // Expect constructor class name (should match)
    if (!Check(TokenType::T_IDENTIFIER) ||
        Peek().value != classNameToken.value) {
      ReportError("constructor class name doesn't match");
      // Still continue for flexibility
    }
    if (Check(TokenType::T_IDENTIFIER)) {
      Advance(); // consume constructor class name
    }

    // Skip type arguments on constructor side (List<int32> myList = new
    // List<int32>())
    if (Check(TokenType::T_LESS)) {
      Advance(); // consume '<'
      while (!IsAtEnd() && !Check(TokenType::T_GREATER)) {
        Advance(); // skip type args
        if (Check(TokenType::T_COMMA)) {
          Advance();
        }
      }
      if (Check(TokenType::T_GREATER)) {
        Advance(); // consume '>'
      }
    }

    // Expect '(' for constructor args
    if (Check(TokenType::T_LPAREN)) {
      // ParseParameters handles the opening and closing parens: ( arg1, arg2 )
      auto args = ParseParameters();
      instanceDecl->SetConstructorArgs(args);
    }
  } else {
    // Expression assignment (e.g., Vec3 pos = obj.GetPosition())
    // Parse the initializer expression and store it
    auto initExpr = ParseExpression();
    instanceDecl->SetInitializerExpression(initExpr);
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseInstanceDecl() - parsed expression initializer"
              << std::endl;
#endif
  }

  // Consume semicolon
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseInstanceDecl() - consumed semicolon"
              << std::endl;
#endif
  }

  return instanceDecl;
}

std::shared_ptr<QMethodCall> Parser::ParseMethodCall() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseMethodCall() - parsing method call" << std::endl;
#endif

  // Get first identifier (instance name)
  Token firstToken = Advance();
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseMethodCall() - first: " << firstToken.value
            << std::endl;
#endif

  // Build the path by consuming .identifier patterns
  std::vector<std::string> pathParts;
  pathParts.push_back(firstToken.value);

  while (Check(TokenType::T_DOT)) {
    Advance(); // consume '.'

    if (!Check(TokenType::T_IDENTIFIER)) {
      ReportError("expected identifier after '.'");
      return nullptr;
    }

    Token next = Advance();
    pathParts.push_back(next.value);
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseMethodCall() - path part: " << next.value
              << std::endl;
#endif
  }

  // Must have at least 2 parts: instance and method
  if (pathParts.size() < 2) {
    ReportError("incomplete method call");
    return nullptr;
  }

  // Last part is the method name, everything else is the instance path
  std::string methodName = pathParts.back();
  pathParts.pop_back();

  // Join remaining parts with dots as instance path
  std::string instancePath;
  for (size_t i = 0; i < pathParts.size(); i++) {
    if (i > 0)
      instancePath += ".";
    instancePath += pathParts[i];
  }

#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseMethodCall() - instance path: " << instancePath
            << ", method: " << methodName << std::endl;
#endif

  auto methodCall = std::make_shared<QMethodCall>(instancePath, methodName);

  // Parse arguments if present
  if (Check(TokenType::T_LPAREN)) {
    auto args = ParseParameters();
    methodCall->SetArguments(args);
  }

  // Consume semicolon
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseMethodCall() - consumed semicolon" << std::endl;
#endif
  }

  return methodCall;
}

std::shared_ptr<QMemberAssign> Parser::ParseMemberAssign() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseMemberAssign() - parsing member assignment"
            << std::endl;
#endif

  // Get instance name
  Token instanceNameToken = Advance();
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseMemberAssign() - instance: "
            << instanceNameToken.value << std::endl;
#endif

  // Expect '.'
  if (!Check(TokenType::T_DOT)) {
    ReportError("expected '.'");
    return nullptr;
  }
  Advance(); // consume '.'

  // Expect member name (identifier)
  if (!Check(TokenType::T_IDENTIFIER)) {
    ReportError("expected member name");
    return nullptr;
  }

  // Build full member path by consuming .identifier patterns
  std::string memberPath;
  Token memberNameToken = Advance();
  memberPath = memberNameToken.value;
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseMemberAssign() - member: " << memberNameToken.value
            << std::endl;
#endif

  // Continue consuming .identifier patterns until we hit '='
  while (Check(TokenType::T_DOT)) {
    Advance(); // consume '.'
    if (!Check(TokenType::T_IDENTIFIER)) {
      ReportError("expected member name after '.'");
      return nullptr;
    }
    Token nextMember = Advance();
    memberPath += "." + nextMember.value;
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseMemberAssign() - chained member: "
              << nextMember.value << std::endl;
#endif
  }

#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseMemberAssign() - full path: "
            << instanceNameToken.value << "." << memberPath << std::endl;
#endif

  auto memberAssign =
      std::make_shared<QMemberAssign>(instanceNameToken.value, memberPath);

  // Expect '='
  if (!Check(TokenType::T_OPERATOR) || Peek().value != "=") {
    ReportError("expected '='");
    return nullptr;
  }
  Advance(); // consume '='

  // Parse the value expression
  auto valueExpr = ParseExpression();
  memberAssign->SetValueExpression(valueExpr);

#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseMemberAssign() - parsed value expression"
            << std::endl;
#endif

  // Consume semicolon
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseMemberAssign() - consumed semicolon"
              << std::endl;
#endif
  }

  return memberAssign;
}

std::shared_ptr<QVariableDecl> Parser::ParseClassTypeMember() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseClassTypeMember() - parsing class-type member"
            << std::endl;
#endif

  // Get class type name (e.g., "other")
  Token classTypeToken = Advance();
  std::string classTypeName = classTypeToken.value;
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseClassTypeMember() - class type: " << classTypeName
            << std::endl;
#endif

  // Parse generic type parameters if present: ClassName<T, U> Name
  std::vector<std::string> typeParams;
  if (Check(TokenType::T_LESS)) { // '<'
    Advance();                    // consume '<'
    while (!IsAtEnd() && !Check(TokenType::T_GREATER)) {
      if (Check(TokenType::T_IDENTIFIER) || IsTypeToken(Peek().type)) {
        typeParams.push_back(Peek().value);
        Advance();
      } else {
        ReportError("Expected type parameter");
      }
      if (Check(TokenType::T_COMMA))
        Advance();
    }
    if (Check(TokenType::T_GREATER))
      Advance();
    else
      ReportError("Expected '>' to close type parameters");
  }

  // Expect member name (identifier)
  if (!Check(TokenType::T_IDENTIFIER)) {
    ReportError("expected member name");
    return nullptr;
  }

  Token memberNameToken = Advance();
  std::string memberName = memberNameToken.value;
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseClassTypeMember() - member name: " << memberName
            << std::endl;
#endif

  // Create variable declaration with T_IDENTIFIER type, but include the
  // classTypeName
  auto member = std::make_shared<QVariableDecl>(TokenType::T_IDENTIFIER,
                                                memberName, classTypeName);
  member->SetTypeParameters(typeParams);

  // Expect '=' for initialization
  if (Check(TokenType::T_OPERATOR) && Peek().value == "=") {
    Advance(); // consume '='

    // Parse the initializer expression (new ClassName())
    auto initExpr = ParseExpression();
    member->SetInitializer(initExpr);

#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseClassTypeMember() - parsed initializer"
              << std::endl;
#endif
  }

  // Consume semicolon (Mandatory)
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseClassTypeMember() - consumed semicolon"
              << std::endl;
#endif
  } else if (!Check(TokenType::T_EOF)) {
    ReportError("Expected end of line (or ';') after member declaration");
  }

  return member;
}

std::shared_ptr<QReturn> Parser::ParseReturn() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseReturn() - parsing return statement" << std::endl;
#endif

  // Consume 'return' keyword
  Advance();

  auto returnStmt = std::make_shared<QReturn>();

  // Parse optional expression (if not immediately at semicolon)
  if (!Check(TokenType::T_END_OF_LINE) && !Check(TokenType::T_EOF) &&
      !Check(TokenType::T_END)) {
    auto expr = ParseExpression();
    returnStmt->SetExpression(expr);
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseReturn() - parsed return expression"
              << std::endl;
#endif
  }

  // Consume semicolon
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseReturn() - consumed semicolon" << std::endl;
#endif
  }

  return returnStmt;
}

std::shared_ptr<QAssign> Parser::ParseAssign() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseAssign() - parsing assignment" << std::endl;
#endif

  // Get variable name
  Token nameToken = Advance();
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseAssign() - variable: " << nameToken.value
            << std::endl;
#endif

  auto assign = std::make_shared<QAssign>(nameToken.value);

  // Check for index expression: var[expr] = value
  if (Check(TokenType::T_LBRACKET)) {
    Advance(); // consume '['
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseAssign() - parsing index expression"
              << std::endl;
#endif

    // Parse index expression until we hit ']'
    auto indexExpr = std::make_shared<QExpression>();
    int bracketDepth = 1;
    while (!IsAtEnd() && bracketDepth > 0) {
      Token current = Peek();
      if (current.type == TokenType::T_LBRACKET) {
        bracketDepth++;
        indexExpr->AddElement(current);
        Advance();
      } else if (current.type == TokenType::T_RBRACKET) {
        bracketDepth--;
        if (bracketDepth > 0) {
          indexExpr->AddElement(current);
        }
        Advance();
      } else {
        indexExpr->AddElement(current);
        Advance();
      }
    }
    assign->SetIndexExpression(indexExpr);
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseAssign() - index parsed with "
              << indexExpr->GetElements().size() << " elements" << std::endl;
#endif
  }

  // Expect '='
  if (!Check(TokenType::T_OPERATOR) || Peek().value != "=") {
    ReportError("expected '='");
    return nullptr;
  }
  Advance(); // consume '='

  // Check if variable is declared (local variables or class members)
  bool isDeclared =
      m_DeclaredVariables.find(nameToken.value) != m_DeclaredVariables.end() ||
      m_ClassMemberVariables.find(nameToken.value) !=
          m_ClassMemberVariables.end();

  if (!isDeclared) {
    ReportError("Undeclared variable '" + nameToken.value + "'",
                QErrorSeverity::Warning);
  }

  // Parse value expression
  auto expr = ParseExpression();
  assign->SetValueExpression(expr);

  // Consume semicolon
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseAssign() - consumed semicolon" << std::endl;
#endif
  }

  return assign;
}

std::shared_ptr<QIf> Parser::ParseIf() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseIf() - parsing if statement" << std::endl;
#endif
  Advance(); // consume 'if'

  auto ifNode = std::make_shared<QIf>();

  // Parse condition
  auto condition = ParseExpression();
  if (!condition) {
    std::cerr << "[ERROR] ParseIf() - expected condition" << std::endl;
    return nullptr;
  }

  // Parse 'then' block
  auto thenBlock = std::make_shared<QCode>();
  ParseCode(thenBlock);
  ifNode->SetIf(condition, thenBlock);

  // Check for elseif or else
  while (Check(TokenType::T_ELSEIF)) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseIf() - parsing elseif" << std::endl;
#endif
    Advance(); // consume 'elseif'

    auto elseIfCond = ParseExpression();
    auto elseIfBlock = std::make_shared<QCode>();
    ParseCode(elseIfBlock);

    ifNode->AddElseIf(elseIfCond, elseIfBlock);
  }

  if (Check(TokenType::T_ELSE)) {
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseIf() - parsing else" << std::endl;
#endif
    Advance(); // consume 'else'

    auto elseBlock = std::make_shared<QCode>();
    ParseCode(elseBlock);
    ifNode->SetElse(elseBlock);
  }

  Consume(TokenType::T_END, "Expected 'end' after if statement");
  return ifNode;
}

std::shared_ptr<QFor> Parser::ParseFor() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseFor() - parsing for loop" << std::endl;
#endif

  Advance(); // Consume 'for'

  // Check for optional type before variable name
  TokenType varType = TokenType::T_UNKNOWN;
  bool hasType = false;

  Token current = Peek();
  if (IsTypeToken(current.type)) {
    // Check if this is a valid for loop type (only numeric types allowed)
    if (current.type == TokenType::T_BOOL ||
        current.type == TokenType::T_STRING_TYPE) {
      std::cerr << "[ERROR] ParseFor() - Illegal for type: " << current.value
                << std::endl;
      return nullptr;
    }
    varType = current.type;
    hasType = true;
    Advance(); // Consume type
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseFor() - type declared: " << current.value
              << std::endl;
#endif
  }

  // Expect variable name
  if (!Check(TokenType::T_IDENTIFIER)) {
    std::cerr << "[ERROR] ParseFor() - expected variable name" << std::endl;
    return nullptr;
  }

  Token varToken = Advance();
  auto forNode = std::make_shared<QFor>(varToken.value);

  // Register the for loop variable as declared
  m_DeclaredVariables.insert(varToken.value);

  // Set the type if one was declared
  if (hasType) {
    forNode->SetVarType(varType);
  }

  // Expect '='
  if (!Check(TokenType::T_OPERATOR) || Peek().value != "=") {
    std::cerr << "[ERROR] ParseFor() - expected '='" << std::endl;
    return nullptr;
  }
  Advance(); // Consume '='

  // Parse start expression
  auto startExpr = ParseExpression();

  // Expect 'to'
  if (!Check(TokenType::T_TO)) {
    std::cerr << "[ERROR] ParseFor() - expected 'to'" << std::endl;
    return nullptr;
  }
  Advance(); // Consume 'to'

  // Parse end expression
  auto endExpr = ParseExpression();

  // Check for optional step ': step'
  std::shared_ptr<QExpression> stepExpr = nullptr;
  if (Check(TokenType::T_COLON)) {
    Advance(); // Consume ':'
    stepExpr = ParseExpression();
  }

  forNode->SetRange(startExpr, endExpr, stepExpr);

  // Parse body
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseFor() - parsing body" << std::endl;
#endif
  auto body = std::make_shared<QCode>();
  ParseCode(body);
  forNode->SetBody(body);

  // Expect 'next'
  if (Check(TokenType::T_NEXT)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseFor() - consumed 'next'" << std::endl;
#endif
  } else {
    std::cerr << "[ERROR] ParseFor() - expected 'next'" << std::endl;
  }

  return forNode;
}

std::shared_ptr<QWhile> Parser::ParseWhile() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseWhile() - parsing while loop" << std::endl;
#endif

  Advance(); // Consume 'while'

  // Parse condition
  auto condition = ParseExpression();
  if (!condition) {
    // If we get here, we expected an expression but didn't find one
    ReportError("expected expression");
    return nullptr;
  }

  auto whileNode = std::make_shared<QWhile>();
  whileNode->SetCondition(condition);

  // Parse body
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseWhile() - parsing body" << std::endl;
#endif
  auto body = std::make_shared<QCode>();
  ParseCode(body);
  whileNode->SetBody(body);

  // Expect 'wend'
  if (Check(TokenType::T_WEND)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseWhile() - consumed 'wend'" << std::endl;
#endif
  } else {
    std::cerr << "[ERROR] ParseWhile() - expected 'wend'" << std::endl;
  }

  return whileNode;
}

std::shared_ptr<QIncrement> Parser::ParseIncrement() {
#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseIncrement() - parsing increment/decrement"
            << std::endl;
#endif

  // Get variable name
  Token varToken = Advance();
  std::string varName = varToken.value;

#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseIncrement() - variable: " << varName << std::endl;
#endif

  // Get operator (++ or --)
  if (!Check(TokenType::T_OPERATOR)) {
    std::cerr << "[ERROR] ParseIncrement() - expected ++ or --" << std::endl;
    return nullptr;
  }

  Token opToken = Advance();
  bool isIncrement = (opToken.value == "++");

#if QLANG_DEBUG
  std::cout << "[DEBUG] ParseIncrement() - operator: " << opToken.value
            << std::endl;
#endif

  auto incrementNode = std::make_shared<QIncrement>(varName, isIncrement);

  // Consume optional semicolon
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
#if QLANG_DEBUG
    std::cout << "[DEBUG] ParseIncrement() - consumed semicolon" << std::endl;
#endif
  }

  return incrementNode;
}