// ---------------------------------------------------------------------------
// Tauri StreamDeck Config – uses __TAURI_INTERNALS__.invoke() for all IPC
// ---------------------------------------------------------------------------
const ACTION_SIZE = 1 + 258 + 24;

const ACTION_NONE = 0, ACTION_KEY = 1, ACTION_CONSUMER = 2, ACTION_MACRO = 3,
      ACTION_TEXT = 4, ACTION_PASTE = 5, ACTION_LAUNCHER = 6;

const LAUNCHER_MAC = 0, LAUNCHER_WINDOWS = 1, LAUNCHER_LINUX = 2;
const NUM_BUTTONS = 6;
const MAX_MACRO_STEPS = 16;
const TEXT_MAX_LEN = 256;

function invoke(command, args) {
  if (window.__TAURI_INTERNALS__) {
    return window.__TAURI_INTERNALS__.invoke(command, args || {});
  }
  throw new Error('Tauri API not available — is this running inside the desktop app?');
}

// ---------------------------------------------------------------------------
// HID key mapping helpers
// ---------------------------------------------------------------------------
const HID_KEY_NAMES = {
  0x04:'A',0x05:'B',0x06:'C',0x07:'D',0x08:'E',0x09:'F',0x0A:'G',
  0x0B:'H',0x0C:'I',0x0D:'J',0x0E:'K',0x0F:'L',0x10:'M',0x11:'N',
  0x12:'O',0x13:'P',0x14:'Q',0x15:'R',0x16:'S',0x17:'T',0x18:'U',
  0x19:'V',0x1A:'W',0x1B:'X',0x1C:'Y',0x1D:'Z',
  0x1E:'1',0x1F:'2',0x20:'3',0x21:'4',0x22:'5',0x23:'6',0x24:'7',
  0x25:'8',0x26:'9',0x27:'0',
  0x28:'Enter',0x29:'Esc',0x2A:'Backspace',0x2B:'Tab',0x2C:'Space',
  0x2D:'-',0x2E:'=',0x2F:'[',0x30:']',0x31:'Backslash',0x32:'#',0x33:';',
  0x34:'\'',0x36:',',0x37:'.',0x38:'/',
  0x3A:'F1',0x3B:'F2',0x3C:'F3',0x3D:'F4',0x3E:'F5',0x3F:'F6',
  0x40:'F7',0x41:'F8',0x42:'F9',0x43:'F10',0x44:'F11',0x45:'F12',
  0x46:'PrintScreen',0x47:'ScrollLock',0x48:'Pause',
  0x49:'Insert',0x4A:'Home',0x4B:'PageUp',0x4C:'Delete',0x4D:'End',
  0x4E:'PageDown',
  0x4F:'\u2192',0x50:'\u2190',0x51:'\u2193',0x52:'\u2191',
  0x53:'NumLock',0x54:'KP /',0x55:'KP *',0x56:'KP -',0x57:'KP +',
  0x58:'KP Enter',0x59:'KP 1',0x5A:'KP 2',0x5B:'KP 3',0x5C:'KP 4',
  0x5D:'KP 5',0x5E:'KP 6',0x5F:'KP 7',0x60:'KP 8',0x61:'KP 9',
  0x62:'KP 0',0x63:'KP .',
  0x65:'Application',0x66:'Power',0x67:'KP =',
  0x68:'F13',0x69:'F14',0x6A:'F15',0x6B:'F16',0x6C:'F17',0x6D:'F18',
};

const HID_MOD_NAMES = [
  [0x01,'Ctrl'],[0x02,'Shift'],[0x04,'Alt'],[0x08,'Cmd']
];

function hidKeyName(mod, keycode) {
  const parts = [];
  for (const [bit, name] of HID_MOD_NAMES) {
    if (mod & bit) parts.push(name);
  }
  const kn = HID_KEY_NAMES[keycode];
  parts.push(kn ? kn : '0x' + keycode.toString(16).toUpperCase().padStart(2,'0'));
  return parts.join(' + ');
}

