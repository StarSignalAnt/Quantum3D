#include "Parser.h"
#include "QAssign.h"
#include <algorithm>
#include <iostream>

Parser::Parser(const std::vector<Token> &tokens) : m_Tokens(tokens) {
  std::cout << "[DEBUG] Parser created with " << tokens.size() << " tokens"
            << std::endl;
}

Parser::Parser(const std::vector<Token> &tokens,
               std::shared_ptr<QErrorCollector> errorCollector)
    : m_Tokens(tokens), m_ErrorCollector(errorCollector) {
  std::cout << "[DEBUG] Parser created with " << tokens.size()
            << " tokens and error collector" << std::endl;
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

Parser::~Parser() { std::cout << "[DEBUG] Parser destroyed" << std::endl; }

std::shared_ptr<QProgram> Parser::Parse() {
  std::cout << "[DEBUG] Parse() called - starting parse" << std::endl;
  return ParseProgram();
}

std::shared_ptr<QProgram> Parser::ParseProgram() {
  std::cout << "[DEBUG] ParseProgram() - creating QProgram node" << std::endl;

  auto program = std::make_shared<QProgram>();

  // Parse classes at program level
  while (!IsAtEnd()) {
    Token current = Peek();

    if (current.type == TokenType::T_CLASS) {
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

  std::cout << "[DEBUG] ParseProgram() - parsed successfully" << std::endl;
  return program;
}

void Parser::ParseCode(std::shared_ptr<QCode> code) {
  std::cout << "[DEBUG] ParseCode() - parsing code block" << std::endl;

  while (!IsAtEnd()) {
    Token current = Peek();
    std::cout << "[DEBUG] ParseCode() - current token: " << current.value
              << " at line " << current.line << std::endl;

    // Check for block end markers
    if (current.type == TokenType::T_END || current.type == TokenType::T_EOF ||
        current.type == TokenType::T_ELSEIF ||
        current.type == TokenType::T_ELSE ||
        current.type == TokenType::T_NEXT ||
        current.type == TokenType::T_WEND) {
      std::cout << "[DEBUG] ParseCode() - reached end of block" << std::endl;
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
      // Check for variable declaration (starts with type token)
    } else if (IsTypeToken(current.type)) {
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

          // Verify if it is a KNOWN type before committing to
          // ParseVariableDecl. If 'i2f' is not a type, we don't want to report
          // "Unknown Type 'i2f'". We want to report "Unexpected token 'i2f'"
          // (Syntax Error).
          bool isKnownType =
              IsTypeToken(current.type) || IsClassName(current.value) ||
              std::find(m_CurrentTypeParams.begin(), m_CurrentTypeParams.end(),
                        current.value) != m_CurrentTypeParams.end();

          if (isKnownType) {
            auto varDecl = ParseVariableDecl();
            if (varDecl) {
              code->AddNode(varDecl);
            }
          } else {
            // It looks like "Id Id" but first Id is NOT a known type.
            // It's likely a typo (e.g. "i2f Val").
            // Report Unexpected Token error and recover.
            ReportError("Unexpected token '" + current.value + "'");
            RecoverToNextStatement();
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

  std::cout << "[DEBUG] ParseCode() - finished parsing block" << std::endl;
}

std::shared_ptr<QStatement> Parser::ParseStatement() {
  Token identifier = Peek();
  std::cout << "Parsing " << identifier.value << std::endl;

  // Consume the identifier
  Advance();

  auto statement = std::make_shared<QStatement>(identifier.value);

  // Check for parameters - look for '('
  if (Check(TokenType::T_LPAREN)) {
    std::cout << "[DEBUG] ParseStatement() - found '(', parsing parameters"
              << std::endl;
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
    std::cout << "[DEBUG] ParseStatement() - consumed end of line" << std::endl;
  }

  return statement;
}

std::shared_ptr<QParameters> Parser::ParseParameters() {
  std::cout << "[DEBUG] ParseParameters() - starting" << std::endl;

  auto params = std::make_shared<QParameters>();

  // Consume '('
  if (Check(TokenType::T_LPAREN)) {
    Advance();
    std::cout << "[DEBUG] ParseParameters() - consumed '('" << std::endl;
  }

  // Check for empty parameters ()
  if (Check(TokenType::T_RPAREN)) {
    Advance();
    std::cout << "[DEBUG] ParseParameters() - empty parameters, consumed ')'"
              << std::endl;
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
    std::cout << "[DEBUG] ParseParameters() - consumed ','" << std::endl;

    expr = ParseExpression();
    if (expr) {
      params->AddParameter(expr);
    }
  }

  // Consume ')'
  if (Check(TokenType::T_RPAREN)) {
    Advance();
    std::cout << "[DEBUG] ParseParameters() - consumed ')'" << std::endl;
  } else {
    std::cerr << "[ERROR] ParseParameters() - expected ')'" << std::endl;
  }

  return params;
}

std::shared_ptr<QExpression> Parser::ParseExpression() {
  std::cout << "[DEBUG] ParseExpression() - starting" << std::endl;

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
      expr->AddElement(current);
      Advance();
    }
  }

  std::cout << "[DEBUG] ParseExpression() - finished with "
            << expr->GetElements().size() << " elements" << std::endl;

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
         type == TokenType::T_BOOL || type == TokenType::T_CPTR;
}

std::shared_ptr<QVariableDecl> Parser::ParseVariableDecl() {
  std::cout << "[DEBUG] ParseVariableDecl() - parsing variable declaration"
            << std::endl;

  // Get the type token
  Token typeToken = Advance();
  std::cout << "[DEBUG] ParseVariableDecl() - type: " << typeToken.value
            << std::endl;

  // STRICT TYPE CHECKING
  // Ensure the type is valid: Primitive, Registered Class, or Generic Param
  bool isValidType =
      IsTypeToken(typeToken.type) || IsClassName(typeToken.value) ||
      std::find(m_CurrentTypeParams.begin(), m_CurrentTypeParams.end(),
                typeToken.value) != m_CurrentTypeParams.end();

  if (!isValidType) {
    ReportError("Unknown type '" + typeToken.value + "'");
    // We return nullptr to stop parsing this statement, but we consume the
    // token. ParseCode needs to handle this gracefully (it continues loop).
    return nullptr;
  }

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
  std::cout << "[DEBUG] ParseVariableDecl() - name: " << nameToken.value
            << std::endl;

  auto varDecl = std::make_shared<QVariableDecl>(
      typeToken.type, nameToken.value, typeToken.value);
  varDecl->SetTypeParameters(typeParams);

  // Check for initializer (= expression)
  if (Check(TokenType::T_OPERATOR) && Peek().value == "=") {
    Advance(); // consume '='
    std::cout << "[DEBUG] ParseVariableDecl() - parsing initializer"
              << std::endl;

    auto initializer = ParseExpression();
    varDecl->SetInitializer(initializer);
  }

  // Consume end of line (Mandatory for declarations)
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
    std::cout << "[DEBUG] ParseVariableDecl() - consumed semicolon"
              << std::endl;
  } else if (!Check(TokenType::T_EOF)) {
    // If NOT at EOF, report error (missing semicolon)
    // This will catch "i2f Val == null" cases where '==' is found instead of
    // EOL
    ReportError("Expected end of line (or ';') after variable declaration");
  }

  return varDecl;
}

std::shared_ptr<QClass> Parser::ParseClass() {
  std::cout << "[DEBUG] ParseClass() - parsing class definition" << std::endl;

  // Consume 'class' keyword
  Advance();

  // Expect class name (identifier)
  if (!Check(TokenType::T_IDENTIFIER)) {
    std::cerr << "[ERROR] ParseClass() - expected class name after 'class'"
              << std::endl;
    return nullptr;
  }

  Token nameToken = Advance();
  std::cout << "[DEBUG] ParseClass() - class name: " << nameToken.value
            << std::endl;

  // Register class name immediately so self-referential members (e.g. Node
  // next) works
  m_ClassNames.insert(nameToken.value);

  // Set current context header (Class Name)
  std::string previousContext = m_CurrentContext;
  m_CurrentContext = nameToken.value;

  auto cls = std::make_shared<QClass>(nameToken.value);

  // Check for inheritance syntax: class Name(ParentClass)
  if (Check(TokenType::T_LPAREN)) {
    Advance(); // consume '('
    std::cout << "[DEBUG] ParseClass() - parsing parent class" << std::endl;

    if (!Check(TokenType::T_IDENTIFIER)) {
      ReportError("expected parent class name after '('");
    } else {
      Token parentToken = Advance();

      // Validate that the parent class exists
      if (!IsClassName(parentToken.value)) {
        ReportError("unknown parent class '" + parentToken.value +
                    "' - parent class must be defined before child class");
      } else {
        cls->SetParentClass(parentToken.value);
        std::cout << "[DEBUG] ParseClass() - parent class: "
                  << parentToken.value << std::endl;
      }
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
    std::cout << "[DEBUG] ParseClass() - parsing generic type parameters"
              << std::endl;

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
      std::cout << "[DEBUG] ParseClass() - type parameter: " << typeParam.value
                << std::endl;

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
      }
      // Parse generic type parameter members (T, K, V, etc.)
    } else if (current.type == TokenType::T_IDENTIFIER &&
               std::find(m_CurrentTypeParams.begin(), m_CurrentTypeParams.end(),
                         current.value) != m_CurrentTypeParams.end()) {
      // This is a generic type parameter used as a member type
      std::cout << "[DEBUG] ParseClass() - parsing generic type member: "
                << current.value << std::endl;
      auto member = ParseVariableDecl();
      if (member) {
        cls->AddMember(member);
      }
      // Parse class-type member variables (ClassName varName = new ...)
    } else if (current.type == TokenType::T_IDENTIFIER &&
               IsClassName(current.value)) {
      // This is a class-type member declaration
      std::cout << "[DEBUG] ParseClass() - parsing class-type member: "
                << current.value << std::endl;
      auto member = ParseClassTypeMember();
      if (member) {
        cls->AddMember(member);
      }
    } else if (current.type == TokenType::T_END_OF_LINE) {
      Advance(); // Skip newlines in class body
    } else {
      // Skip unknown tokens inside class
      std::cout << "[DEBUG] ParseClass() - skipping token: " << current.value
                << std::endl;
      Advance();
    }
  }

  // Consume 'end' keyword
  if (Check(TokenType::T_END)) {
    Advance();
    std::cout << "[DEBUG] ParseClass() - consumed 'end'" << std::endl;
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
  std::cout << "[DEBUG] ParseMethod() - parsing method" << std::endl;

  // Consume 'method' keyword
  Advance();

  // Expect return type (void or type token)
  TokenType returnType = TokenType::T_VOID;
  Token typeToken = Peek();

  if (typeToken.type == TokenType::T_VOID || IsTypeToken(typeToken.type)) {
    returnType = typeToken.type;
    Advance();
    std::cout << "[DEBUG] ParseMethod() - return type: " << typeToken.value
              << std::endl;
  }

  // Expect method name (identifier)
  if (!Check(TokenType::T_IDENTIFIER)) {
    std::cerr << "[ERROR] ParseMethod() - expected method name" << std::endl;
    return nullptr;
  }

  Token nameToken = Advance();
  std::cout << "[DEBUG] ParseMethod() - method name: " << nameToken.value
            << std::endl;

  auto method = std::make_shared<QMethod>(nameToken.value);
  method->SetReturnType(returnType);

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
          std::cout << "[DEBUG] ParseMethod() - parsed param: " << paramName
                    << " (type: " << paramTypeName << ")" << std::endl;
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
      std::cout << "[DEBUG] ParseMethod() - consumed parameters, count: "
                << method->GetParameters().size() << std::endl;
    }
  }

  // Parse method body until 'end'
  ParseCode(method->GetBody());

  // Consume 'end' keyword for method
  if (Check(TokenType::T_END)) {
    Advance();
    std::cout << "[DEBUG] ParseMethod() - consumed 'end'" << std::endl;
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
  std::cout << "[DEBUG] ParseInstanceDecl() - parsing instance declaration"
            << std::endl;

  // Get class name
  Token classNameToken = Advance();
  std::cout << "[DEBUG] ParseInstanceDecl() - class: " << classNameToken.value
            << std::endl;

  // Check for generic type arguments <type1, type2, ...>
  std::vector<std::string> typeArgs;
  if (Check(TokenType::T_LESS)) {
    Advance(); // consume '<'
    std::cout << "[DEBUG] ParseInstanceDecl() - parsing type arguments"
              << std::endl;

    while (!IsAtEnd() && !Check(TokenType::T_GREATER)) {
      // Get the type name (could be identifier or type keyword)
      Token typeArg = Advance();
      typeArgs.push_back(typeArg.value);
      std::cout << "[DEBUG] ParseInstanceDecl() - type arg: " << typeArg.value
                << std::endl;

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
  std::cout << "[DEBUG] ParseInstanceDecl() - instance: "
            << instanceNameToken.value << std::endl;

  auto instanceDecl = std::make_shared<QInstanceDecl>(classNameToken.value,
                                                      instanceNameToken.value);

  // Set type arguments if any
  if (!typeArgs.empty()) {
    instanceDecl->SetTypeArguments(typeArgs);
  }

  // Expect '='
  if (!Check(TokenType::T_OPERATOR) || Peek().value != "=") {
    ReportError("expected '='");
    return nullptr;
  }
  Advance(); // consume '='

  // Expect 'new'
  if (!Check(TokenType::T_NEW)) {
    ReportError("expected 'new'");
    return nullptr;
  }
  Advance(); // consume 'new'

  // Expect constructor class name (should match)
  if (!Check(TokenType::T_IDENTIFIER) || Peek().value != classNameToken.value) {
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

  // Consume semicolon
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
    std::cout << "[DEBUG] ParseInstanceDecl() - consumed semicolon"
              << std::endl;
  }

  return instanceDecl;
}

std::shared_ptr<QMethodCall> Parser::ParseMethodCall() {
  std::cout << "[DEBUG] ParseMethodCall() - parsing method call" << std::endl;

  // Get first identifier (instance name)
  Token firstToken = Advance();
  std::cout << "[DEBUG] ParseMethodCall() - first: " << firstToken.value
            << std::endl;

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
    std::cout << "[DEBUG] ParseMethodCall() - path part: " << next.value
              << std::endl;
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

  std::cout << "[DEBUG] ParseMethodCall() - instance path: " << instancePath
            << ", method: " << methodName << std::endl;

  auto methodCall = std::make_shared<QMethodCall>(instancePath, methodName);

  // Parse arguments if present
  if (Check(TokenType::T_LPAREN)) {
    auto args = ParseParameters();
    methodCall->SetArguments(args);
  }

  // Consume semicolon
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
    std::cout << "[DEBUG] ParseMethodCall() - consumed semicolon" << std::endl;
  }

  return methodCall;
}

std::shared_ptr<QMemberAssign> Parser::ParseMemberAssign() {
  std::cout << "[DEBUG] ParseMemberAssign() - parsing member assignment"
            << std::endl;

  // Get instance name
  Token instanceNameToken = Advance();
  std::cout << "[DEBUG] ParseMemberAssign() - instance: "
            << instanceNameToken.value << std::endl;

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
  std::cout << "[DEBUG] ParseMemberAssign() - member: " << memberNameToken.value
            << std::endl;

  // Continue consuming .identifier patterns until we hit '='
  while (Check(TokenType::T_DOT)) {
    Advance(); // consume '.'
    if (!Check(TokenType::T_IDENTIFIER)) {
      ReportError("expected member name after '.'");
      return nullptr;
    }
    Token nextMember = Advance();
    memberPath += "." + nextMember.value;
    std::cout << "[DEBUG] ParseMemberAssign() - chained member: "
              << nextMember.value << std::endl;
  }

  std::cout << "[DEBUG] ParseMemberAssign() - full path: "
            << instanceNameToken.value << "." << memberPath << std::endl;

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

  std::cout << "[DEBUG] ParseMemberAssign() - parsed value expression"
            << std::endl;

  // Consume semicolon
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
    std::cout << "[DEBUG] ParseMemberAssign() - consumed semicolon"
              << std::endl;
  }

  return memberAssign;
}

std::shared_ptr<QVariableDecl> Parser::ParseClassTypeMember() {
  std::cout << "[DEBUG] ParseClassTypeMember() - parsing class-type member"
            << std::endl;

  // Get class type name (e.g., "other")
  Token classTypeToken = Advance();
  std::string classTypeName = classTypeToken.value;
  std::cout << "[DEBUG] ParseClassTypeMember() - class type: " << classTypeName
            << std::endl;

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
  std::cout << "[DEBUG] ParseClassTypeMember() - member name: " << memberName
            << std::endl;

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

    std::cout << "[DEBUG] ParseClassTypeMember() - parsed initializer"
              << std::endl;
  }

  // Consume semicolon (Mandatory)
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
    std::cout << "[DEBUG] ParseClassTypeMember() - consumed semicolon"
              << std::endl;
  } else if (!Check(TokenType::T_EOF)) {
    ReportError("Expected end of line (or ';') after member declaration");
  }

  return member;
}

