# ArdIREC branding assets

The canonical visual identity lives in `/icon`. Do not create unrelated app-logo variants elsewhere in the repository.

| Surface | Canonical asset |
| --- | --- |
| Windows executable icon | `icon/favicon.ico` via `apps/desktop/ardirec.rc.in` |
| Qt application/window icon | `icon/android-chrome-512x512.png` embedded under `qrc:/branding/` |
| Workstation toolbar / About logo | `icon/android-chrome-192x192.png` |
| Landing-page favicon | `favicon.ico`, 16×16, 32×32 |
| Apple touch icon | `apple-touch-icon.png` |
| Web app / social brand image | `android-chrome-512x512.png` |
| Web manifest | `icon/site.webmanifest` |
| Inno Setup icon | `icon/favicon.ico` through `installer/ardirec.iss` |
| Repository README brand image | `icon/android-chrome-192x192.png` |

The desktop build embeds the canonical PNG/ICO files directly from `/icon`. Windows executable metadata therefore stays visually aligned with the QML logo and installer without maintaining a second copy of the artwork.