function jsKeyToHid(key) {
  if (key.length === 1) {
    const c = key.charCodeAt(0);
    if (c >= 97 && c <= 122) return c - 93;
    if (c >= 65 && c <= 90) return c - 61;
    if (c >= 49 && c <= 57) return c - 19;
    if (c === 48) return 0x27;
    if (c === 33) return 0x1E;
    if (c === 64) return 0x1F;
    if (c === 35) return 0x20;
    if (c === 36) return 0x21;
    if (c === 37) return 0x22;
    if (c === 94) return 0x23;
    if (c === 38) return 0x24;
    if (c === 42) return 0x25;
    if (c === 40) return 0x26;
    if (c === 41) return 0x27;
    if (c === 45) return 0x2D;
    if (c === 61) return 0x2E;
    if (c === 91) return 0x2F;
    if (c === 93) return 0x30;
    if (c === 92) return 0x31;
    if (c === 59) return 0x33;
    if (c === 39) return 0x34;
    if (c === 44) return 0x36;
    if (c === 46) return 0x37;
    if (c === 47) return 0x38;
    if (c === 96) return 0x35;
    if (c === 32) return 0x2C;
    if (c === 126) return 0x35;
  }
  const MAP = {
    'Enter':0x28,'Escape':0x29,'Backspace':0x2A,'Tab':0x2B,'Space':0x2C,
    'ArrowRight':0x4F,'ArrowLeft':0x50,'ArrowDown':0x51,'ArrowUp':0x52,
    'Delete':0x4C,'Home':0x4A,'End':0x4D,'PageUp':0x4B,'PageDown':0x4E,
    'Insert':0x49,'F1':0x3A,'F2':0x3B,'F3':0x3C,'F4':0x3D,'F5':0x3E,
    'F6':0x3F,'F7':0x40,'F8':0x41,'F9':0x42,'F10':0x43,'F11':0x44,
    'F12':0x45,'F13':0x68,'F14':0x69,'F15':0x6A,'F16':0x6B,'F17':0x6C,
    'F18':0x6D,'PrintScreen':0x46,'ScrollLock':0x47,'Pause':0x48,
    'NumLock':0x53,'CapsLock':0x39,
  };
  return MAP[key] || 0;
}

function jsModToHid(e) {
  let mod = 0;
  if (e.ctrlKey)  mod |= 0x01;
  if (e.shiftKey) mod |= 0x02;
  if (e.altKey)   mod |= 0x04;
  if (e.metaKey)  mod |= 0x08;
  return mod;
}

// ---------------------------------------------------------------------------
// Connection state
// ---------------------------------------------------------------------------
let connected = false;
let currentPort = null;

const statusEl = document.getElementById('status');
const statusText = document.getElementById('statusText');
const connectBtn = document.getElementById('connectBtn');
const saveFlashBtn = document.getElementById('saveFlashBtn');
const refreshBtn = document.getElementById('refreshBtn');
const grid = document.getElementById('grid');

function setStatus(text, cls) {
  statusText.textContent = text;
  statusEl.className = 'status' + (cls ? ' ' + cls : '');
}

connectBtn.addEventListener('click', showPortPicker);
saveFlashBtn.addEventListener('click', saveFlash);
refreshBtn.addEventListener('click', loadMapping);

// ---------------------------------------------------------------------------
// Port selection UI
// ---------------------------------------------------------------------------

async function showPortPicker() {
  if (connected) {
    await doDisconnect();
    return;
  }

  setStatus('Scanning ports...', null);
  let ports;
  try {
    ports = await invoke('list_ports');
  } catch (e) {
    setStatus('Failed to list ports: ' + e, 'err');
    return;
  }

  if (!ports || ports.length === 0) {
    setStatus('No serial ports found', 'err');
    return;
  }

  const overlay = document.createElement('div');
  overlay.className = 'recording-overlay';

  const box = document.createElement('div');
  box.className = 'box';
  box.style.minWidth = '320px';

  const h2 = document.createElement('h2');
  h2.textContent = 'Select Serial Port';
  box.appendChild(h2);

  const sel = document.createElement('select');
  sel.style.width = '100%';
  sel.style.marginBottom = '12px';
  ports.forEach(function(p) {
    const opt = document.createElement('option');
    opt.value = p;
    opt.textContent = p;
    sel.appendChild(opt);
  });
  box.appendChild(sel);

  const btnRow = document.createElement('div');
  btnRow.style.cssText = 'display:flex;gap:8px;margin-top:8px;';

  const connectOk = document.createElement('button');
  connectOk.textContent = 'Connect';
  connectOk.addEventListener('click', async function() {
    const portName = sel.value;
    overlay.remove();
    await doConnect(portName);
  });
  btnRow.appendChild(connectOk);

  const cancelBtn = document.createElement('button');
  cancelBtn.textContent = 'Cancel';
  cancelBtn.className = 'secondary';
  cancelBtn.addEventListener('click', function() {
    overlay.remove();
    setStatus('Not connected', null);
  });
  btnRow.appendChild(cancelBtn);

  box.appendChild(btnRow);
  overlay.appendChild(box);
  document.body.appendChild(overlay);
}

