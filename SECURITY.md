# Security Policy

## Supported versions

Only the latest development version on `main` is currently supported.

## Reporting a vulnerability

If you discover a security vulnerability in Arcana, please report it
responsibly by emailing the project maintainer directly rather than filing
a public issue.

Include:

- description of the vulnerability,
- steps to reproduce,
- affected components (bytecode loader, verifier, VM, etc.),
- potential impact.

You should receive an acknowledgment within 72 hours.

## Scope

The bytecode loader and verifier treat all input as untrusted. Malformed
bytecode must never cause out-of-bounds memory access, arbitrary code
execution, or process compromise. A crash caused by malformed input is
considered a security bug.

The compiler treats semantic graph input as semi-trusted (it may be
malformed but not adversarial). Compiler crashes on malformed graphs are
bugs but not security vulnerabilities.
