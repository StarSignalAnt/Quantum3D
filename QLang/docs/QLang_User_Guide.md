# QLang User Guide

A comprehensive guide to the QLang scripting language for the Quantum Engine.

---

## Table of Contents

1. [Introduction](#introduction)
2. [Basic Syntax](#basic-syntax)
3. [Data Types](#data-types)
4. [Variables](#variables)
5. [Operators](#operators)
6. [Operator Overloading](#operator-overloading) *(New in v0.2)*
7. [Control Flow](#control-flow)
8. [Functions](#functions)
9. [Classes](#classes)
10. [Inheritance](#inheritance)
11. [Generics](#generics)
12. [Built-In Classes](#built-in-classes) *(New in v0.2)*
13. [Engine Integration](#engine-integration)
14. [Error Handling](#error-handling)
15. [Changelog](#changelog)

---

## Introduction

QLang is a statically-typed scripting language designed for game development and 3D engine integration. It features:

- **Clean, readable syntax** inspired by modern languages
- **Class-based OOP** with single inheritance
- **Operator overloading** for custom types *(New in v0.2)*
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

---

## Operator Overloading

*New in v0.2*

Classes can overload arithmetic operators by defining specific methods. When an operator is used with a class instance on the left side, the corresponding method is called automatically.

### Operator to Method Mapping

| Operator | Method Name |
|----------|-------------|
| `+` | `Plus` |
| `-` | `Minus` |
| `*` | `Multiply` |
| `/` | `Divide` |

### Example: Vec3 Arithmetic

```
class Vec3
    float32 X
    float32 Y
    float32 Z

    method void Vec3(float32 x, float32 y, float32 z)
        X = x
        Y = y
        Z = z
    end

    // Overload + for Vec3 + Vec3
    method Vec3 Plus(Vec3 right)
        return new Vec3(X + right.X, Y + right.Y, Z + right.Z)
    end

    // Overload + for Vec3 + scalar (function overloading)
    method Vec3 Plus(float32 scalar)
        return new Vec3(X + scalar, Y + scalar, Z + scalar)
    end

    // Overload * for Vec3 * scalar
    method Vec3 Multiply(float32 scalar)
        return new Vec3(X * scalar, Y * scalar, Z * scalar)
    end
end

// Usage
Vec3 a = new Vec3(1, 2, 3)
Vec3 b = new Vec3(4, 5, 6)
Vec3 sum = a + b           // Calls a.Plus(b)
Vec3 scaled = a * 2.0      // Calls a.Multiply(2.0)
Vec3 offset = a + 10.0     // Calls a.Plus(10.0)
```

### Notes

- Operator overloading supports **function overloading**. Define multiple versions of `Plus` to handle different argument types.
- If no matching method is found, the default behavior (error for class instances) is used.

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

### Engine Native Functions

*New in v0.2*

| Function | Parameters | Description |
|----------|------------|-------------|
| `printf` | `...args` | Print values with type info |
| `print` | `...args` | Print values |
| `NodeTurn` | `cptr node, Vec3 rotation` | Rotate a scene node |
| `NodeSetPosition` | `cptr node, Vec3 position` | Set node position |

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

    method float32 Add(float32 a, float32 b)
        return a + b
    end
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
end

class Dog(Animal)
    string breed

    method void Dog()
        breed = "Unknown"
        name = "Dog"    // Accessing inherited member
    end
end
```

### Constructor Chaining

Parent constructors are called automatically before child constructors.

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

```
Container<int32> c = new Container<int32>(42)
int32 val = c.GetValue()
```

---

## Built-In Classes

*New in v0.2*

QLang provides several built-in classes for common game development tasks. These are automatically loaded when the engine starts.

---

### Vec3

A 3D vector class with comprehensive operator overloading and utility methods.

**Location:** `engine/qlang/classes/Vec3.q`

#### Members

| Member | Type | Description |
|--------|------|-------------|
| `X` | `float32` | X component |
| `Y` | `float32` | Y component |
| `Z` | `float32` | Z component |

#### Constructors

| Signature | Description |
|-----------|-------------|
| `Vec3()` | Creates a zero vector (0, 0, 0) |
| `Vec3(float32 x, float32 y, float32 z)` | Creates a vector with specified components |
| `Vec3(float32 value)` | Creates a vector with all components set to the same value |

#### Operator Overloads

| Operator | Method | Parameters | Returns | Description |
|----------|--------|------------|---------|-------------|
| `+` | `Plus` | `Vec3 right` | `Vec3` | Component-wise addition |
| `+` | `Plus` | `float32 scalar` | `Vec3` | Add scalar to all components |
| `-` | `Minus` | `Vec3 right` | `Vec3` | Component-wise subtraction |
| `-` | `Minus` | `float32 scalar` | `Vec3` | Subtract scalar from all components |
| `*` | `Multiply` | `Vec3 right` | `Vec3` | Component-wise multiplication |
| `*` | `Multiply` | `float32 scalar` | `Vec3` | Scale all components |
| `/` | `Divide` | `Vec3 right` | `Vec3` | Component-wise division |
| `/` | `Divide` | `float32 scalar` | `Vec3` | Divide all components by scalar |

#### Utility Methods

| Method | Parameters | Returns | Description |
|--------|------------|---------|-------------|
| `Dot` | `Vec3 other` | `float32` | Dot product of two vectors |
| `Cross` | `Vec3 other` | `Vec3` | Cross product of two vectors |
| `LengthSquared` | — | `float32` | Squared length (faster than computing actual length) |
| `Negate` | — | `Vec3` | Returns the negated vector (-X, -Y, -Z) |
| `Lerp` | `Vec3 target, float32 t` | `Vec3` | Linear interpolation towards target by factor t |
| `Print` | — | `void` | Prints the vector to console |

#### Factory Methods

| Method | Returns | Description |
|--------|---------|-------------|
| `Zero()` | `Vec3(0, 0, 0)` | Zero vector |
| `One()` | `Vec3(1, 1, 1)` | Unit vector (all ones) |
| `Up()` | `Vec3(0, 1, 0)` | Up direction (+Y) |
| `Down()` | `Vec3(0, -1, 0)` | Down direction (-Y) |
| `Left()` | `Vec3(-1, 0, 0)` | Left direction (-X) |
| `Right()` | `Vec3(1, 0, 0)` | Right direction (+X) |
| `Forward()` | `Vec3(0, 0, 1)` | Forward direction (+Z) |
| `Back()` | `Vec3(0, 0, -1)` | Back direction (-Z) |

#### Example Usage

```
Vec3 a = new Vec3(1, 2, 3)
Vec3 b = new Vec3(4, 5, 6)

// Operator overloading
Vec3 sum = a + b                    // (5, 7, 9)
Vec3 scaled = a * 2.0               // (2, 4, 6)
Vec3 diff = b - a                   // (3, 3, 3)

// Utility methods
float32 dot = a.Dot(b)              // 32
Vec3 cross = a.Cross(b)             // (-3, 6, -3)
Vec3 lerped = a.Lerp(b, 0.5)        // (2.5, 3.5, 4.5)

// Factory methods
Vec3 origin = a.Zero()              // (0, 0, 0)
Vec3 up = a.Up()                    // (0, 1, 0)
```

---

### GameNode

Base class for all engine-integrated game objects. Extend this class to create custom game behavior.

**Location:** `engine/qlang/classes/GameNode.q`

#### Members

| Member | Type | Description |
|--------|------|-------------|
| `NodePtr` | `cptr` | Pointer to the C++ GraphNode (set by engine) |

#### Lifecycle Methods

Override these methods to implement custom behavior:

| Method | Parameters | Description |
|--------|------------|-------------|
| `OnPlay()` | — | Called when play mode starts |
| `OnUpdate(float32 dt)` | `dt`: Delta time in seconds | Called every frame during play |
| `OnRender()` | — | Called during rendering phase |
| `OnStop()` | — | Called when play mode stops |

#### Transform Methods

| Method | Parameters | Description |
|--------|------------|-------------|
| `Turn(Vec3 rotation)` | Rotation in degrees (Euler angles) | Rotates the node by the specified amount |
| `SetPosition(Vec3 position)` | World position | Sets the node's position |
| `GetPosition()` | — | Returns the node's current position |

#### Example Usage

```
class Player(GameNode)
    float32 speed = 5.0
    float32 rotationSpeed = 90.0

    method void OnUpdate(float32 dt)
        // Move forward
        Vec3 move = new Vec3(speed * dt, 0, 0)
        Vec3 pos = GetPosition()
        SetPosition(pos + move)

        // Rotate
        Vec3 rot = new Vec3(0, rotationSpeed * dt, 0)
        Turn(rot)
    end
end
```

---


### GameNode Base Class

*Updated in v0.2*

The `GameNode` class provides lifecycle hooks for engine integration:

```
class GameNode
    cptr NodePtr

    method void OnPlay()
        // Called when play starts
    end

    method void OnUpdate(float32 dt)
        // Called every frame with delta time
    end

    method void OnRender()
        // Called during rendering
    end

    method void OnStop()
        // Called when play stops
    end

    method void Turn(Vec3 rotation)
        NodeTurn(NodePtr, rotation)
    end

    method void SetPosition(Vec3 position)
        NodeSetPosition(NodePtr, position)
    end
end
```

### Creating Custom Game Nodes

Extend `GameNode` to create custom behavior:

```
class MyPlayer(GameNode)
    float32 speed = 5.0

    method void OnUpdate(float32 dt)
        Vec3 move = new Vec3(speed * dt, 0, 0)
        Vec3 pos = GetPosition()
        SetPosition(pos + move)
    end
end
```

### Vec3 Class

*Updated in v0.2*

The built-in `Vec3` class supports operator overloading:

```
class Vec3
    float32 X
    float32 Y
    float32 Z

    method void Vec3(float32 x, float32 y, float32 z)
        X = x
        Y = y
        Z = z
    end

    method Vec3 Plus(Vec3 right)
        return new Vec3(X + right.X, Y + right.Y, Z + right.Z)
    end
end
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

---

## Appendix: Reserved Keywords

```
bool    class   cptr    else    elseif  end     false
float32 float64 for     if      int32   int64   method
module  new     next    null    return  short   string
this    to      true    void    wend    while
```

---

## Changelog

### v0.2 (19.12.2025)

**New Features:**
- **Operator Overloading**: Classes can now define `Plus`, `Minus`, `Multiply`, and `Divide` methods to overload `+`, `-`, `*`, `/` operators.
- **Function Overloading for Operators**: Multiple `Plus` (or other) methods with different parameter types are supported.
- **Delta Time**: `OnUpdate` now receives a `float32 dt` parameter for frame-independent updates.
- **New Native Functions**: Added `NodeSetPosition(cptr, Vec3)` for setting node positions.
- **Built-In Classes Documentation**: Added comprehensive API reference for `Vec3` and `GameNode`.

**Updated Classes:**
- **Vec3**: Full operator overloading (+, -, *, /), dot/cross product, lerp, negate, factory methods (Zero, One, Up, Down, Left, Right, Forward, Back).
- **GameNode**: `OnUpdate` signature changed to `OnUpdate(float32 dt)`. Added `SetPosition(Vec3)` and `Turn(Vec3)` methods.

**Bug Fixes:**
- Fixed false positive "Expected operator between values" error when using closing parentheses in constructor calls.
- Fixed literal handling in `TokenToValue` to correctly parse integers, floats, and booleans.
- Improved constructor error reporting when no matching constructor is found.

### v0.1 (Initial Release)

- Core language features: classes, methods, inheritance, generics
- Primitive types: int32, int64, float32, float64, string, bool, cptr
- Control flow: if/elseif/else, while/wend, for/next
- Native function registration
- C++ QRunner API for engine integration

---

*QLang v0.2 - Quantum Engine Scripting Language*