async function doConnect(portName) {
  try {
    setStatus('Connecting...', null);
    const result = await invoke('connect_port', { portName });
    connected = true;
    currentPort = portName;
    setStatus('Connected to ' + portName, 'ok');
    connectBtn.textContent = 'Disconnect';
    saveFlashBtn.disabled = false;
    refreshBtn.disabled = false;
    await loadMapping();
  } catch (e) {
    setStatus('Connect failed: ' + e, 'err');
  }
}

async function doDisconnect() {
  try {
    await invoke('disconnect_port');
  } catch (e) {
    // ignore
  }
  connected = false;
  currentPort = null;
  setStatus('Disconnected', null);
  connectBtn.textContent = 'Connect';
  saveFlashBtn.disabled = true;
  refreshBtn.disabled = true;
  grid.innerHTML = '<div class="empty-state"><h3>No device connected</h3><p>Click "Connect" to select your Stream Deck serial port.</p></div>';
}

// ---------------------------------------------------------------------------
// Mapping (via Tauri commands)
// ---------------------------------------------------------------------------

async function loadMapping() {
  try {
    setStatus('Loading mapping...', null);
    const mapping = await invoke('get_mapping');
    currentMapping = mapping || [];
    renderGrid();
    setStatus('Connected', 'ok');
  } catch (e) {
    setStatus('Load failed: ' + e, 'err');
  }
}

// ---------------------------------------------------------------------------
// Paste / clipboard handling
// ---------------------------------------------------------------------------
// The firmware sends a 0xBE notification when a Paste button is pressed.
// In the WebSerial version this came via the serial read loop. In the Tauri
// version the backend should listen for that packet and emit a Tauri event.
// We listen for it here.

let currentMapping = [];

if (window.__TAURI_INTERNALS__ && window.__TAURI_INTERNALS__.event) {
  window.__TAURI_INTERNALS__.event.listen('clipboard-ready', async function(evt) {
    const btnIdx = evt.payload;
    await handleClipboardReady(btnIdx);
  });
}

async function handleClipboardReady(btnIdx) {
  if (btnIdx >= currentMapping.length) return;
  const action = currentMapping[btnIdx];
  if (!action || action.type !== ACTION_PASTE || !action.text) return;
  try {
    await navigator.clipboard.writeText(action.text);
    console.log('[StreamDeck] Clipboard written:', action.text.length, 'chars');
  } catch (e) {
    console.warn('[StreamDeck] Clipboard write failed:', e);
  }
}

// ---------------------------------------------------------------------------
// Macro recording
// ---------------------------------------------------------------------------

let recordingCtx = null;

function startMacroRecording(steps, onDone) {
  const overlay = document.createElement('div');
  overlay.className = 'recording-overlay';
  const box = document.createElement('div');
  box.className = 'box';
  box.innerHTML = '<h2>Recording Macro</h2><p>Press keys to add steps. Press <b>Escape</b> or click <b>Stop</b> to finish.</p>';
  const stopBtn = document.createElement('button');
  stopBtn.textContent = 'Stop Recording';
  stopBtn.addEventListener('click', stopMacroRecording);
  box.appendChild(stopBtn);
  overlay.appendChild(box);
  document.body.appendChild(overlay);

  recordingCtx = { steps: steps, onDone: onDone, overlay: overlay, lastKey: null, lastMod: 0 };

  document.addEventListener('keydown', onRecordKeyDown, true);
  document.addEventListener('keyup', onRecordKeyUp, true);
}