std::shared_ptr<QReturn> Parser::ParseReturn() {
  std::cout << "[DEBUG] ParseReturn() - parsing return statement" << std::endl;

  // Consume 'return' keyword
  Advance();

  auto returnStmt = std::make_shared<QReturn>();

  // Parse optional expression (if not immediately at semicolon)
  if (!Check(TokenType::T_END_OF_LINE) && !Check(TokenType::T_EOF) &&
      !Check(TokenType::T_END)) {
    auto expr = ParseExpression();
    returnStmt->SetExpression(expr);
    std::cout << "[DEBUG] ParseReturn() - parsed return expression"
              << std::endl;
  }

  // Consume semicolon
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
    std::cout << "[DEBUG] ParseReturn() - consumed semicolon" << std::endl;
  }

  return returnStmt;
}

std::shared_ptr<QAssign> Parser::ParseAssign() {
  std::cout << "[DEBUG] ParseAssign() - parsing assignment" << std::endl;

  // Get variable name
  Token nameToken = Advance();
  std::cout << "[DEBUG] ParseAssign() - variable: " << nameToken.value
            << std::endl;

  // Expect '='
  if (!Check(TokenType::T_OPERATOR) || Peek().value != "=") {
    ReportError("expected '='");
    return nullptr;
  }
  Advance(); // consume '='

  auto assign = std::make_shared<QAssign>(nameToken.value);

  // Parse expression
  auto expr = ParseExpression();
  assign->SetValueExpression(expr);

  // Consume semicolon
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
    std::cout << "[DEBUG] ParseAssign() - consumed semicolon" << std::endl;
  }

  return assign;
}

