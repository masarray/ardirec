/* Progressive enhancement: verified stable assets only. Static v0.2.0 links work offline or without JS. */
(() => {
  "use strict";
  const api = "https://api.github.com/repos/masarray/ardirec/releases/latest";
  const releaseBase = "https://github.com/masarray/ardirec/releases/download/";
  const fileRules = {
    windows: /^ArDiRec-v\d+\.\d+\.\d+-Windows-x64-Setup\.zip$/,
    linux: /^ArDiRec-v\d+\.\d+\.\d+-Linux-x86_64-Setup\.zip$/,
    checksums: /^SHA256SUMS\.txt$/
  };
  const safeAsset = asset => {
    if (!asset || typeof asset.name !== "string" || typeof asset.browser_download_url !== "string") return false;
    try {
      const url = new URL(asset.browser_download_url);
      return url.protocol === "https:" &&
             url.origin === "https://github.com" &&
             url.href.startsWith(releaseBase) &&
             url.pathname.endsWith("/" + encodeURIComponent(asset.name));
    } catch {
      return false;
    }
  };
  const getAsset = (assets, pattern) => assets.find(asset => pattern.test(asset.name) && safeAsset(asset));
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 4500);
  fetch(api, { signal: controller.signal, headers: { Accept: "application/vnd.github+json" } })
    .then(response => {
      if (!response.ok) throw new Error("release lookup unavailable");
      return response.json();
    })
    .then(data => {
      if (!data || data.draft || data.prerelease ||
          typeof data.tag_name !== "string" || !/^v\d+\.\d+\.\d+$/.test(data.tag_name) ||
          !Array.isArray(data.assets)) return;
      const assets = {
        windows: getAsset(data.assets, fileRules.windows),
        linux: getAsset(data.assets, fileRules.linux),
        checksums: getAsset(data.assets, fileRules.checksums)
      };
      if (!assets.windows || !assets.linux || !assets.checksums ||
          !assets.windows.name.startsWith("ArDiRec-" + data.tag_name + "-") ||
          !assets.linux.name.startsWith("ArDiRec-" + data.tag_name + "-")) return;
      const links = [
        ["download-windows", assets.windows.browser_download_url],
        ["download-linux", assets.linux.browser_download_url],
        ["download-checksums", assets.checksums.browser_download_url]
      ];
      for (const [id, url] of links) {
        const anchor = document.getElementById(id);
        if (anchor) anchor.href = url;
      }
      document.querySelectorAll("[data-release-version]").forEach(element => { element.textContent = data.tag_name; });
      const windowsMeta = document.querySelector("#download-windows + .package-meta");
      const linuxMeta = document.querySelector("#download-linux + .package-meta");
      const mb = value => (value / 1000000).toFixed(1) + " MB";
      if (windowsMeta && Number.isFinite(assets.windows.size)) windowsMeta.textContent = "Installer ZIP · " + mb(assets.windows.size) + " · Signed: no";
      if (linuxMeta && Number.isFinite(assets.linux.size)) linuxMeta.textContent = "Debian / AppImage ZIP · " + mb(assets.linux.size);
    })
    .catch(() => { /* Keep the verified version-pinned static fallback. */ })
    .finally(() => clearTimeout(timeout));
})();