function stopMacroRecording() {
  if (!recordingCtx) return;
  document.removeEventListener('keydown', onRecordKeyDown, true);
  document.removeEventListener('keyup', onRecordKeyUp, true);
  recordingCtx.overlay.remove();
  const cb = recordingCtx.onDone;
  recordingCtx = null;
  cb();
}

function onRecordKeyDown(e) {
  if (!recordingCtx) return;
  if (e.key === 'Escape') { stopMacroRecording(); return; }
  e.preventDefault();
  e.stopPropagation();

  const mod = jsModToHid(e);
  const key = jsKeyToHid(e.key);
  if (key === 0) return;
  if (recordingCtx.steps.length >= MAX_MACRO_STEPS) return;

  recordingCtx.steps.push({ modifier: mod, keycode: key });
  recordingCtx.lastKey = key;
  recordingCtx.lastMod = mod;
}

function onRecordKeyUp(e) {
  if (!recordingCtx) return;
  e.preventDefault();
  e.stopPropagation();

  const mod = jsModToHid(e);
  const key = jsKeyToHid(e.key);
  if (key === recordingCtx.lastKey && key !== 0) {
    recordingCtx.lastKey = null;
  }
  if ((mod & 0x0F) === 0 && recordingCtx.steps.length > 0) {
    const last = recordingCtx.steps[recordingCtx.steps.length - 1];
    if (last.keycode !== 0x00 && last.keycode !== 0xFF) {
      if (recordingCtx.steps.length < MAX_MACRO_STEPS) {
        recordingCtx.steps.push({ modifier: 0, keycode: 0x00 });
      }
    }
  }
}

// ---------------------------------------------------------------------------
// UI options
// ---------------------------------------------------------------------------

const KEY_OPTIONS = [
  { label: 'F13', mod: 0, key: 0x68 }, { label: 'F14', mod: 0, key: 0x69 },
  { label: 'F15', mod: 0, key: 0x6A }, { label: 'F16', mod: 0, key: 0x6B },
  { label: 'F17', mod: 0, key: 0x6C }, { label: 'F18', mod: 0, key: 0x6D },
  { label: 'Ctrl+C', mod: 0x01, key: 0x06 }, { label: 'Ctrl+V', mod: 0x01, key: 0x19 },
  { label: 'Ctrl+Z', mod: 0x01, key: 0x1D }, { label: 'Ctrl+A', mod: 0x01, key: 0x04 },
  { label: 'Ctrl+X', mod: 0x01, key: 0x1B }, { label: 'Ctrl+S', mod: 0x01, key: 0x16 },
  { label: 'Ctrl+Shift+T', mod: 0x03, key: 0x17 },
  { label: 'Cmd+C (Mac)', mod: 0x08, key: 0x06 },
  { label: 'Cmd+V (Mac)', mod: 0x08, key: 0x19 },
  { label: 'Cmd+Tab (Mac App Switch)', mod: 0x08, key: 0x2B },
  { label: 'Alt+Tab (Win App Switch)', mod: 0x05, key: 0x2B },
  { label: 'Ctrl+Alt+Del', mod: 0x05, key: 0x4C },
  { label: 'Tab', mod: 0, key: 0x2B }, { label: 'Enter', mod: 0, key: 0x28 },
  { label: 'Escape', mod: 0, key: 0x29 }, { label: 'Space', mod: 0, key: 0x2C },
  { label: 'Backspace', mod: 0, key: 0x2A },
  { label: 'Delete', mod: 0, key: 0x4C }, { label: 'Insert', mod: 0, key: 0x49 },
  { label: 'Home', mod: 0, key: 0x4A }, { label: 'End', mod: 0, key: 0x4D },
  { label: 'Page Up', mod: 0, key: 0x4B }, { label: 'Page Down', mod: 0, key: 0x4E },
  { label: 'Up Arrow', mod: 0, key: 0x52 }, { label: 'Down Arrow', mod: 0, key: 0x51 },
  { label: 'Left Arrow', mod: 0, key: 0x50 }, { label: 'Right Arrow', mod: 0, key: 0x4F },
];

