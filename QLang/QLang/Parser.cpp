#include "Parser.h"

Parser::Parser(const std::vector<Token> &tokens) : m_Tokens(tokens) {
  std::cout << "[DEBUG] Parser created with " << tokens.size() << " tokens"
            << std::endl;
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
    } else {
      // Parse code block for program-level statements
      break;
    }
  }

  // Parse program code block
  ParseCode(program->GetCode());

  std::cout << "[DEBUG] ParseProgram() - finished parsing" << std::endl;
  return program;
}

void Parser::ParseCode(std::shared_ptr<QCode> code) {
  std::cout << "[DEBUG] ParseCode() - parsing code block" << std::endl;

  while (!IsAtEnd()) {
    Token current = Peek();
    std::cout << "[DEBUG] ParseCode() - current token: " << current.value
              << " at line " << current.line << std::endl;

    // Check for block end markers
    if (current.type == TokenType::T_END || current.type == TokenType::T_EOF) {
      std::cout << "[DEBUG] ParseCode() - reached end of block" << std::endl;
      break;
    }

    // Check for return statement
    if (current.type == TokenType::T_RETURN) {
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
    } else if (current.type == TokenType::T_IDENTIFIER) {
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
      } else {
        auto statement = ParseStatement();
        if (statement) {
          code->AddNode(statement);
        }
      }
    } else {
      // Skip tokens we don't handle yet
      std::cout << "[DEBUG] ParseCode() - skipping token: " << current.value
                << std::endl;
      Advance();
    }
  }

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
    std::cout << "[DEBUG] ParseStatement() - no parameters (no '(' found)"
              << std::endl;
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
  while (!IsAtEnd() && !Check(TokenType::T_END_OF_LINE) &&
         !Check(TokenType::T_EOF)) {
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
         type == TokenType::T_BOOL;
}

