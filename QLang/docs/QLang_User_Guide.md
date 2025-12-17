# QLang User Guide

A comprehensive guide to the QLang scripting language for the Quantum Engine.

---

## Table of Contents

1. [Introduction](#introduction)
2. [Basic Syntax](#basic-syntax)
3. [Data Types](#data-types)
4. [Variables](#variables)
5. [Operators](#operators)
6. [Control Flow](#control-flow)
7. [Functions](#functions)
8. [Classes](#classes)
9. [Inheritance](#inheritance)
10. [Generics](#generics)
11. [C++ Integration](#c-integration)
12. [Error Handling](#error-handling)

---

## Introduction

QLang is a statically-typed scripting language designed for game development and 3D engine integration. It features:

- **Clean, readable syntax** inspired by modern languages
- **Class-based OOP** with single inheritance
- **Generic types** for reusable components
- **C++ interoperability** for native function calls and engine integration
- **Strong typing** with explicit type declarations

---

## Basic Syntax

### Comments

```
// Single-line comment

/* Multi-line
   comment */
```

### Statement Termination

Statements are terminated by newlines or semicolons:

```
int32 x = 10
int32 y = 20;
```

### Code Blocks

Blocks use the `end` keyword to close:

```
if condition
    // code
end

class MyClass
    // members and methods
end

method void DoSomething()
    // code
end
```

---

## Data Types

### Primitive Types

| Type | Description | Example |
|------|-------------|---------|
| `int32` | 32-bit signed integer | `42` |
| `int64` | 64-bit signed integer | `9999999999` |
| `short` | 16-bit signed integer | `128` |
| `float32` | 32-bit floating point | `3.14` |
| `float64` | 64-bit floating point | `3.14159265359` |
| `string` | Text string | `"Hello World"` |
| `bool` | Boolean (true/false) | `true`, `false` |
| `cptr` | C pointer (void*) | For C++ interop |

### Special Values

```
null    // Null reference
true    // Boolean true
false   // Boolean false
```

---

## Variables

### Declaration with Type

```
int32 count = 0
float32 speed = 5.5
string name = "Player"
bool isActive = true
```

### Declaration without Initializer

Variables are initialized to default values:

```
int32 score       // Defaults to 0
string message    // Defaults to ""
bool flag         // Defaults to false
```

### Assignment

```
count = count + 1
name = "New Name"
```

---

## Operators

### Arithmetic Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Addition | `a + b` |
| `-` | Subtraction | `a - b` |
| `*` | Multiplication | `a * b` |
| `/` | Division | `a / b` |

### Comparison Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `==` | Equal | `a == b` |
| `!=` | Not equal | `a != b` |
| `<` | Less than | `a < b` |
| `>` | Greater than | `a > b` |
| `<=` | Less or equal | `a <= b` |
| `>=` | Greater or equal | `a >= b` |

### Logical Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `&&` | Logical AND | `a && b` |
| `\|\|` | Logical OR | `a \|\| b` |
| `!` | Logical NOT | `!a` |

### Increment/Decrement

```
count++    // Increment by 1
count--    // Decrement by 1
```

### Compound Assignment

```
x += 5     // x = x + 5
x -= 3     // x = x - 3
```

---

## Control Flow

### If / Else / Elseif

```
if health <= 0
    printf("Game Over")
elseif health < 20
    printf("Low Health!")
else
    printf("Health OK")
end
```

### While Loop

```
int32 i = 0
while i < 10
    printf("Count:", i)
    i++
wend
```

### For Loop

QLang uses a range-based for loop:

```
for x = 0 to 10
    printf("x =", x)
next

// With step
for x = 0 to 100 : 10
    printf("x =", x)    // 0, 10, 20, ...
next
```

### Return Statement

```
method int32 Add(int32 a, int32 b)
    return a + b
end
```

---

## Functions

### Calling Native Functions

Native functions are registered from C++ and called directly:

```
printf("Hello", 42, 3.14)
print("Debug message")
```

### Custom Functions via Methods

Functions are defined as methods within classes:

```
class Utils
    method int32 Max(int32 a, int32 b)
        if a > b
            return a
        end
        return b
    end
end

Utils u = new Utils()
int32 result = u.Max(10, 20)    // Returns 20
```

---

## Classes

### Class Definition

```
class Player
    // Member variables
    int32 health
    int32 score
    string name

    // Constructor (same name as class)
    method void Player()
        health = 100
        score = 0
        name = "Unknown"
    end

    // Methods
    method void TakeDamage(int32 amount)
        health = health - amount
        if health < 0
            health = 0
        end
    end

    method int32 GetHealth()
        return health
    end
end
```

### Creating Instances

```
Player p1 = new Player()
```

### Constructor Overloading

Classes support multiple constructors with different parameters:

```
class Person
    string name
    int32 age

    // Default constructor
    method void Person()
        name = "Unknown"
        age = 0
    end

    // Parameterized constructor
    method void Person(string n, int32 a)
        name = n
        age = a
    end
end

Person p1 = new Person()                    // Uses default
Person p2 = new Person("Alice", 30)         // Uses parameterized
```

### Method Overloading

Methods can have the same name with different parameter types:

```
class Math
    method int32 Add(int32 a, int32 b)
        return a + b
    end

    method string Add(string a, string b)
        return a + b
    end
end

Math m = new Math()
int32 sum = m.Add(5, 10)              // Returns 15
string text = m.Add("Hello", " World") // Returns "Hello World"
```

### Accessing Members

```
p1.health = 50
printf("Score:", p1.score)
p1.TakeDamage(25)
```

### Member Initialization

Members can have default values:

```
class Item
    int32 quantity = 1
    string name = "Unknown Item"
    float32 weight = 0.0
end
```

---

## Inheritance

### Basic Inheritance

Use parentheses after the class name to specify a parent:

```
class Animal
    int32 age
    string name

    method void Animal()
        age = 0
        name = "Animal"
    end

    method void Speak()
        printf("...")
    end
end

class Dog(Animal)
    string breed

    method void Dog()
        breed = "Unknown"
        name = "Dog"    // Accessing inherited member
    end

    // Override parent method
    method void Speak()
        printf("Woof!")
    end
end
```

### Constructor Chaining

Parent constructors are called automatically before child constructors:

```
Dog d = new Dog()
// 1. Animal members initialized
// 2. Animal() constructor called
// 3. Dog members initialized
// 4. Dog() constructor called
```

### Multi-Level Inheritance

Inheritance chains are fully supported:

```
class Entity
    int32 id
end

class Actor(Entity)
    int32 health
end

class Player(Actor)
    string name
end

// Player has: id, health, and name
```

### Method Lookup

Methods are searched from child to parent:

```
class Base
    method void DoSomething()
        printf("Base implementation")
    end
end

class Child(Base)
    // Inherits DoSomething from Base
end

Child c = new Child()
c.DoSomething()    // Calls Base.DoSomething
```

---

## Generics

### Generic Classes

Use angle brackets to define type parameters:

```
class Container<T>
    T value
    
    method void Container(T val)
        value = val
    end
    
    method T GetValue()
        return value
    end
end
```

### Using Generic Classes

Specify the type when creating instances:

```
class Node
    int32 data = 42
end

Container<Node> c = new Container<Node>(new Node())
```

### Multiple Type Parameters

```
class Pair<K, V>
    K key
    V value
    
    method void Pair(K k, V v)
        key = k
        value = v
    end
end
```

---

## C++ Integration

### Engine Integration API

From C++, you can interact with QLang classes using QRunner:

```cpp
// Parse and run script to register classes
QRunner runner(context, errorCollector);
runner.Run(program);

// Find a class definition
auto playerClass = runner.FindClass("Player");

// Create an instance
auto player = runner.CreateInstance("Player");

// Call methods
std::vector<QValue> args = {25};  // TakeDamage(25)
runner.CallMethod(player, "TakeDamage", args);

// Get return values
QValue result = runner.CallMethod(player, "GetHealth");
if (std::holds_alternative<int32_t>(result)) {
    int32_t health = std::get<int32_t>(result);
}
```

### Native Function Registration

Register C++ functions for QLang to call:

```cpp
QValue myPrint(QContext* ctx, const std::vector<QValue>& args) {
    for (const auto& arg : args) {
        std::cout << ValueToString(arg) << " ";
    }
    std::cout << std::endl;
    return std::monostate{};  // void return
}

context->AddFunc("printf", myPrint);
```

### Game Node Pattern

Typical usage for 3D engine game nodes:

```
// QLang script
class GameNode
    int32 x
    int32 y
    
    method int32 Update(float32 deltaTime)
        x = x + 1
        return 0
    end
    
    method void Render()
        printf("Draw at", x, y)
    end
end
```

```cpp
// C++ engine
auto node = runner.CreateInstance("GameNode");

// Game loop
while (running) {
    std::vector<QValue> args = {deltaTime};
    runner.CallMethod(node, "Update", args);
    runner.CallMethod(node, "Render");
}
```

---

## Error Handling

### Parse Errors

Errors are reported with line and column information:

```
Error at line 15, column 8: Unknown type 'int23'
```

### Runtime Errors

Runtime errors include a call stack trace:

```
[RUNTIME ERROR] unknown function: nonexistent
  at SomeMethod
  at Main
```

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `unknown parent class 'X'` | Parent not defined before child | Define parent class first |
| `expected 'end' to close class` | Missing `end` keyword | Add `end` after class body |
| `method not found` | Wrong name or parameter types | Check method signature |

---

## Appendix: Reserved Keywords

```
bool    class   cptr    else    elseif  end     false
float32 float64 for     if      int32   int64   method
module  new     next    null    return  short   string
this    to      true    void    wend    while
```

---

*QLang v1.0 - Quantum Engine Scripting Language*
