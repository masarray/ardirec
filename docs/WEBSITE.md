# ArDiRec website — release and deployment contract

Official user-facing website: **https://masarray.github.io/ardirec/**.

## Source ownership

The static website is intentionally small and dependency-free:

- `index.html` — content, accessible structure, SEO and an explicitly illustrative waveform graphic
- `assets/site.css` — responsive visual system
- `assets/site.js` — optional latest-Stable release discovery with a verified pinned fallback
- `icon/` — existing canonical application icons and browser manifest
- `robots.txt` / `sitemap.xml` — crawl discovery
- `scripts/check_pages_site.py` — static release, accessibility, link and metadata guard
- `.github/workflows/pages.yml` — public-files-only Pages deployment

The workflow **does not publish** source code, tests, raw COMTRADE fixtures, repository workflows, or private engineering notes as website files. It stages only the explicit site allowlist.

## Deployment

Repository **Settings → Pages → Build and deployment → Source: GitHub Actions**.

Push site changes to `main` (or select the workflow's `Run workflow` option) to deploy. The workflow validates static links, expected icons, release fallback URLs and JavaScript syntax, uploads a static artifact, then deploys it to the `github-pages` environment.

Run locally:

```bash
python3 scripts/check_pages_site.py
node --check assets/site.js
```

## Stable-download policy

Direct Windows/Linux download links in HTML are pinned to the verified v0.2.0 public assets. Client-side progressive enhancement asks GitHub's public `releases/latest` API for a later **non-draft, non-prerelease** release with both matching installer ZIPs and SHA-256 checksums.

If the API is unavailable, rate-limited or incomplete, the safe static links remain functional. Never generate download URLs by guessing a future filename. After publishing a newer Stable, update the HTML fallback links, label, file sizes and structured software version as part of the release milestone.

The website is a promotional/documentation site, not a browser-based disturbance analysis tool. Its signal image is labeled as an illustration; replace it with a real, permission-cleared application screenshot only when one is deliberately supplied.

## Verification before publication

Check the canonical site in a desktop and narrow-screen browser. Confirm header navigation, workflow and download anchors, Windows/Linux ZIP links, checksum link, metadata preview, favicon, readable contrast, keyboard focus and reduced-motion behavior. For downloads, inspect the official GitHub asset identity rather than assuming a click alone proves the package is safe.

Published binaries and release checksums are not changed by website deployments.
