# Security Policy

ardirec parses files that may come from external parties. Treat COMTRADE and companion files as untrusted input.

Please report security vulnerabilities privately to the project maintainer rather than publishing an exploit in an issue. Until a dedicated security contact is configured, use GitHub's private vulnerability reporting feature when enabled for the repository.

Security-sensitive parser changes should include malformed-input tests. The project aims to use bounded reads, checked conversions and sanitizers in development/CI where practical.
