# Recommended GitHub Repository Setup

## Repository

- Name: `ardirec`
- Visibility: Public
- Default branch: `main`
- License: GPL-3.0-or-later
- Description: `Modern open-source COMTRADE workstation for protection and disturbance analysis.`

## Suggested topics

`comtrade`, `protection-relay`, `disturbance-analysis`, `fault-recorder`, `power-systems`, `qt`, `qml`, `cpp`, `oscillography`, `substation`, `electrical-engineering`

## Repository features

Enable:

- Issues
- Discussions when user/community traffic begins
- Private vulnerability reporting
- Dependabot alerts
- Code scanning / CodeQL

## Branch protection recommendation

For `main` after the first successful CI run:

- require pull request before merge;
- require `core` checks;
- require `desktop-linux` once stable;
- require branch to be up to date for high-risk parser/calculation changes;
- block force pushes and branch deletion.

## About proprietary references

Do not upload vendor binaries, manuals, screenshots or relay records unless redistribution is permitted. Product names may be mentioned for interoperability/benchmark context, with trademarks belonging to their owners.
