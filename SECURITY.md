# Security policy

ArDiRec opens measurement files that may originate outside your organization. Treat COMTRADE records and companion metadata as untrusted inputs, even when they come from familiar equipment.

## Reporting a vulnerability

Please use [GitHub private vulnerability reporting](https://github.com/masarray/ardirec/security/advisories) if it is enabled for this repository. Do not publish exploit details, proof-of-concept files, security credentials or confidential substation records in a public issue.

If private reporting is unavailable, contact the repository maintainer privately through an existing trusted channel rather than posting sensitive details publicly. No response-time or remediation SLA is promised.

## In scope

- Unsafe parsing of CFG, DAT and optional companion files.
- File handling, path traversal, memory safety and resource-exhaustion problems.
- Unsafe behavior when opening malformed or adversarial records.

Please include the affected ArDiRec version, operating system, reproduction steps and a minimal non-sensitive test case where possible.

Security fixes should add regression tests and preserve bounded-input handling. See [Contributing](CONTRIBUTING.md).
