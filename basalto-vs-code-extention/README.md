# Basalto Language Support for VS Code

Visual Studio Code extension providing syntax highlighting and language configuration for the Basalto programming language.

## Overview

This extension adds language support for Basalto, a Portuguese-first programming language. It provides syntax highlighting, bracket matching, indentation rules, and other editor features to improve the development experience when working with Basalto source files.

## Features

- **Syntax Highlighting** - Comprehensive syntax highlighting for Basalto source code
- **Language Configuration** - Bracket matching, auto-closing pairs, and indentation rules
- **Comment Support** - Line comments (`//`) and block comments (`/* */`)
- **String Interpolation** - Syntax highlighting for string interpolation expressions (`${variable}`)
- **Type Annotations** - Highlighting for type annotations (`: tipo`)
- **Struct Definitions** - Special highlighting for `estrutura` definitions

## Installation

### From VS Code Marketplace

1. Open Visual Studio Code
2. Navigate to Extensions (Ctrl+Shift+X / Cmd+Shift+X)
3. Search for "Basalto"
4. Click Install

### From VSIX Package

Install from a `.vsix` file using the command line:

```bash
code --install-extension basalto-0.0.1.vsix
```

## File Association

Files with the `.bso` extension are automatically recognized as Basalto source code and will receive syntax highlighting and language features.

## Language Features

### Keyword Highlighting

**Control Flow Keywords:**
- `se`, `senao`, `enquanto`, `cada`, `infinito`
- `parar`, `continuar`, `retorne`, `garantir`

**Declaration Keywords:**
- `programa`, `funcao`, `estrutura`, `externo`, `var`, `alias`

**I/O Keywords:**
- `ler`, `escreval`

### Type Highlighting

**Numeric Types:**
- `inteiro`, `inteiro32`, `inteiro64`

**Floating-Point Types:**
- `real`, `real32`, `real64`

**Other Types:**
- `texto`, `booleano`, `vazio`

**Boolean Literals:**
- `verdadeiro`, `falso`

### Syntax Features

- **Comments**: Line comments (`//`) and block comments (`/* */`)
- **Strings**: Double-quoted strings with escape sequence support and interpolation syntax
- **Numbers**: Integer and floating-point literals with optional type suffixes (`r`, `f`)
- **Functions**: Function identifiers are highlighted
- **Structs**: Structure definitions using the `estrutura` keyword are highlighted

## Example

```basalto
programa "Exemplo" {
    var nome: texto = "Basalto";
    var idade: inteiro32 = 25;
    
    escreval("Olá, ${nome}! Você tem ${idade} anos.");
    
    se (idade > 18) {
        escreval("Você é maior de idade.");
    }
}
```

## Editor Configuration

The extension automatically configures the following editor features:

- Bracket matching for braces (`{}`), brackets (`[]`), and parentheses (`()`)
- Auto-closing pairs for brackets and quotes
- Indentation rules based on brace structure
- Comment toggling support

## Requirements

- Visual Studio Code version 1.107.0 or higher

## Contributing

Contributions are welcome. Please submit issues or pull requests through the project repository.

## License

[Specify license]

## Changelog

See [CHANGELOG.md](./CHANGELOG.md) for version history and change details.