const CONSUMER_OPTIONS = [
  { label: 'Volume Up', code: 0x00E9 }, { label: 'Volume Down', code: 0x00EA },
  { label: 'Mute', code: 0x00E2 }, { label: 'Play/Pause', code: 0x00CD },
  { label: 'Next Track', code: 0x00B5 }, { label: 'Prev Track', code: 0x00B6 },
  { label: 'Stop', code: 0x00B7 }, { label: 'Eject', code: 0x00B8 },
  { label: 'Mail', code: 0x018C }, { label: 'Calculator', code: 0x0192 },
  { label: 'My Computer', code: 0x0194 },
];

const QUICK_MACRO_COMBOS = [
  { label: 'Ctrl+C', steps: [{ modifier: 0x01, keycode: 0x06 }, { modifier: 0, keycode: 0x00 }] },
  { label: 'Ctrl+V', steps: [{ modifier: 0x01, keycode: 0x19 }, { modifier: 0, keycode: 0x00 }] },
  { label: 'Ctrl+Z', steps: [{ modifier: 0x01, keycode: 0x1D }, { modifier: 0, keycode: 0x00 }] },
  { label: 'Ctrl+A', steps: [{ modifier: 0x01, keycode: 0x04 }, { modifier: 0, keycode: 0x00 }] },
  { label: 'Ctrl+X', steps: [{ modifier: 0x01, keycode: 0x1B }, { modifier: 0, keycode: 0x00 }] },
  { label: 'Ctrl+S', steps: [{ modifier: 0x01, keycode: 0x16 }, { modifier: 0, keycode: 0x00 }] },
  { label: 'Cmd+C (Mac)', steps: [{ modifier: 0x08, keycode: 0x06 }, { modifier: 0, keycode: 0x00 }] },
  { label: 'Cmd+V (Mac)', steps: [{ modifier: 0x08, keycode: 0x19 }, { modifier: 0, keycode: 0x00 }] },
  { label: 'Tab', steps: [{ modifier: 0, keycode: 0x2B }, { modifier: 0, keycode: 0x00 }] },
  { label: 'Enter', steps: [{ modifier: 0, keycode: 0x28 }, { modifier: 0, keycode: 0x00 }] },
  { label: 'Space', steps: [{ modifier: 0, keycode: 0x2C }, { modifier: 0, keycode: 0x00 }] },
  { label: 'Pause (200ms)', steps: [{ modifier: 0, keycode: 0xFF }] },
];

// ---------------------------------------------------------------------------
// Render grid
// ---------------------------------------------------------------------------