std::shared_ptr<QVariableDecl> Parser::ParseVariableDecl() {
  std::cout << "[DEBUG] ParseVariableDecl() - parsing variable declaration"
            << std::endl;

  // Get the type token
  Token typeToken = Advance();
  std::cout << "[DEBUG] ParseVariableDecl() - type: " << typeToken.value
            << std::endl;

  // Expect identifier (variable name)
  if (!Check(TokenType::T_IDENTIFIER)) {
    std::cerr << "[ERROR] ParseVariableDecl() - expected identifier after type"
              << std::endl;
    return nullptr;
  }

  Token nameToken = Advance();
  std::cout << "[DEBUG] ParseVariableDecl() - name: " << nameToken.value
            << std::endl;

  auto varDecl =
      std::make_shared<QVariableDecl>(typeToken.type, nameToken.value);

  // Check for initializer (= expression)
  if (Check(TokenType::T_OPERATOR) && Peek().value == "=") {
    Advance(); // consume '='
    std::cout << "[DEBUG] ParseVariableDecl() - parsing initializer"
              << std::endl;

    auto initializer = ParseExpression();
    varDecl->SetInitializer(initializer);
  }

  // Consume end of line
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
    std::cout << "[DEBUG] ParseVariableDecl() - consumed semicolon"
              << std::endl;
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

  auto cls = std::make_shared<QClass>(nameToken.value);

  // Parse class body until END token
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
    std::cerr << "[ERROR] ParseClass() - expected 'end' to close class"
              << std::endl;
  }

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

  // Expect '(' for parameters
  if (Check(TokenType::T_LPAREN)) {
    Advance(); // consume '('

    // Parse parameter list: type name, type name, ...
    while (!IsAtEnd() && !Check(TokenType::T_RPAREN)) {
      // Expect type token
      if (IsTypeToken(Peek().type)) {
        TokenType paramType = Peek().type;
        Advance(); // consume type

        // Expect parameter name
        if (Check(TokenType::T_IDENTIFIER)) {
          std::string paramName = Peek().value;
          Advance(); // consume name

          method->AddParameter(paramType, paramName);
          std::cout << "[DEBUG] ParseMethod() - parsed param: " << paramName
                    << std::endl;
        } else {
          std::cerr << "[ERROR] ParseMethod() - expected parameter name"
                    << std::endl;
        }
      } else {
        std::cerr << "[ERROR] ParseMethod() - expected parameter type"
                  << std::endl;
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
    std::cerr << "[ERROR] ParseMethod() - expected 'end' to close method"
              << std::endl;
  }

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

  // Expect instance name (identifier)
  if (!Check(TokenType::T_IDENTIFIER)) {
    std::cerr << "[ERROR] ParseInstanceDecl() - expected instance name"
              << std::endl;
    return nullptr;
  }

  Token instanceNameToken = Advance();
  std::cout << "[DEBUG] ParseInstanceDecl() - instance: "
            << instanceNameToken.value << std::endl;

  auto instanceDecl = std::make_shared<QInstanceDecl>(classNameToken.value,
                                                      instanceNameToken.value);

  // Expect '='
  if (!Check(TokenType::T_OPERATOR) || Peek().value != "=") {
    std::cerr << "[ERROR] ParseInstanceDecl() - expected '='" << std::endl;
    return nullptr;
  }
  Advance(); // consume '='

  // Expect 'new'
  if (!Check(TokenType::T_NEW)) {
    std::cerr << "[ERROR] ParseInstanceDecl() - expected 'new'" << std::endl;
    return nullptr;
  }
  Advance(); // consume 'new'

  // Expect constructor class name (should match)
  if (!Check(TokenType::T_IDENTIFIER) || Peek().value != classNameToken.value) {
    std::cerr
        << "[ERROR] ParseInstanceDecl() - constructor class name doesn't match"
        << std::endl;
    // Still continue for flexibility
  }
  if (Check(TokenType::T_IDENTIFIER)) {
    Advance(); // consume constructor class name
  }

  // Expect '(' for constructor args
  if (Check(TokenType::T_LPAREN)) {
    Advance(); // consume '('

    // Parse constructor arguments (as expression for now)
    if (!Check(TokenType::T_RPAREN)) {
      auto args = ParseExpression();
      instanceDecl->SetConstructorArgs(args);
    }

    if (Check(TokenType::T_RPAREN)) {
      Advance(); // consume ')'
    }
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
      std::cerr << "[ERROR] ParseMethodCall() - expected identifier after '.'"
                << std::endl;
      return nullptr;
    }

    Token next = Advance();
    pathParts.push_back(next.value);
    std::cout << "[DEBUG] ParseMethodCall() - path part: " << next.value
              << std::endl;
  }

  // Must have at least 2 parts: instance and method
  if (pathParts.size() < 2) {
    std::cerr << "[ERROR] ParseMethodCall() - incomplete method call"
              << std::endl;
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
    std::cerr << "[ERROR] ParseMemberAssign() - expected '.'" << std::endl;
    return nullptr;
  }
  Advance(); // consume '.'

  // Expect member name (identifier)
  if (!Check(TokenType::T_IDENTIFIER)) {
    std::cerr << "[ERROR] ParseMemberAssign() - expected member name"
              << std::endl;
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
      std::cerr
          << "[ERROR] ParseMemberAssign() - expected member name after '.'"
          << std::endl;
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
    std::cerr << "[ERROR] ParseMemberAssign() - expected '='" << std::endl;
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

  // Expect member name (identifier)
  if (!Check(TokenType::T_IDENTIFIER)) {
    std::cerr << "[ERROR] ParseClassTypeMember() - expected member name"
              << std::endl;
    return nullptr;
  }

  Token memberNameToken = Advance();
  std::string memberName = memberNameToken.value;
  std::cout << "[DEBUG] ParseClassTypeMember() - member name: " << memberName
            << std::endl;

  // Create variable declaration with T_IDENTIFIER type to indicate class type
  auto member =
      std::make_shared<QVariableDecl>(TokenType::T_IDENTIFIER, memberName);

  // Store the class type name for later reference
  // We'll use the initializer expression to carry the class type info

  // Expect '=' for initialization
  if (Check(TokenType::T_OPERATOR) && Peek().value == "=") {
    Advance(); // consume '='

    // Parse the initializer expression (new ClassName())
    auto initExpr = ParseExpression();
    member->SetInitializer(initExpr);

    std::cout << "[DEBUG] ParseClassTypeMember() - parsed initializer"
              << std::endl;
  }

  // Consume semicolon
  if (Check(TokenType::T_END_OF_LINE)) {
    Advance();
    std::cout << "[DEBUG] ParseClassTypeMember() - consumed semicolon"
              << std::endl;
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
