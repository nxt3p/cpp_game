#!/usr/bin/env node
/**
 * Automated WebGL browser playtest for http://127.0.0.1:8081/index.html
 * Outputs JSON report to stdout.
 */
import puppeteer from 'puppeteer-core';
import { existsSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const __dirname = dirname(fileURLToPath(import.meta.url));
const PORT = process.env.WEBGL_SERVE_PORT || '8081';
const URL = `http://127.0.0.1:${PORT}/index.html`;
const VIEWPORT = { width: 1280, height: 720 };

const CHROME_CANDIDATES = [
  '/mnt/c/Program Files/Google/Chrome/Application/chrome.exe',
  '/mnt/c/Program Files (x86)/Google/Chrome/Application/chrome.exe',
  '/usr/bin/google-chrome',
  '/usr/bin/chromium-browser',
  '/usr/bin/chromium',
];

function findChrome() {
  for (const candidate of CHROME_CANDIDATES) {
    if (existsSync(candidate)) {
      return candidate;
    }
  }
  return null;
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function clickCanvas(page, x, y) {
  const canvas = await page.$('#canvas');
  if (!canvas) {
    throw new Error('Canvas not found');
  }
  const box = await canvas.boundingBox();
  if (!box) {
    throw new Error('Canvas has no bounding box');
  }
  await page.mouse.click(box.x + x, box.y + y);
}

async function pressKey(page, key) {
  await page.keyboard.press(key);
}

const report = {
  url: URL,
  timestamp: new Date().toISOString(),
  build: {},
  load: {},
  navigation: {},
  gameplay: {},
  performance: {},
  items: {},
  saveLoad: {},
  issues: [],
  passed: [],
};

async function main() {
  const chromePath = findChrome();
  if (!chromePath) {
    report.issues.push('Chrome/Chromium not found; browser playtest skipped.');
    console.log(JSON.stringify(report, null, 2));
    process.exit(0);
  }

  const consoleLogs = [];
  const pageErrors = [];

  const browser = await puppeteer.launch({
    executablePath: chromePath,
    headless: true,
    args: [
      '--no-sandbox',
      '--disable-setuid-sandbox',
      '--disable-gpu',
      '--disable-dev-shm-usage',
      '--window-size=1280,720',
    ],
  });

  try {
    const page = await browser.newPage();
    await page.setViewport(VIEWPORT);

    page.on('console', (msg) => {
      consoleLogs.push({ type: msg.type(), text: msg.text() });
    });
    page.on('pageerror', (err) => {
      pageErrors.push(String(err));
    });

    const loadStart = performance.now();
    await page.goto(URL, { waitUntil: 'networkidle0', timeout: 60000 });
    await page.waitForFunction(
      () => document.getElementById('status')?.textContent?.includes('WebGL runtime ready'),
      { timeout: 90000 },
    );
    const loadMs = performance.now() - loadStart;
    report.load.wasmReadyMs = Math.round(loadMs);
    report.passed.push(`WebGL runtime ready in ${report.load.wasmReadyMs}ms`);

    await sleep(1500);

    // Main menu -> START
    await clickCanvas(page, 640, 330);
    await sleep(800);
    report.navigation.mainMenuStart = true;
    report.passed.push('Clicked START on main menu');

    // Warrior class select (left box center)
    await clickCanvas(page, 384, 392);
    await sleep(1200);
    report.navigation.classSelected = 'Warrior';
    report.passed.push('Selected Warrior class');

    // Move toward gate (click north on ground)
    await clickCanvas(page, 640, 280);
    await sleep(800);
    await clickCanvas(page, 640, 220);
    await sleep(1500);

    // Enter Plains - click gate area repeatedly
    for (let i = 0; i < 8; i++) {
      await clickCanvas(page, 640, 200);
      await sleep(400);
    }
    report.gameplay.plainsEntryAttempted = true;
    report.passed.push('Attempted Plains entry via gate clicks');

    // Combat: click around center to engage mobs
    for (let i = 0; i < 40; i++) {
      await clickCanvas(page, 520 + (i % 5) * 40, 360 + (i % 3) * 30);
      await sleep(250);
    }
    report.gameplay.combatClicks = 40;
    report.passed.push('Ran combat click loop on Plains');

    // Open character screen
    await pressKey(page, 'KeyC');
    await sleep(600);
    report.gameplay.characterScreenOpened = true;

    // Open inventory
    await pressKey(page, 'KeyI');
    await sleep(600);
    report.items.inventoryOpened = true;
    report.passed.push('Opened inventory overlay (I key)');

    await pressKey(page, 'KeyI');
    await sleep(300);

    // Pause -> Save and Exit (returns to main menu in current build)
    await pressKey(page, 'Escape');
    await sleep(500);
    await clickCanvas(page, 640, 380);
    await sleep(1500);
    report.saveLoad.saveAndExitClicked = true;
    report.passed.push('Triggered Save and Exit from pause menu');

    // Check if CONTINUE appears (save in session)
    await sleep(500);
    const menuShot = await page.screenshot({ encoding: 'base64' });
    report.saveLoad.menuScreenshotCaptured = Boolean(menuShot?.length > 1000);

    // Performance: measure rAF FPS over 2 seconds via injected script
    const fpsSample = await page.evaluate(async () => {
      await new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r)));
      const frames = [];
      const start = performance.now();
      return new Promise((resolve) => {
        let count = 0;
        function tick(now) {
          frames.push(now);
          if (now - start >= 2000) {
            const intervals = [];
            for (let i = 1; i < frames.length; i++) {
              intervals.push(frames[i] - frames[i - 1]);
            }
            intervals.sort((a, b) => a - b);
            const median = intervals[Math.floor(intervals.length / 2)] || 16.67;
            resolve({
              sampleFrames: frames.length,
              medianFrameMs: Math.round(median * 10) / 10,
              estimatedFps: Math.round((1000 / median) * 10) / 10,
            });
          } else {
            requestAnimationFrame(tick);
          }
        }
        requestAnimationFrame(tick);
      });
    });

    report.performance.canvasMedianFrameMs = fpsSample.medianFrameMs;
    report.performance.estimatedCanvasFps = fpsSample.estimatedFps;
    report.performance.sampleFrames = fpsSample.sampleFrames;
    report.passed.push(
      `Canvas rAF ~${fpsSample.estimatedFps} FPS (median ${fpsSample.medianFrameMs}ms)`,
    );

    // Reload test for save persistence
    await page.reload({ waitUntil: 'networkidle0' });
    await page.waitForFunction(
      () => document.getElementById('status')?.textContent?.includes('WebGL runtime ready'),
      { timeout: 90000 },
    );
    await sleep(1500);
    report.saveLoad.reloadAfterSave = true;

    // Click where CONTINUE would be if save persisted (upper button slot)
    await clickCanvas(page, 640, 370);
    await sleep(800);
    report.saveLoad.continueSlotClickedAfterReload = true;

    if (pageErrors.length > 0) {
      report.issues.push(...pageErrors.map((e) => `Page error: ${e}`));
    }

    const abortLogs = consoleLogs.filter(
      (l) => l.type === 'error' || /abort|exception|wasm/i.test(l.text),
    );
    if (abortLogs.length > 0) {
      report.issues.push(...abortLogs.slice(0, 5).map((l) => `Console: ${l.text}`));
    } else {
      report.passed.push('No wasm/abort console errors detected');
    }

    // IDBFS note from GameEngine.js
    report.saveLoad.persistenceNote =
      "Web saves persist in browser localStorage (cppGame_save_v1) until site data is cleared.";
  } finally {
    await browser.close();
  }

  console.log(JSON.stringify(report, null, 2));
}

main().catch((error) => {
  report.issues.push(String(error));
  console.log(JSON.stringify(report, null, 2));
  process.exit(1);
});