function renderGrid() {
  grid.innerHTML = '';
  currentMapping.forEach(function(action, idx) {
    const card = document.createElement('div');
    card.className = 'card';

    const typeSelect = document.createElement('select');
    var typeLabels = ['None', 'Key', 'Consumer / Media', 'Macro', 'Text', 'Paste (clipboard)', 'App Launcher'];
    var typeValues = [ACTION_NONE, ACTION_KEY, ACTION_CONSUMER, ACTION_MACRO, ACTION_TEXT, ACTION_PASTE, ACTION_LAUNCHER];
    for (var ti = 0; ti < typeLabels.length; ti++) {
      const opt = document.createElement('option');
      opt.value = typeValues[ti];
      opt.textContent = typeLabels[ti];
      if (typeValues[ti] === action.type) opt.selected = true;
      typeSelect.appendChild(opt);
    }

    const detailWrap = document.createElement('div');

    function renderDetail() {
      detailWrap.innerHTML = '';
      const type = parseInt(typeSelect.value, 10);

      if (type === ACTION_KEY) {
        const sel = document.createElement('select');
        KEY_OPTIONS.forEach(function(k) {
          const opt = document.createElement('option');
          opt.value = JSON.stringify({ mod: k.mod, key: k.key });
          opt.textContent = k.label;
          if (k.mod === action.modifier && k.key === action.keycode) opt.selected = true;
          sel.appendChild(opt);
        });
        detailWrap.appendChild(labeled('Key', sel));
        detailWrap._sel = sel;

      } else if (type === ACTION_CONSUMER) {
        const sel = document.createElement('select');
        CONSUMER_OPTIONS.forEach(function(c) {
          const opt = document.createElement('option');
          opt.value = c.code;
          opt.textContent = c.label;
          if (c.code === action.consumerCode) opt.selected = true;
          sel.appendChild(opt);
        });
        detailWrap.appendChild(labeled('Media action', sel));
        detailWrap._sel = sel;

      } else if (type === ACTION_TEXT) {
        const input = document.createElement('input');
        input.type = 'text';
        input.maxLength = TEXT_MAX_LEN - 1;
        input.value = action.text || '';
        detailWrap.appendChild(labeled('Text to type', input));
        detailWrap._sel = input;

      } else if (type === ACTION_PASTE) {
        const textarea = document.createElement('textarea');
        textarea.rows = 4;
        textarea.maxLength = TEXT_MAX_LEN - 1;
        textarea.value = action.text || '';
        textarea.placeholder = 'Text to paste (sent to clipboard on press)';
        detailWrap.appendChild(labeled('Clipboard text (up to 256 chars)', textarea));
        detailWrap._sel = textarea;
        const hint = document.createElement('div');
        hint.style.cssText = 'font-size:11px;color:var(--muted);margin-top:4px;';
        hint.textContent = 'Press the button \u2192 firmware notifies webconfig \u2192 text written to clipboard \u2192 sends Cmd/Ctrl+V.';
        detailWrap.appendChild(hint);

      } else if (type === ACTION_LAUNCHER) {
        const sel = document.createElement('select');
        var osOpts = [
          { label: 'macOS (Cmd+Space)', val: LAUNCHER_MAC },
          { label: 'Windows (Win key)', val: LAUNCHER_WINDOWS },
          { label: 'Linux (Super key)', val: LAUNCHER_LINUX }
        ];
        osOpts.forEach(function(o) {
          const opt = document.createElement('option');
          opt.value = o.val; opt.textContent = o.label;
          if (o.val === (action.launcherOs != null ? action.launcherOs : LAUNCHER_MAC)) opt.selected = true;
          sel.appendChild(opt);
        });
        if (action.launcherOs == null) {
          const ua = navigator.userAgent;
          if (ua.indexOf('Win') !== -1) sel.value = LAUNCHER_WINDOWS;
          else if (ua.indexOf('Linux') !== -1) sel.value = LAUNCHER_LINUX;
          else sel.value = LAUNCHER_MAC;
        }
        detailWrap.appendChild(labeled('Open search with', sel));
        detailWrap._osSel = sel;

        const appRow = document.createElement('div');
        appRow.style.cssText = 'display:flex;gap:6px;align-items:flex-end;';

        const appInput = document.createElement('input');
        appInput.type = 'text';
        appInput.maxLength = 255;
        appInput.value = action.text || '';
        appInput.placeholder = 'e.g. Chrome, Spotify, Terminal';
        appInput.style.flex = '1';
        detailWrap._appInput = appInput;

        const browseBtn = document.createElement('button');
        browseBtn.textContent = 'Browse apps';
        browseBtn.style.cssText = 'font-size:11px;padding:4px 8px;white-space:nowrap;';
        browseBtn.addEventListener('click', async function() {
          await showAppBrowser(appInput);
        });

        appRow.appendChild(labeled('App name to launch', appInput));
        appRow.appendChild(browseBtn);
        detailWrap.appendChild(appRow);

        const hint = document.createElement('div');
        hint.style.cssText = 'font-size:11px;color:var(--muted);margin-top:4px;';
        hint.textContent = 'Opens Spotlight/Start menu, types the app name, and presses Enter.';
        detailWrap.appendChild(hint);

      } else if (type === ACTION_MACRO) {
        const steps = action.steps || [];
        action.count = action.count || steps.length;
        const stepBadges = document.createElement('div');
        stepBadges.className = 'macro-steps';

        function renderBadges() {
          stepBadges.innerHTML = '';
          for (let i = 0; i < action.count; i++) {
            const step = steps[i];
            const badge = document.createElement('span');
            badge.className = 'badge';
            if (step.keycode === 0x00) {
              badge.textContent = '\u2191 Release';
              badge.style.background = '#3a3040';
            } else if (step.keycode === 0xFF) {
              badge.textContent = 'Pause';
              badge.style.background = '#3a3a20';
            } else {
              badge.textContent = hidKeyName(step.modifier, step.keycode);
            }
            const remove = document.createElement('span');
            remove.className = 'remove';
            remove.textContent = '\u00d7';
            (function(idx) {
              remove.addEventListener('click', function() {
                steps.splice(idx, 1);
                action.count = steps.length;
                renderBadges();
              });
            })(i);
            badge.appendChild(remove);
            stepBadges.appendChild(badge);
          }
        }

        renderBadges();
        detailWrap.appendChild(labeled('Steps (each combo + release marker)', stepBadges));
        detailWrap._steps = steps;
        detailWrap._action = action;

        const addRow = document.createElement('div');
        addRow.style.cssText = 'display:flex;gap:4px;margin-top:6px;flex-wrap:wrap;';

        const recordBtn = document.createElement('button');
        recordBtn.textContent = 'Record';
        recordBtn.className = 'record';
        recordBtn.style.cssText = 'font-size:11px;padding:4px 8px;';
        recordBtn.addEventListener('click', function() {
          if (recordingCtx) return;
          startMacroRecording(steps, function() {
            action.count = steps.length;
            renderBadges();
          });
          const updateInterval = setInterval(function() {
            if (!recordingCtx) { clearInterval(updateInterval); renderBadges(); return; }
            renderBadges();
          }, 200);
        });
        addRow.appendChild(recordBtn);

        const addBtn = document.createElement('button');
        addBtn.textContent = '+ Add step';
        addBtn.style.cssText = 'font-size:11px;padding:4px 8px;background:#2a2e3a;';
        addBtn.addEventListener('click', function() {
          if (steps.length >= MAX_MACRO_STEPS) return;
          steps.push({ modifier: 0, keycode: 0x04 });
          action.count = steps.length;
          renderBadges();
        });
        addRow.appendChild(addBtn);

        QUICK_MACRO_COMBOS.forEach(function(combo) {
          const btn = document.createElement('button');
          btn.textContent = '+ ' + combo.label;
          btn.style.cssText = 'font-size:11px;padding:4px 8px;background:#2a2e3a;';
          btn.addEventListener('click', function() {
            combo.steps.forEach(function(s) {
              if (steps.length < MAX_MACRO_STEPS) {
                steps.push({ modifier: s.modifier, keycode: s.keycode });
              }
            });
            action.count = steps.length;
            renderBadges();
          });
          addRow.appendChild(btn);
        });

        detailWrap.appendChild(addRow);
        const hint = document.createElement('div');
        hint.style.cssText = 'font-size:11px;color:var(--muted);margin-top:4px;';
        hint.textContent = 'Each step is a key+modifier held down. "Release" sends all held keys at once, then clears.';
        detailWrap.appendChild(hint);
      }
    }

    const labelInput = document.createElement('input');
    labelInput.type = 'text';
    labelInput.maxLength = 23;
    labelInput.value = action.label || ('Button ' + (idx + 1));

    typeSelect.addEventListener('change', renderDetail);
    renderDetail();

    const applyBtn = document.createElement('button');
    applyBtn.textContent = 'Apply live';
    applyBtn.addEventListener('click', function() {
      applyButton(idx, typeSelect, detailWrap, labelInput);
    });

    card.appendChild(el('h3', 'Button ' + (idx + 1)));
    card.appendChild(labeled('Label', labelInput));
    card.appendChild(labeled('Action type', typeSelect));
    card.appendChild(detailWrap);
    const row = document.createElement('div');
    row.className = 'apply-row';
    row.appendChild(applyBtn);
    card.appendChild(row);

    grid.appendChild(card);
  });
}

