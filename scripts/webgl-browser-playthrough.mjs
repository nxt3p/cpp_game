#!/usr/bin/env node
import { createServer } from 'node:http';
import { readFileSync, existsSync, writeFileSync } from 'node:fs';
import { join, extname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawn } from 'node:child_process';

const __dirname = fileURLToPath(new URL('.', import.meta.url));
const buildDir = join(__dirname, '..', 'build-webgl');
const port = Number(process.env.WEBGL_SMOKE_PORT ?? 8782);
const debugPort = Number(process.env.WEBGL_CDP_PORT ?? 9231);
const url = `http://127.0.0.1:${port}/index.html`;
const chromeCandidates = [
    process.env.CHROME_PATH,
    '/mnt/c/Program Files/Google/Chrome/Application/chrome.exe',
    '/mnt/c/Program Files (x86)/Google/Chrome/Application/chrome.exe',
    '/usr/bin/google-chrome',
    '/usr/bin/chromium-browser',
    '/usr/bin/chromium',
].filter(Boolean);
const chromePath = chromeCandidates.find((candidate) => existsSync(candidate));

const mime = {
    '.html': 'text/html',
    '.js': 'application/javascript',
    '.wasm': 'application/wasm',
    '.data': 'application/octet-stream',
    '.png': 'image/png',
};

function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

function startServer() {
    return new Promise((resolve) => {
        const server = createServer((req, res) => {
            const path = req.url === '/' ? '/index.html' : req.url.split('?')[0];
            const filePath = join(buildDir, path);
            if (!filePath.startsWith(buildDir) || !existsSync(filePath)) {
                res.writeHead(404);
                res.end('not found');
                return;
            }
            const body = readFileSync(filePath);
            res.writeHead(200, { 'Content-Type': mime[extname(filePath)] ?? 'application/octet-stream' });
            res.end(body);
        });
        server.listen(port, '0.0.0.0', () => resolve(server));
    });
}

async function waitForCdp() {
    for (let attempt = 0; attempt < 40; ++attempt) {
        try {
            const response = await fetch(`http://127.0.0.1:${debugPort}/json/list`);
            const targets = await response.json();
            const page = targets.find((target) => target.type === 'page');
            if (page?.webSocketDebuggerUrl) {
                return page.webSocketDebuggerUrl;
            }
        } catch {
            // retry
        }
        await sleep(250);
    }
    throw new Error(`Timed out waiting for CDP on port ${debugPort}`);
}

class CdpSession {
    constructor(webSocketUrl) {
        this.webSocketUrl = webSocketUrl;
        this.nextId = 1;
        this.pending = new Map();
        this.events = [];
        this.logs = [];
        this.ws = null;
    }

    async connect() {
        this.ws = new WebSocket(this.webSocketUrl);
        await new Promise((resolve, reject) => {
            this.ws.addEventListener('open', resolve, { once: true });
            this.ws.addEventListener('error', reject, { once: true });
        });
        this.ws.addEventListener('message', (event) => {
            const message = JSON.parse(event.data);
            if (message.id && this.pending.has(message.id)) {
                const { resolve, reject } = this.pending.get(message.id);
                this.pending.delete(message.id);
                if (message.error) {
                    reject(new Error(message.error.message ?? 'CDP error'));
                } else {
                    resolve(message.result);
                }
                return;
            }
            if (message.method === 'Runtime.consoleAPICalled') {
                const text = (message.params?.args ?? [])
                    .map((arg) => arg.value ?? arg.description ?? '')
                    .join(' ');
                if (text) {
                    this.logs.push(text);
                }
            } else if (message.method === 'Runtime.exceptionThrown') {
                const details = message.params?.exceptionDetails;
                this.logs.push(`PAGEERROR: ${details?.text ?? 'unknown exception'}`);
            }
            this.events.push(message);
        });
        await this.send('Runtime.enable');
        await this.send('Page.enable');
        await this.send('Log.enable');
    }

    send(method, params = {}) {
        const id = this.nextId++;
        const payload = JSON.stringify({ id, method, params });
        return new Promise((resolve, reject) => {
            this.pending.set(id, { resolve, reject });
            this.ws.send(payload);
        });
    }

    async navigate(targetUrl) {
        await this.send('Page.navigate', { url: targetUrl });
        await sleep(4000);
    }

    async click(x, y) {
        await this.send('Input.dispatchMouseEvent', {
            type: 'mousePressed',
            x,
            y,
            button: 'left',
            clickCount: 1,
        });
        await this.send('Input.dispatchMouseEvent', {
            type: 'mouseReleased',
            x,
            y,
            button: 'left',
            clickCount: 1,
        });
    }

    async screenshot(path) {
        const result = await this.send('Page.captureScreenshot', { format: 'png' });
        writeFileSync(path, Buffer.from(result.data, 'base64'));
    }

    close() {
        if (this.ws) {
            this.ws.close();
        }
    }
}

async function main() {
    if (!existsSync(join(buildDir, 'GameEngine.js'))) {
        throw new Error('Missing build-webgl output. Run ./scripts/build-webgl.sh first.');
    }
    if (!chromePath) {
        throw new Error('Chrome/Chromium executable not found.');
    }

    const server = await startServer();
    const chrome = spawn(
        chromePath,
        [
            '--headless=new',
            '--disable-gpu',
            '--no-sandbox',
            '--window-size=1280,720',
            `--remote-debugging-port=${debugPort}`,
            '--remote-debugging-address=0.0.0.0',
            'about:blank',
        ],
        { stdio: 'ignore' },
    );

    let session;
    try {
        const webSocketUrl = await waitForCdp();
        session = new CdpSession(webSocketUrl);
        await session.connect();
        await session.navigate(url);
        await sleep(3000);

        let blob = session.logs.join('\n');
        for (const token of ['Booting into Main Menu', 'Entering main loop']) {
            if (!blob.includes(token)) {
                throw new Error(`Missing boot log: ${token}`);
            }
        }

        await session.screenshot('/mnt/c/Users/Public/cppgame-menu.png');

        await session.click(640, 330);
        await sleep(1500);
        await session.click(384, 392);
        await sleep(3000);

        blob = session.logs.join('\n');
        for (const token of [
            'Play selected -> Character Select.',
            'Screen -> CharacterSelect',
            'Screen -> InGame',
            'Adventure started as',
        ]) {
            if (!blob.includes(token)) {
                throw new Error(`Missing playthrough log: ${token}`);
            }
        }
        if (blob.includes('Fatal engine error') || blob.includes('shader compilation failed')) {
            throw new Error('Runtime fatal error detected in browser logs');
        }

        await session.screenshot('/mnt/c/Users/Public/cppgame-town.png');
        console.log('WebGL browser playthrough passed.');
        console.log('  main menu booted');
        console.log('  class select opened');
        console.log('  town gameplay entered');
        console.log('  screenshots: /mnt/c/Users/Public/cppgame-menu.png, /mnt/c/Users/Public/cppgame-town.png');
    } finally {
        if (session) {
            session.close();
        }
        if (!chrome.killed) {
            chrome.kill('SIGTERM');
        }
        server.close();
    }
}

main().catch((error) => {
    console.error('WebGL browser playthrough FAILED:', error.message);
    process.exit(1);
});
