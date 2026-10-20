// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");
const { chromium } = require("playwright-core");
const root = path.resolve(process.argv[2]);
const server = http.createServer((request, response) => {
  const file = path.resolve(root, "." + new URL(request.url, "http://localhost").pathname);
  if (!file.startsWith(root + path.sep)) { response.writeHead(403).end(); return; }
  fs.readFile(file, (error, bytes) => {
    if (error) { response.writeHead(404).end(); return; }
    response.setHeader("Content-Type", file.endsWith(".wasm") ? "application/wasm" : file.endsWith(".js") ? "text/javascript" : "text/html");
    response.end(bytes);
  });
});
(async () => {
  let browser;
  try {
    await new Promise(resolve => server.listen(0, "127.0.0.1", resolve));
    browser = await chromium.launch({executablePath: process.env.CHROME_PATH || "/usr/bin/google-chrome", headless: true, args: ["--no-sandbox"]});
    console.log("Browser: " + await browser.version());
    const page = await browser.newPage();
    const errors = [];
    page.on("pageerror", error => errors.push(String(error)));
    await page.goto(`http://127.0.0.1:${server.address().port}/browser.html`);
    await page.waitForFunction(() => document.body.dataset.caorsStatus !== "pending", null, {timeout: 30000});
    if (await page.getAttribute("body", "data-caors-status") !== "passed" || errors.length) throw new Error(errors.join("\n") || "protocol failed");
    console.log("Browser delayed completion, cancellation, queue discard and handle lifetime passed");
  } finally {
    if (browser) await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