function el(tag, text) { const e = document.createElement(tag); e.textContent = text; return e; }
function labeled(text, control) {
  const wrap = document.createElement('div');
  const l = document.createElement('label');
  l.textContent = text;
  wrap.appendChild(l);
  wrap.appendChild(control);
  return wrap;
}

// ---------------------------------------------------------------------------
// Apply button via Tauri
// ---------------------------------------------------------------------------

async function applyButton(idx, typeSelect, detailWrap, labelInput) {
  const type = parseInt(typeSelect.value, 10);
  const action = { type: type, label: labelInput.value };

  if (type === ACTION_KEY && detailWrap._sel) {
    const parsed = JSON.parse(detailWrap._sel.value);
    action.modifier = parsed.mod;
    action.keycode = parsed.key;
  } else if (type === ACTION_CONSUMER && detailWrap._sel) {
    action.consumerCode = parseInt(detailWrap._sel.value, 10);
  } else if ((type === ACTION_TEXT || type === ACTION_PASTE) && detailWrap._sel) {
    action.text = detailWrap._sel.value;
  } else if (type === ACTION_LAUNCHER) {
    action.launcherOs = parseInt(detailWrap._osSel.value, 10);
    action.text = detailWrap._appInput.value;
  } else if (type === ACTION_MACRO && detailWrap._steps) {
    action.steps = detailWrap._steps;
    action.count = detailWrap._steps.length;
  }

  currentMapping[idx] = action;

  try {
    setStatus('Applying...', null);
    await invoke('set_button', { idx: idx, action: action });
    setStatus('Applied live (not yet saved to flash)', 'ok');
  } catch (e) {
    setStatus('Apply failed: ' + e, 'err');
  }
}