std::shared_ptr<QIf> Parser::ParseIf() {
  std::cout << "[DEBUG] ParseIf() - parsing if statement" << std::endl;
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
    std::cout << "[DEBUG] ParseIf() - parsing elseif" << std::endl;
    Advance(); // consume 'elseif'

    auto elseIfCond = ParseExpression();
    auto elseIfBlock = std::make_shared<QCode>();
    ParseCode(elseIfBlock);

    ifNode->AddElseIf(elseIfCond, elseIfBlock);
  }

  if (Check(TokenType::T_ELSE)) {
    std::cout << "[DEBUG] ParseIf() - parsing else" << std::endl;
    Advance(); // consume 'else'

    auto elseBlock = std::make_shared<QCode>();
    ParseCode(elseBlock);
    ifNode->SetElse(elseBlock);
  }

  Consume(TokenType::T_END, "Expected 'end' after if statement");
  return ifNode;
}

std::shared_ptr<QFor> Parser::ParseFor() {
  std::cout << "[DEBUG] ParseFor() - parsing for loop" << std::endl;

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
    std::cout << "[DEBUG] ParseFor() - type declared: " << current.value
              << std::endl;
  }

  // Expect variable name
  if (!Check(TokenType::T_IDENTIFIER)) {
    std::cerr << "[ERROR] ParseFor() - expected variable name" << std::endl;
    return nullptr;
  }

  Token varToken = Advance();
  auto forNode = std::make_shared<QFor>(varToken.value);

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
  std::cout << "[DEBUG] ParseFor() - parsing body" << std::endl;
  auto body = std::make_shared<QCode>();
  ParseCode(body);
  forNode->SetBody(body);

  // Expect 'next'
  if (Check(TokenType::T_NEXT)) {
    Advance();
    std::cout << "[DEBUG] ParseFor() - consumed 'next'" << std::endl;
  } else {
    std::cerr << "[ERROR] ParseFor() - expected 'next'" << std::endl;
  }

  return forNode;
}

