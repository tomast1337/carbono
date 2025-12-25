# Carbono

A native compiler for Modern VisualG written in C99.

## Requirements

* GCC or Clang
* Flex
* Bison

## Build

1. Compile the build system:
```bash
cc -o nob nob.c

```


2. Build the project:
```bash
./nob

```



## Usage

Compile a `.alg` file:

```bash
./build/carbono input.alg

```

This produces a native executable (default name: `output`). Run it:

```bash
./output

```

## Motivation

I started programming in Java at 14. When I was 15, I entered the IT technical course at [FAETEC](https://www.faetec.rj.gov.br/). While the curriculum included Java, it began with [VisualG](https://sourceforge.net/projects/visualg30/) to teach algorithms.

I remember being completely frustrated with the tool. At the time, I didn't understand its limitations. I constantly asked: *Why is the I/O so basic? Why can't I write a UI program? Why can't I write a web server?*

Now, having finished high school and completed a Computer Science degree at [CEFET/RJ](https://www.cefet-rj.br/), I understand exactly why those limitations existed. **Carbono** is my attempt to fix them.

### The Philosophy

This project is a variation of the VisualG language (rooted in Pascal) but aligned with modern standards.

* **Syntax:** Familiarity of Pascal.
* **Modernity and Simplicity:** The minimal overhead of **C** and **Zig**.

### The Goal: FFI (Foreign Function Interface)

The ultimate goal of Carbono is robust support for **FFI**. By allowing the language to interact seamlessly with system functions and C libraries, we remove the "toy language" ceiling. This will finally enable a VisualG-like syntax to power:

* Graphical User Interfaces (UI)
* Web Servers
* Complex I/O and Graphics
* System-level programming

## Methodological Basis

Carbono is architected as a **Source-to-Source Compiler** (Transpiler), a design choice supported by decades of compiler literature to ensure portability, performance, and educational efficacy.

### 1. C as a Portable Intermediate Representation

Instead of targeting machine code directly or complex IRs like LLVM, Carbono targets **C99**. This approach treats C as a "portable assembly," a technique pioneered by the first C++ compiler (*Cfront*) and validated by modern research [1].

* **Portability:** By emitting standard C, Carbono programs can run on any architecture (x86, ARM, RISC-V) that supports a C compiler, without requiring backend-specific maintenance.
* **Optimization:** We leverage the mature optimization pipelines of existing compilers (GCC/Clang), which perform register allocation and instruction scheduling far better than a custom backend could [2].

### 2. Pedagogical Design: "Low Floor, High Ceiling"

The primary architectural goal is to solve the "toy language" problem common in CS education. Seymour Papert’s constructionist theory emphasizes that effective learning tools must have a **"Low Floor"** (easy entry for novices) and a **"High Ceiling"** (capable of complex professional work) [3].

* **The Floor (VisualG Compatibility):** We maintain the accessible Portuguese syntax of VisualG to ensure zero friction for students entering from high school technical courses.
* **The Ceiling (FFI & Native compilation):** Existing tools like VisualG impose an artificial "ceiling" by preventing system-level interaction [4]. Carbono removes this by allowing **Foreign Function Interface (FFI)** calls to C libraries. This enables students to transition from simple algorithms to building real-world software (Web Servers, GUIs) without switching languages.

### 3. Compilation Pipeline

The compiler follows a standard 4-stage architecture [5]:

1. **Lexical Analysis (Flex):** Tokenization of the source stream.
2. **Syntactic Analysis (Bison):** Construction of the Abstract Syntax Tree (AST).
3. **Semantic Analysis & Codegen:** recursive descent of the AST to emit C99 code.
4. **Native Build:** Invocation of the host compiler via `nob.c` to produce the final binary.

### References

* **[1] Stroustrup, B. (1986).** *The C++ Programming Language*. Addison-Wesley. (See discussion on *Cfront* implementation).
* **[2] Necula, G. C., et al. (2002).** "CIL: Intermediate Language and Tools for Analysis and Transformation of C Programs." *Proceedings of CC 2002*.
* **[3] Papert, S. (1980).** *Mindstorms: Children, Computers, and Powerful Ideas*. Basic Books.
* **[4] Resnick, M., et al. (2009).** "Scratch: Programming for All." *Communications of the ACM*, 52(11). (Discusses the importance of "Wide Walls" and extending ceilings).
* **[5] Aho, A. V., Lam, M. S., Sethi, R., & Ullman, J. D. (2006).** *Compilers: Principles, Techniques, and Tools (2nd Edition)*. Pearson.