// ---------------------------------------------------------------------------
// Save to flash via Tauri
// ---------------------------------------------------------------------------

async function saveFlash() {
  try {
    setStatus('Saving to flash...', null);
    const ok = await invoke('save_flash');
    if (ok) {
      setStatus('Saved to flash', 'ok');
    } else {
      setStatus('Flash save reported failure', 'err');
    }
  } catch (e) {
    setStatus('Save failed: ' + e, 'err');
  }
}

// ---------------------------------------------------------------------------
// App browser (list_installed_apps via Tauri)
// ---------------------------------------------------------------------------

async function showAppBrowser(targetInput) {
  let apps;
  try {
    apps = await invoke('list_installed_apps');
  } catch (e) {
    setStatus('Failed to list apps: ' + e, 'err');
    return;
  }

  if (!apps || apps.length === 0) {
    setStatus('No installed apps found', 'err');
    return;
  }

  const overlay = document.createElement('div');
  overlay.className = 'recording-overlay';

  const box = document.createElement('div');
  box.className = 'box';
  box.style.minWidth = '400px';
  box.style.maxWidth = '500px';

  const h2 = document.createElement('h2');
  h2.textContent = 'Browse Installed Apps';
  box.appendChild(h2);

  const searchInput = document.createElement('input');
  searchInput.type = 'text';
  searchInput.placeholder = 'Search apps...';
  searchInput.style.width = '100%';
  searchInput.style.marginBottom = '8px';
  box.appendChild(searchInput);

  const listEl = document.createElement('div');
  listEl.style.cssText = 'max-height:300px;overflow-y:auto;border:1px solid #333;border-radius:4px;';
  box.appendChild(listEl);

  function renderList(filter) {
    listEl.innerHTML = '';
    const lf = (filter || '').toLowerCase();
    const filtered = lf ? apps.filter(function(a) { return a.toLowerCase().indexOf(lf) !== -1; }) : apps;
    filtered.forEach(function(appName) {
      const item = document.createElement('div');
      item.textContent = appName;
      item.style.cssText = 'padding:6px 10px;cursor:pointer;font-size:13px;border-bottom:1px solid #222;';
      item.addEventListener('mouseenter', function() { item.style.background = '#2a2e3a'; });
      item.addEventListener('mouseleave', function() { item.style.background = ''; });
      item.addEventListener('click', function() {
        targetInput.value = appName;
        overlay.remove();
      });
      listEl.appendChild(item);
    });
  }

  renderList('');
  searchInput.addEventListener('input', function() {
    renderList(searchInput.value);
  });

  const btnRow = document.createElement('div');
  btnRow.style.cssText = 'display:flex;gap:8px;margin-top:8px;';

  const closeBtn = document.createElement('button');
  closeBtn.textContent = 'Cancel';
  closeBtn.className = 'secondary';
  closeBtn.addEventListener('click', function() { overlay.remove(); });
  btnRow.appendChild(closeBtn);

  box.appendChild(btnRow);
  overlay.appendChild(box);
  document.body.appendChild(overlay);
  searchInput.focus();
}