std::shared_ptr<QWhile> Parser::ParseWhile() {
  std::cout << "[DEBUG] ParseWhile() - parsing while loop" << std::endl;

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
  std::cout << "[DEBUG] ParseWhile() - parsing body" << std::endl;
  auto body = std::make_shared<QCode>();
  ParseCode(body);
  whileNode->SetBody(body);

  // Expect 'wend'
  if (Check(TokenType::T_WEND)) {
    Advance();
    std::cout << "[DEBUG] ParseWhile() - consumed 'wend'" << std::endl;
  } else {
    std::cerr << "[ERROR] ParseWhile() - expected 'wend'" << std::endl;
  }

  return whileNode;
}

std::shared_ptr<QIncrement> Parser::ParseIncrement() {
  std::cout << "[DEBUG] ParseIncrement() - parsing increment/decrement"
            << std::endl;

  // Get variable name
  Token varToken = Advance();
  std::string varName = varToken.value;

  std::cout << "[DEBUG] ParseIncrement() - variable: " << varName << std::endl;

  // Get operator (++ or --)
  if (!Check(TokenType::T_OPERATOR)) {
    std::cerr << "[ERROR] ParseIncrement() - expected ++ or --" << std::endl;
    return nullptr;
  }

  Token opToken = Advance();
  bool isIncrement = (opToken.value == "++");

  std::cout << "[DEBUG] ParseIncrement() - operator: " << opToken.value
            << std::endl;

  auto incrementNode = std::make_shared<QIncrement>(varName, isIncrement);

  // Consume optional semicolon
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
    std::cout << "[DEBUG] ParseIncrement() - consumed semicolon" << std::endl;
  }

  return incrementNode;
}