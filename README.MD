# Basalto

A native, systems-level compiler for a modernized VisualG-like language, written in C.

## Requirements

* GCC or Clang (must support `-ldl` for dynamic linking)
* Flex
* Bison

## Build

Basalto uses a minimal C build system (`nob.c`) to avoid complex dependencies.

1. **Bootstrap the build system:**
```bash
cc -o nob nob.c

```


2. **Build the compiler:**
```bash
./nob

```



This generates the `basalto` binary in the `./build/` directory.

## Usage

Compile a `.bso` file:

```bash
./build/basalto input.bso

```

This produces a native executable (default name: `program`). Run it:

```bash
./program

```

## Motivation

I started programming in Java at 14. When I was 15, I entered the IT technical course at [FAETEC](https://www.faetec.rj.gov.br/). While the curriculum included Java, it began with [VisualG](https://sourceforge.net/projects/visualg30/) to teach algorithms.

I remember being completely frustrated with the tool. At the time, I didn't understand its limitations. I constantly asked: *Why is the I/O so basic? Why can't I write a UI program? Why can't I write a web server?*

Now, having finished high school and completed a Computer Science degree at [CEFET/RJ](https://www.cefet-rj.br/), I understand exactly why those limitations existed. **Basalto** is my attempt to fix them.

### The Philosophy

Basalto is a hybrid language. It reimagines the educational accessibility of **VisualG** by fusing it with the syntactic precision and structure of modern systems languages.

* **Syntax:** A unique blend. It keeps the friendly Portuguese keywords of **VisualG/Pascal** but adopts the concise block structure (`{}`) of **C** and the explicit type declarations (`var x: type`) found in **Go** and **Zig**.
* **Performance:** The raw speed, manual memory control, and zero-cost abstractions of **C**.
* **Architecture:** "Low Floor, High Ceiling" — approachable enough for a first-time learner, yet structurally robust enough for professional systems engineering.

### The Goal: FFI (Foreign Function Interface)

The defining feature of Basalto is robust support for **FFI**. By allowing the language to interact seamlessly with system functions and C libraries (`.so`/`.dll`), we remove the "toy language" ceiling. This finally enables a VisualG-like syntax to power:

* Graphical User Interfaces (UI) via libraries like Raylib or GTK.
* Web Servers and Networking.
* Complex I/O and System-level programming.

## Language Features (v1.0)

### 1. Variables & Types

Strongly typed with inference support.

```go
var x: inteiro = 42;
var pi: real = 3.1415;
var nome: texto = "Mundo";

```

### 2. Structs & Methods

Object-oriented capabilities using C-like structs and Uniform Call Syntax.

```go
estrutura Player {
    hp: inteiro
    nome: texto
}

// 'self' automatically acts as a pointer to the struct
funcao curar(self: Player, qtd: inteiro): vazio {
    self.hp = self.hp + qtd;
}

```

### 3. Native FFI

Load C libraries dynamically and map symbols to Basalto functions.

```go
externo math "libm.so.6" {
    // Maps the C symbol "cos" to the Basalto name "cosseno"
    funcao cosseno(x: real): real = "cos"
}

```

### 4. Dynamic Arrays

Arrays act as vectors with automatic resizing.

```go
var lista: [inteiro] = [1, 2];
lista.push(3);
escreval("Tamanho: ${lista.len()}");

```

### 5. File Embedding

Bake assets (text or binary) directly into the executable.

```go
var shader: texto = incorporar("shaders/vertex.glsl");

```

## Methodological Basis

Basalto is architected as a **Source-to-Source Compiler** (Transpiler).

Instead of targeting machine code directly or complex IRs like LLVM, Basalto targets **C99**. This approach treats C as a "portable assembly," a technique pioneered by the first C++ compiler (*Cfront*) and validated by modern research.

* **Portability:** By emitting standard C, Basalto programs can run on any architecture (x86, ARM, RISC-V) that supports a C compiler.
* **Optimization:** We leverage the mature optimization pipelines of existing compilers (GCC/Clang), which perform register allocation and instruction scheduling far better than a custom backend could.

## Compilation Pipeline

The compiler follows a standard 4-stage architecture:

1. **Lexical Analysis (Flex):** Tokenization of the source stream.
2. **Syntactic Analysis (Bison):** Construction of the Abstract Syntax Tree (AST).
3. **Semantic Analysis & Hoisting:** A multi-pass traversal that separates definitions (structs, functions) from execution logic, allowing functions to be defined anywhere in the file.
4. **Codegen & Build:** Emits C99 code and invokes the host compiler via `nob.c` to produce the final binary.

## References

* **Stroustrup, B. (1986).** *The C++ Programming Language*. Addison-Wesley.
* **Papert, S. (1980).** *Mindstorms: Children, Computers, and Powerful Ideas*. Basic Books.
* **Resnick, M., et al. (2009).** "Scratch: Programming for All." *Communications of the ACM*.
